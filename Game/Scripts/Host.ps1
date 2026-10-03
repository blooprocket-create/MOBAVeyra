#Requires -Version 7.0
<#
.SYNOPSIS
    Hosts a public test of Veyra from this PC (ADR-057): players' launchers reach it at
    https://veyra.blooprocket.workers.dev.
.DESCRIPTION
    1. Finds this PC's public IPv4 address from Cloudflare's trace, unless -PublicHost names one.
    2. Writes Backend/config/hosted.json (ignored by git) from Backend/config/public.json, with that
       address as the match servers' publicHost: what players' games connect to.
    3. Starts Postgres, the backend with hosted.json, and the release server in Docker.
    4. Opens two Cloudflare quick tunnels with cloudflared, one to the backend and one to the release
       server, each with a temporary trycloudflare.com address.
    5. Records those addresses as the veyra Worker's secrets API_ORIGIN and RELEASES_ORIGIN with
       Wrangler, so the Worker forwards players to them.
    6. Checks the public address answers, then leaves the tunnels running in the background. Run it
       again with -Stop to close them; the Worker then tells players the servers are offline.

    Needs, once:
    - Docker Desktop running, and the match server image (Game/Scripts/Package.ps1 for the server,
      then docker compose --profile match-server build match-server);
    - cloudflared (winget install Cloudflare.cloudflared);
    - Node.js, and Wrangler signed in to the Cloudflare account (npx wrangler login);
    - a client published to the public channel (Game/Scripts/Publish.ps1 -Channel public);
    - for players outside this network, the router forwarding UDP 7780-7789 to this PC, and Windows
      letting those ports in. Those are the host's own changes; this script makes none.

    Hosting replaces the local development backend: stop it before running smoke tests.

    Exit codes: 0 hosting (or stopped); 1 a step failed; 2 something it needs is missing.
.PARAMETER PublicHost
    The address players' games connect to for matches, instead of the one Cloudflare sees.
.PARAMETER Stop
    Closes the tunnels and stops the backend.
.EXAMPLE
    ./Game/Scripts/Host.ps1
.EXAMPLE
    ./Game/Scripts/Host.ps1 -Stop
#>
[CmdletBinding()]
param(
    [string]$PublicHost,
    [switch]$Stop
)

$ErrorActionPreference = 'Stop'
$ExitPassed = 0
$ExitFailed = 1
$ExitInfrastructure = 2

$repo = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$hostingDir = Join-Path $repo 'Game\Saved\Hosting'
$stateFile = Join-Path $hostingDir 'tunnels.json'
$publicAddress = 'https://veyra.blooprocket.workers.dev'
$channel = 'public'
# The ports compose.yaml publishes on this PC for the backend and the release server.
$backendPort = 8080
$releasesPort = 8090
# How long a quick tunnel may take to report its address, and the public address to answer.
$tunnelWaitSeconds = 60
$answerWaitSeconds = 90

function Stop-Tunnels {
    if (Test-Path -LiteralPath $stateFile) {
        $state = Get-Content -LiteralPath $stateFile -Raw | ConvertFrom-Json
        foreach ($id in $state.processIds) {
            Stop-Process -Id $id -Force -ErrorAction SilentlyContinue
        }
        Remove-Item -LiteralPath $stateFile -Force
    }
}

if ($Stop) {
    Stop-Tunnels
    Push-Location $repo
    try { docker compose stop backend releases | Out-Null } finally { Pop-Location }
    Write-Host "Hosting stopped. $publicAddress now tells players the servers are offline."
    exit $ExitPassed
}

$cloudflared = (Get-Command cloudflared -ErrorAction SilentlyContinue)?.Source
if (-not $cloudflared) {
    $cloudflared = @(${env:ProgramFiles(x86)}, $env:ProgramFiles) | Where-Object { $_ } |
        ForEach-Object { Join-Path $_ 'cloudflared\cloudflared.exe' } | Where-Object { Test-Path -LiteralPath $_ } | Select-Object -First 1
}
if (-not $cloudflared) {
    Write-Host 'cloudflared was not found. Install it: winget install Cloudflare.cloudflared'
    exit $ExitInfrastructure
}
if (-not (Get-Command npx -ErrorAction SilentlyContinue)) {
    Write-Host 'Node.js (npx) was not found; Wrangler needs it.'
    exit $ExitInfrastructure
}
docker info --format '{{.ServerVersion}}' *> $null
if ($LASTEXITCODE -ne 0) {
    Write-Host 'Docker is not running. Start Docker Desktop.'
    exit $ExitInfrastructure
}
docker image inspect veyra-match-server:local *> $null
if ($LASTEXITCODE -ne 0) {
    Write-Host 'There is no match server image (veyra-match-server:local). Package the server, then: docker compose --profile match-server build match-server'
    exit $ExitInfrastructure
}
if (-not (Test-Path -LiteralPath (Join-Path $repo "Game\Saved\Releases\channels\$channel.json"))) {
    Write-Host "Nothing is published to the $channel channel yet: players could not install the game. Run Game/Scripts/Publish.ps1 -Channel $channel -NoServe."
    exit $ExitInfrastructure
}

