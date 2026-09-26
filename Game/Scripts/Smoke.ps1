#Requires -Version 7.0
<#
.SYNOPSIS
    Plays a scripted match: two clients against the containerised Linux server.
.DESCRIPTION
    Starts the match-server service from the root compose.yaml (ADR-005 step 2), which builds its
    image from the packaged server (Package.ps1 -Target VeyraServer -Platform Linux). Then it starts
    two headless clients with -VeyraSmoke, which connect to 127.0.0.1:7777, wait for the match to go
    live, move their Vanguards and cast their Q ability at each other; the server must land both
    casts. The first client also pauses and resumes the match and checks that its own world stops
    (ADR-006 §8).

    Clients: 'Editor' runs the editor build as a game (UnrealEditor.exe -game); 'Packaged' runs
    the packaged client from Package.ps1 -Target VeyraClient -Platform Win64.

    Server: 'Container' is the done criterion's setup. 'Editor' runs the editor build as a local
    dedicated server instead, for when Docker is unavailable; it proves the same match flow, but
    not the Linux build or the container.

    Client logs, the server log and a summary go to Game/Saved/Smoke/<timestamp>. The server is
    stopped at the end unless -KeepServer is given.

    Exit codes: 0 passed; 1 a client or the server did not do its part; 2 infrastructure error.
.PARAMETER Clients
    Which client build to run.
.PARAMETER Server
    Which server to run: the container, or a local editor-build dedicated server.
.PARAMETER TimeoutMinutes
    Minutes to wait for the clients before stopping them.
.PARAMETER KeepServer
    Leaves the server container running afterwards.
.PARAMETER EngineRoot
    Engine folder to use instead of the one registered for the project's EngineAssociation.
.EXAMPLE
    ./Game/Scripts/Smoke.ps1 -Clients Editor
#>
[CmdletBinding()]
param(
    [ValidateSet('Editor', 'Packaged')]
    [string]$Clients = 'Packaged',

    [ValidateSet('Container', 'Editor')]
    [string]$Server = 'Container',

    [ValidateRange(1, 60)]
    [int]$TimeoutMinutes = 5,

    [switch]$KeepServer,

    [string]$EngineRoot
)

$ErrorActionPreference = 'Stop'
Import-Module (Join-Path $PSScriptRoot 'VeyraProject.psm1') -Force

$ExitPassed = 0
$ExitFailed = 1
$ExitInfrastructure = 2

# The server logs this line once it is loading the match and accepting players (AVeyraGameMode).
$ServerReadyLine = 'Match server ready'
$ServerReadyTimeoutSeconds = 120
$ServerAddress = '127.0.0.1:7777'
# The editor server's map and options; compose.yaml gives the container the same ones.
$EditorServerArguments = @('/Game/Veyra/Developer/Maps/L_Greybox?VeyraExpectedPlayers=2', '-port=7777', '-server', '-log', '-nullrhi', '-unattended', '-nosplash', '-LogCmds="LogVeyraAbilities Verbose"')

$projectFile = Get-VeyraProjectFile
$gameDir = Split-Path -Parent $projectFile
$repositoryDir = Split-Path -Parent $gameDir
$engineRoot = Resolve-VeyraEngineRoot -ProjectFile $projectFile -EngineRoot $EngineRoot

