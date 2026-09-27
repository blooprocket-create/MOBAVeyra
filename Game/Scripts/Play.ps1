#Requires -Version 7.0
<#
.SYNOPSIS
    Plays a local match with one command: a development server and a game window.
.DESCRIPTION
    A developer convenience until the real play flow (launcher, sign-in, Play, Custom practice,
    champion select) arrives in M6; see Docs/ADR/ADR-010. It starts a local match server on the
    grey-box map, waits until it is ready, and opens a game window that connects to it, playing the
    Vanguard you choose. The server waits for just you, so the match starts after the 15-second
    preparation (Match.json) instead of the loading timeout.

    -Bots adds that many bot Vanguards when preparation begins. They join the smaller team, so the
    first is an enemy and the next an ally, and so on. They wander the lane and do not fight back:
    they are targets to try the kits on. They play the developer match's Vanguard (Match.json).

    The server is a local editor-build dedicated server by default; -Server Container runs the
    packaged Linux server in Docker instead (Package.ps1 -Target VeyraServer -Platform Linux).
    The window is the editor build running as a game by default; -Client Packaged runs the packaged
    client (Package.ps1 -Target VeyraClient -Platform Win64).

    Close the game window to end: the script then stops the server. Logs go to
    Game/Saved/Play/<timestamp>.

    -Check proves the path without a window: a headless scripted client plays the chosen Vanguard's
    whole kit against a bot (Smoke.ps1's kit mode) and must pass.

    Exit codes: 0 played (or, with -Check, passed); 1 the check failed; 2 infrastructure error.
.PARAMETER Vanguard
    The Vanguard to play, from Game/Tuning/Vanguards.json.
.PARAMETER Bots
    Bot Vanguards to add, alternating enemy and ally.
.PARAMETER Server
    'Editor' (default) or 'Container'.
.PARAMETER Client
    'Editor' (default) or 'Packaged'.
.PARAMETER Check
    Runs a headless scripted client instead of a window, and reports whether it passed.
.PARAMETER EngineRoot
    Engine folder to use instead of the one registered for the project's EngineAssociation.
.EXAMPLE
    ./Game/Scripts/Play.ps1 -Vanguard oriel
.EXAMPLE
    ./Game/Scripts/Play.ps1 -Vanguard bryn -Bots 3
#>
[CmdletBinding()]
param(
    [string]$Vanguard = 'cairn',

    [ValidateRange(0, 9)]
    [int]$Bots = 1,

    [ValidateSet('Editor', 'Container')]
    [string]$Server = 'Editor',

    [ValidateSet('Editor', 'Packaged')]
    [string]$Client = 'Editor',

    [switch]$Check,

    [string]$EngineRoot
)

$ErrorActionPreference = 'Stop'
Import-Module (Join-Path $PSScriptRoot 'VeyraProject.psm1') -Force

$ExitPlayed = 0
$ExitFailed = 1
$ExitInfrastructure = 2

# Harness settings, not gameplay: where the server listens, the line it logs once it accepts
# players, how long to wait for that line, the window size, and how long a -Check may take.
$ServerPort = 7777
$ServerAddress = "127.0.0.1:$ServerPort"
$ServerReadyLine = 'Match server ready'
$ServerReadyTimeoutSeconds = 180
$Window = @('-windowed', '-ResX=1600', '-ResY=900')
$CheckTimeoutMinutes = 5
$MapUrl = '/Game/Veyra/Developer/Maps/L_Greybox'

$projectFile = Get-VeyraProjectFile
$gameDir = Split-Path -Parent $projectFile
$repositoryDir = Split-Path -Parent $gameDir
$engineRoot = Resolve-VeyraEngineRoot -ProjectFile $projectFile -EngineRoot $EngineRoot

$vanguards = (Get-Content -LiteralPath (Join-Path $gameDir 'Tuning\Vanguards.json') -Raw | ConvertFrom-Json).vanguards
if (-not $vanguards.PSObject.Properties[$Vanguard]) {
    Write-Host "Unknown Vanguard '$Vanguard'. Choose one of: $(($vanguards.PSObject.Properties.Name) -join ', ')."
    exit $ExitInfrastructure
}
if (@(Get-NetUDPEndpoint -LocalPort $ServerPort -ErrorAction SilentlyContinue).Count -gt 0) {
    Write-Host "Something already listens on UDP port $ServerPort, perhaps a match server left running. Stop it first."
    exit $ExitInfrastructure
}

$logDir = Join-Path $gameDir ('Saved\Play\' + (Get-Date -Format 'yyyyMMdd-HHmmss'))
New-Item -ItemType Directory -Force -Path $logDir | Out-Null
$serverLogPath = Join-Path $logDir 'Server.log'
$clientLogPath = Join-Path $logDir 'Client.log'

$editorExecutable = Join-Path $engineRoot 'Engine\Binaries\Win64\UnrealEditor.exe'
if ($Client -eq 'Editor') {
    $clientExecutable = $editorExecutable
    $clientPrefix = @("`"$projectFile`"", $ServerAddress, '-game')
}
else {
    $packageDir = Join-Path $gameDir 'Saved\Packages\VeyraClient-Win64'
    $clientExecutable = Get-ChildItem -LiteralPath $packageDir -Recurse -Filter 'VeyraClient.exe' -ErrorAction SilentlyContinue |
        Where-Object { $_.DirectoryName -like '*\Binaries\Win64' } | Select-Object -First 1 -ExpandProperty FullName
    $clientPrefix = @($ServerAddress)
}
if (-not $clientExecutable -or -not (Test-Path -LiteralPath $clientExecutable -PathType Leaf)) {
    Write-Host "The $Client client was not found. $(if ($Client -eq 'Packaged') { 'Run Package.ps1 -Target VeyraClient -Platform Win64 first.' } else { 'Build VeyraEditor first.' })"
    exit $ExitInfrastructure
}

# The server's map URL: wait for this one player, and add the bots when preparation begins.
$urlOptions = $(if ($Bots -gt 0) { "?VeyraLoadBots=$Bots" } else { '' })
$serverProcess = $null

function Get-ServerLog {
    if ($Server -eq 'Editor') {
        return $(if (Test-Path -LiteralPath $serverLogPath) { Get-Content -LiteralPath $serverLogPath } else { @() })
    }
    return (& docker compose --project-directory $repositoryDir --profile match-server logs --no-color match-server 2>&1)
}

function Stop-Server {
    if ($Server -eq 'Editor') {
        if ($serverProcess -and -not $serverProcess.HasExited) {
            $serverProcess.Kill($true)
            $serverProcess.WaitForExit()
        }
        return
    }
    Get-ServerLog | Out-File -LiteralPath $serverLogPath -Encoding utf8
    & docker compose --project-directory $repositoryDir --profile match-server rm --stop --force match-server 2>&1 | Out-Null
}

$clientProcess = $null
try {
    if ($Server -eq 'Editor') {
        Write-Host 'Starting a local match server (editor build).'
        $serverArguments = @("`"$projectFile`"", "${MapUrl}?VeyraExpectedPlayers=1$urlOptions", "-port=$ServerPort", '-server', '-nullrhi', '-unattended', '-nosplash',
            '-LogCmds="LogVeyraAbilities Verbose"', "-ABSLOG=`"$serverLogPath`"")
        $serverProcess = Start-Process -FilePath $editorExecutable -ArgumentList ($serverArguments -join ' ') -PassThru -WindowStyle Hidden
    }
    else {
        Write-Host 'Starting the match server container (packaged Linux server).'
        $env:VEYRA_EXPECTED_PLAYERS = '1'
        $env:VEYRA_MATCH_URL_OPTIONS = $urlOptions
        & docker compose --project-directory $repositoryDir --profile match-server up --build --detach match-server 2>&1 | Out-Null
        if ($LASTEXITCODE -ne 0) {
            Write-Host 'The match server container did not start. Package the Linux server first, and check that Docker is running.'
            exit $ExitInfrastructure
        }
    }

    $deadline = (Get-Date).AddSeconds($ServerReadyTimeoutSeconds)
    while (-not ((Get-ServerLog) -match $ServerReadyLine)) {
        if ((Get-Date) -gt $deadline -or ($serverProcess -and $serverProcess.HasExited)) {
            Write-Host "The server did not get ready. Its log: $serverLogPath"
            exit $ExitInfrastructure
        }
        Start-Sleep -Seconds 1
    }
    Write-Host 'The server is ready.'

    $clientArguments = $clientPrefix + @("-VeyraVanguard=$Vanguard", '-nosplash', "-ABSLOG=`"$clientLogPath`"")
    if ($Check) {
        $clientArguments += '-nullrhi', '-nosound', '-unattended', '-VeyraSmoke', '-VeyraSmokeKit'
        Write-Host "Checking: a headless client plays $Vanguard's kit against a bot."
    }
    else {
        $clientArguments += $Window
        Write-Host ''
        Write-Host "Opening the game as $Vanguard$(if ($Bots -gt 0) { " with $Bots bot(s)" }). The match goes live after the preparation countdown."
        Write-Host '  Right-click          move, or attack the enemy under the cursor'
        Write-Host '  A                    attack-move toward the cursor'
        Write-Host '  Q W E R              cast at the cursor'
        Write-Host '  Ctrl + Q/W/E/R       spend a skill point on that ability (R opens at level 6)'
        Write-Host '  ~ (tilde)            console; "Veyra.Dev.GrantLevels 5" reaches level 6'
        Write-Host '  Close the window     end the match; the server stops too'
        Write-Host ''
    }
    $clientProcess = Start-Process -FilePath $clientExecutable -ArgumentList ($clientArguments -join ' ') -PassThru
    $null = $clientProcess.Handle # Keeps the exit code readable after the process ends.

    if ($Check) {
        if (-not $clientProcess.WaitForExit([int][TimeSpan]::FromMinutes($CheckTimeoutMinutes).TotalMilliseconds)) {
            $clientProcess.Kill($true)
        }
        $verdict = if (Test-Path -LiteralPath $clientLogPath) { Select-String -LiteralPath $clientLogPath -Pattern 'VeyraSmoke: (PASS|FAIL).*' | Select-Object -Last 1 } else { $null }
        Write-Host ("Check: {0}" -f $(if ($verdict) { $verdict.Matches[0].Value } else { 'no verdict logged' }))
        Write-Host "Logs: $logDir"
        exit $(if ($verdict -and $verdict.Matches[0].Value -match 'PASS') { $ExitPlayed } else { $ExitFailed })
    }
    $clientProcess.WaitForExit()
    Write-Host "The game closed. Logs: $logDir"
    exit $ExitPlayed
}
finally {
    if ($clientProcess -and -not $clientProcess.HasExited) {
        $clientProcess.Kill($true)
    }
    Stop-Server
}