# 1. The address players' games connect to for matches.
if (-not $PublicHost) {
    $trace = Invoke-RestMethod -Uri 'https://1.1.1.1/cdn-cgi/trace' -TimeoutSec 15
    $PublicHost = ([regex]::Match($trace, '(?m)^ip=(\d{1,3}(\.\d{1,3}){3})$')).Groups[1].Value
    if (-not $PublicHost) {
        Write-Host 'Could not find this PC''s public IPv4 address; pass -PublicHost.'
        exit $ExitFailed
    }
}
Write-Host "Players' games will connect to matches at $PublicHost, UDP 7780-7789."

# 2. The backend's configuration for this run.
$config = Get-Content -LiteralPath (Join-Path $repo 'Backend\config\public.json') -Raw | ConvertFrom-Json -AsHashtable
$config.allocator.docker.publicHost = $PublicHost
$config | ConvertTo-Json -Depth 32 | Set-Content -LiteralPath (Join-Path $repo 'Backend\config\hosted.json') -Encoding utf8NoBOM

# 3. The backend, its database and the release server.
Stop-Tunnels
Push-Location $repo
try {
    $env:VEYRA_BACKEND_CONFIG = 'hosted.json'
    docker compose up --detach --build postgres backend releases
    if ($LASTEXITCODE -ne 0) {
        Write-Host 'Docker could not start the backend.'
        exit $ExitFailed
    }
}
finally {
    Remove-Item Env:VEYRA_BACKEND_CONFIG -ErrorAction SilentlyContinue
    Pop-Location
}

# 4. Two quick tunnels, each reporting its temporary address in its log.
New-Item -ItemType Directory -Force -Path $hostingDir | Out-Null
function Open-Tunnel([string]$Name, [int]$Port) {
    $log = Join-Path $hostingDir "$Name-tunnel.log"
    Remove-Item -LiteralPath $log -Force -ErrorAction SilentlyContinue
    $process = Start-Process -FilePath $cloudflared -ArgumentList @('tunnel', '--no-autoupdate', '--url', "http://localhost:$Port", '--logfile', $log) `
        -WindowStyle Hidden -PassThru
    $deadline = (Get-Date).AddSeconds($tunnelWaitSeconds)
    while ((Get-Date) -lt $deadline) {
        if (Test-Path -LiteralPath $log) {
            $found = [regex]::Match((Get-Content -LiteralPath $log -Raw), 'https://[a-z0-9-]+\.trycloudflare\.com')
            if ($found.Success) {
                return [pscustomobject]@{ Url = $found.Value; ProcessId = $process.Id }
            }
        }
        Start-Sleep -Seconds 1
    }
    Stop-Process -Id $process.Id -Force -ErrorAction SilentlyContinue
    throw "The $Name tunnel reported no address within $tunnelWaitSeconds s; see $log."
}
try {
    $api = Open-Tunnel 'backend' $backendPort
    $files = Open-Tunnel 'releases' $releasesPort
}
catch {
    Write-Host $_.Exception.Message
    Stop-Tunnels
    exit $ExitFailed
}
@{ processIds = @($api.ProcessId, $files.ProcessId); backend = $api.Url; releases = $files.Url } |
    ConvertTo-Json | Set-Content -LiteralPath $stateFile -Encoding utf8NoBOM
Write-Host "Tunnels: backend $($api.Url), releases $($files.Url)."

# 5. The Worker forwards to them from now on.
Push-Location $repo
try {
    foreach ($secret in @(@('API_ORIGIN', $api.Url), @('RELEASES_ORIGIN', $files.Url))) {
        $secret[1] | npx --yes wrangler secret put $secret[0] *> (Join-Path $hostingDir "secret-$($secret[0]).log")
        if ($LASTEXITCODE -ne 0) {
            Write-Host "Wrangler could not set $($secret[0]); is it signed in (npx wrangler login)? See Game/Saved/Hosting."
            Stop-Tunnels
            exit $ExitFailed
        }
    }
}
finally {
    Pop-Location
}

# 6. The public address answers for the backend and the release store.
$deadline = (Get-Date).AddSeconds($answerWaitSeconds)
$healthy = $false
while (-not $healthy -and (Get-Date) -lt $deadline) {
    try {
        $health = Invoke-WebRequest -Uri "$publicAddress/healthz" -TimeoutSec 10 -SkipHttpErrorCheck
        $release = Invoke-WebRequest -Uri "$publicAddress/releases/channels/$channel.json" -TimeoutSec 10 -SkipHttpErrorCheck
        $healthy = $health.StatusCode -eq 200 -and $release.StatusCode -eq 200
    }
    catch {
        $healthy = $false
    }
    if (-not $healthy) {
        Start-Sleep -Seconds 3
    }
}
if (-not $healthy) {
    Write-Host "$publicAddress does not answer for the backend and the $channel channel yet; the tunnels stay open. Check again in a minute."
    exit $ExitFailed
}
Write-Host "Veyra is hosted at $publicAddress. Players install with the public Setup (Launcher/Package.ps1 -Config public)."
Write-Host 'Stop with: ./Game/Scripts/Host.ps1 -Stop'
exit $ExitPassed