$reportDir = Join-Path $gameDir ('Saved\Smoke\' + (Get-Date -Format 'yyyyMMdd-HHmmss'))
New-Item -ItemType Directory -Force -Path $reportDir | Out-Null
Write-Host "Report folder: $reportDir"

if ($Clients -eq 'Editor') {
    $clientExecutable = Join-Path $engineRoot 'Engine\Binaries\Win64\UnrealEditor.exe'
    $clientPrefix = @("`"$projectFile`"", $ServerAddress, '-game')
}
else {
    # The game binary itself, not the launcher UAT places at the package root, so the script waits
    # on the process that plays.
    $packageDir = Join-Path $gameDir 'Saved\Packages\VeyraClient-Win64'
    $clientExecutable = Get-ChildItem -LiteralPath $packageDir -Recurse -Filter 'VeyraClient.exe' -ErrorAction SilentlyContinue |
        Where-Object { $_.DirectoryName -like '*\Binaries\Win64' } | Select-Object -First 1 -ExpandProperty FullName
    if (-not $clientExecutable) {
        $clientExecutable = Join-Path $packageDir '<not packaged>\VeyraClient.exe'
    }
    $clientPrefix = @($ServerAddress)
}
if (-not (Test-Path -LiteralPath $clientExecutable -PathType Leaf)) {
    Write-Host "The client was not found at '$clientExecutable'."
    exit $ExitInfrastructure
}

$serverLogPath = Join-Path $reportDir 'Server.log'
$serverProcess = $null

function Invoke-Compose {
    param([string[]]$Arguments)
    & docker compose --project-directory $repositoryDir --profile match-server @Arguments | Out-Host
    return $LASTEXITCODE
}

function Get-ServerLog {
    if ($Server -eq 'Editor') {
        return $(if (Test-Path -LiteralPath $serverLogPath) { Get-Content -LiteralPath $serverLogPath } else { @() })
    }
    return (& docker compose --project-directory $repositoryDir --profile match-server logs --no-color match-server 2>&1)
}

function Stop-Server {
    if ($Server -eq 'Editor') {
        # The editor server writes its own log file; stopping it ends the match.
        if (-not $KeepServer -and $serverProcess -and -not $serverProcess.HasExited) {
            $serverProcess.Kill($true)
            $serverProcess.WaitForExit()
        }
        return
    }
    Get-ServerLog | Out-File -LiteralPath $serverLogPath -Encoding utf8
    if (-not $KeepServer) {
        $null = Invoke-Compose -Arguments @('down')
    }
}

if ($Server -eq 'Editor') {
    Write-Host 'Starting a local editor-build match server.'
    $serverExecutable = Join-Path $engineRoot 'Engine\Binaries\Win64\UnrealEditor.exe'
    $serverArguments = @("`"$projectFile`"") + $EditorServerArguments + @("-ABSLOG=`"$serverLogPath`"")
    $serverProcess = Start-Process -FilePath $serverExecutable -ArgumentList ($serverArguments -join ' ') -PassThru
}
else {
    Write-Host 'Starting the match server container.'
    if ((Invoke-Compose -Arguments @('up', '--build', '--detach', 'match-server')) -ne 0) {
        Write-Host 'The match server container did not start.'
        exit $ExitInfrastructure
    }
}

$deadline = (Get-Date).AddSeconds($ServerReadyTimeoutSeconds)
while (-not ((Get-ServerLog) -match $ServerReadyLine)) {
    if ((Get-Date) -gt $deadline) {
        Write-Host "The server did not report '$ServerReadyLine' within $ServerReadyTimeoutSeconds seconds."
        Stop-Server
        exit $ExitInfrastructure
    }
    Start-Sleep -Seconds 1
}
Write-Host 'The server is ready.'

$clientProcesses = foreach ($index in 1, 2) {
    $clientArguments = $clientPrefix + @(
        '-VeyraSmoke'
        '-nullrhi'
        '-nosound'
        '-nosplash'
        '-unattended'
        "-ABSLOG=`"$(Join-Path $reportDir "Client$index.log")`""
    )
    if ($index -eq 1) {
        $clientArguments += '-VeyraSmokePause'
    }
    $process = Start-Process -FilePath $clientExecutable -ArgumentList ($clientArguments -join ' ') -PassThru
    $null = $process.Handle # Keeps the exit code readable after the process ends.
    $process
}

$failed = $false
foreach ($process in $clientProcesses) {
    if (-not $process.WaitForExit([TimeSpan]::FromMinutes($TimeoutMinutes))) {
        $process.Kill($true)
        $failed = $true
    }
}
Stop-Server

$index = 0
foreach ($process in $clientProcesses) {
    $index++
    $log = Join-Path $reportDir "Client$index.log"
    # The logged verdict is the client's result. Its exit code only catches a crash: on Windows the
    # engine exits 0 after any clean exit, whatever the smoke client asked for.
    $verdict = if (Test-Path -LiteralPath $log) { Select-String -LiteralPath $log -Pattern 'VeyraSmoke: (PASS|FAIL).*' | Select-Object -Last 1 } else { $null }
    Write-Host ("Client {0}: exit code {1}; {2}" -f $index, $(if ($process.HasExited) { $process.ExitCode } else { 'killed' }), $(if ($verdict) { $verdict.Matches[0].Value } else { 'no verdict logged' }))
    if (-not $process.HasExited -or $process.ExitCode -ne 0 -or -not $verdict -or $verdict.Matches[0].Value -notmatch 'PASS') {
        $failed = $true
    }
}

foreach ($expected in 'Preparation begins with 2 player(s)', 'The match is live', 'Match paused', 'Match resumed') {
    if (-not (Select-String -LiteralPath $serverLogPath -SimpleMatch $expected -Quiet)) {
        Write-Host "The server log never says '$expected'."
        $failed = $true
    }
}
# Each client's cast, as the server resolved it (VeyraAbilities logs it at Verbose).
$abilityQ = (Get-Content -LiteralPath (Join-Path $gameDir 'Tuning\Match.json') -Raw | ConvertFrom-Json).developerLoadout.abilityQ
$casts = @(Select-String -LiteralPath $serverLogPath -SimpleMatch " cast $abilityQ at ").Count
if ($casts -lt 2) {
    Write-Host "The server log shows $casts cast(s) of $abilityQ; expected one from each client."
    $failed = $true
}

if ($failed) {
    Write-Host "The smoke test failed. Logs: $reportDir"
    exit $ExitFailed
}
Write-Host "The smoke test passed. Logs: $reportDir"
exit $ExitPassed
