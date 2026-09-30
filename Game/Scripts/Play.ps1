#Requires -Version 7.0
<#
.SYNOPSIS
    Plays Veyra the way a player reaches a match: the launcher, sign-in, Play, Custom practice,
    champion select and the match.
.DESCRIPTION
    By default this starts the local services and opens the launcher (ADR-010):
    - it rebuilds the match-server image from the packaged Linux server, so the backend can start a
      match server once your champion select ends;
    - it starts the local backend and its database (compose.yaml), and leaves them running
      afterwards: a match server you leave still reports its result to the backend later;
    - it builds the launcher (Launcher/, cargo; quick when nothing changed) and opens its window.

    In the launcher, choose a development account and press Play. The launcher starts the packaged
    client, hands it a launch code and closes; the game signs in. A new account chooses its starter
    Vanguard first. Then Play, Custom, Practice: a short champion select, then the match on the
    battleground, with practice bots near its middle to try your kit on. Esc opens the menu, where End Custom
    Match leaves it; the results follow, and Continue returns to the shell. The script waits until
    the game closes. The game's log goes to Game/Saved/Play/<timestamp>.

    It needs the packaged client and server (Package.ps1 -Target VeyraClient -Platform Win64 and
    -Target VeyraServer -Platform Linux), Docker running, and Rust from rustup to build the launcher.

    -ResetOnboarding <account> makes that development account new again, so it chooses a starter.

    -Opponent plays the matchmade path instead of practice: a headless scripted opponent signs in
    as the second development account (Backend/config/local.json, devLogin.accounts) through the
    launcher's headless twin, and queues for the local 1v1 casual mode. Sign in as another account,
    choose Casual Select in Play, Ready, Find Match and Accept. The opponent locks a Vanguard after
    you and stands still in the match; Esc, End Match (Developer) ends it. After the results it
    queues again, until the game closes, and then this script closes it too.

    -Check proves the same path without a window: Smoke.ps1 -Flow Practice -Launcher Cli, where the
    launcher's headless twin signs in and a scripted client clicks through the flow. With -Opponent,
    Smoke.ps1 -Flow Casual -Launcher Cli, two scripted clients through a whole matchmade match.

    -Direct skips the launcher and the backend, to try kits quickly. It starts a local match server on
    the battleground (-Map Greybox: the one-lane grey box, where -Check runs unless told otherwise),
    waits until it is ready, and opens a game window that connects to it, playing
    -Vanguard. The server waits for just you, so the match starts after the 15-second preparation
    (Match.json) instead of the loading timeout. -Bots adds that many bot Vanguards when preparation
    begins; they join the smaller team, so the first is an enemy and the next an ally, and so on. They
    wander the lane and do not fight back: they are targets to try the kits on, and play the developer
    match's Vanguard (Match.json). The server is a local editor-build dedicated server by default;
    -Server Container runs the packaged Linux server in Docker instead. The window is the editor build
    running as a game by default; -Client Packaged runs the packaged client. Close the window to end:
    the script then stops the server. With -Check, a headless scripted client plays the chosen
    Vanguard's whole kit against a bot (Smoke.ps1's kit mode) and must pass.

    Exit codes: 0 played (or, with -Check, passed); 1 the check failed; 2 infrastructure error.
.PARAMETER Direct
    Skips the launcher and the backend: a local match server and a window that joins it at once.
.PARAMETER ResetOnboarding
    A development account to make new again before the launcher opens. Not with -Direct.
.PARAMETER Opponent
    Starts a scripted opponent for a matchmade 1v1 alongside the launcher. Not with -Direct.
.PARAMETER Vanguard
    With -Direct: the Vanguard to play, from Game/Tuning/Vanguards.json.
.PARAMETER Bots
    With -Direct: bot Vanguards to add, alternating enemy and ally.
.PARAMETER Server
    With -Direct: 'Editor' (default) or 'Container'.
.PARAMETER Client
    With -Direct: 'Editor' (default) or 'Packaged'.
.PARAMETER Map
    With -Direct: 'Battleground' (default; with -Check, 'Greybox') or 'Greybox'.
.PARAMETER Check
    Runs a headless scripted client instead of a window, and reports whether it passed.
.PARAMETER EngineRoot
    Engine folder to use instead of the one registered for the project's EngineAssociation.
.EXAMPLE
    ./Game/Scripts/Play.ps1
.EXAMPLE
    ./Game/Scripts/Play.ps1 -ResetOnboarding DevOne
.EXAMPLE
    ./Game/Scripts/Play.ps1 -Opponent
.EXAMPLE
    ./Game/Scripts/Play.ps1 -Direct -Vanguard bryn -Bots 3
#>
[CmdletBinding()]
param(
    [switch]$Direct,

    [string]$ResetOnboarding,

    [switch]$Opponent,

    [string]$Vanguard = 'cairn',

    [ValidateRange(0, 9)]
    [int]$Bots = 1,

    [ValidateSet('Editor', 'Container')]
    [string]$Server = 'Editor',

    [ValidateSet('Editor', 'Packaged')]
    [string]$Client = 'Editor',

    [ValidateSet('Battleground', 'Greybox')]
    [string]$Map,

    [switch]$Check,

    [string]$EngineRoot
)

$ErrorActionPreference = 'Stop'
Import-Module (Join-Path $PSScriptRoot 'VeyraProject.psm1') -Force

$ExitPlayed = 0
$ExitFailed = 1
$ExitInfrastructure = 2

# Harness settings, not gameplay: the game window's size.
$Window = @('-windowed', '-ResX=1600', '-ResY=900')

$projectFile = Get-VeyraProjectFile
$gameDir = Split-Path -Parent $projectFile
$repositoryDir = Split-Path -Parent $gameDir

$directOnly = @('Vanguard', 'Bots', 'Server', 'Client', 'Map') | Where-Object { $PSBoundParameters.ContainsKey($_) }
if (-not $Direct -and $directOnly) {
    Write-Host "Only -Direct takes -$($directOnly -join ', -'); the launcher's path chooses its Vanguard in champion select."
    exit $ExitInfrastructure
}
if ($Direct -and ($ResetOnboarding -or $Opponent)) {
    Write-Host '-ResetOnboarding and -Opponent apply to the launcher''s path, not with -Direct.'
    exit $ExitInfrastructure
}

$logDir = Join-Path $gameDir ('Saved\Play\' + (Get-Date -Format 'yyyyMMdd-HHmmss'))
New-Item -ItemType Directory -Force -Path $logDir | Out-Null
$clientLogPath = Join-Path $logDir 'Client.log'

# Runs docker compose and returns its exit code. Docker's live progress display fails in a terminal
# once its output is captured ("failed to get console"), so it prints plain progress; its lines are
# shown only when it fails.
function Invoke-Compose([string[]]$Arguments) {
    $said = @(& docker compose --progress plain --project-directory $repositoryDir @Arguments 2>&1 | ForEach-Object { "$_" })
    $code = $LASTEXITCODE
    if ($code -ne 0) {
        $said | Select-Object -Last 25 | ForEach-Object { Write-Host "  $_" }
    }
    return $code
}

function Write-MatchControls([string]$Indent = '  ') {
    Write-Host "${Indent}Right-click          move, or attack the enemy under the cursor"
    Write-Host "${Indent}A                    attack-move toward the cursor"
    Write-Host "${Indent}Q W E R              cast at the cursor"
    Write-Host "${Indent}Ctrl + Q/W/E/R       spend a skill point on that ability (R opens at level 6)"
    Write-Host "${Indent}~ (tilde)            console; `"Veyra.Dev.Help`" lists the developer commands"
}

if (-not $Direct) {
    # The launcher's path (ADR-010): the launcher, the backend, and the match server it starts.
    $launcherDir = Join-Path $repositoryDir 'Launcher'
    $launcherExecutable = Join-Path $launcherDir 'target\release\veyra-launcher.exe'
    $launcherConfigPath = Join-Path $launcherDir 'config\local.json'
    $launcherConfig = Get-Content -LiteralPath $launcherConfigPath -Raw | ConvertFrom-Json
    $buildManifest = [System.IO.Path]::GetFullPath((Join-Path (Split-Path -Parent $launcherConfigPath) $launcherConfig.game.buildManifest))
    if (-not (Test-Path -LiteralPath $buildManifest -PathType Leaf)) {
        Write-Host "No packaged client at '$buildManifest'. Run Build.ps1 -Target VeyraClient, then Package.ps1 -Target VeyraClient -Platform Win64."
        exit $ExitInfrastructure
    }

    $cargo = (Get-Command cargo -ErrorAction SilentlyContinue)?.Source
    if (-not $cargo) {
        $cargo = Join-Path $env:USERPROFILE '.cargo\bin\cargo.exe'
    }
    if (Test-Path -LiteralPath $cargo -PathType Leaf) {
        Write-Host 'Building the launcher.'
        $env:PATH = (Split-Path -Parent $cargo) + [System.IO.Path]::PathSeparator + $env:PATH
        & $cargo build --release --locked --manifest-path (Join-Path $launcherDir 'Cargo.toml') --package veyra-launcher --package veyra-launch-cli
        if ($LASTEXITCODE -ne 0) {
            Write-Host 'The launcher did not build.'
            exit $ExitInfrastructure
        }
    }
    elseif (-not (Test-Path -LiteralPath $launcherExecutable -PathType Leaf)) {
        Write-Host 'The launcher is not built and cargo was not found. Install Rust with rustup (stable, MSVC).'
        exit $ExitInfrastructure
    }

    # -Opponent: the second development account spars in the local 1v1 casual mode (ADR-010 §10).
    $backendConfig = Get-Content -LiteralPath (Join-Path $repositoryDir 'Backend\config\local.json') -Raw | ConvertFrom-Json
    $devAccounts = @($backendConfig.devLogin.accounts)
    if ($Opponent) {
        $casualMode = $backendConfig.modes | Where-Object { $_.enabled -and $_.matchmaking -eq 'casualSelect' } | Select-Object -First 1
        if (-not $casualMode -or $casualMode.humanPlayersPerTeam -ne 1 -or $devAccounts.Count -lt 2) {
            Write-Host '-Opponent needs an enabled casualSelect mode of one human player per team, and two development accounts, in Backend/config/local.json.'
            exit $ExitInfrastructure
        }
        $opponentAccount = $devAccounts[1]
    }

    if ($Check) {
        $checkFlow = $(if ($Opponent) { 'Casual' } else { 'Practice' })
        Write-Host "Checking the play flow without a window: Smoke.ps1 -Flow $checkFlow -Launcher Cli."
        $smokeArguments = @{ Flow = $checkFlow; Launcher = 'Cli' }
        if ($EngineRoot) {
            $smokeArguments.EngineRoot = $EngineRoot
        }
        & (Join-Path $PSScriptRoot 'Smoke.ps1') @smokeArguments
        exit $LASTEXITCODE
    }

    Write-Host 'Building the match server image from the packaged server.'
    if ((Invoke-Compose -Arguments @('--profile', 'match-server', 'build', 'match-server')) -ne 0) {
        Write-Host 'The match server image did not build. Check that Docker is running, and package the server: Build.ps1 -Target VeyraServer -Platform Linux, then Package.ps1 -Target VeyraServer -Platform Linux.'
        exit $ExitInfrastructure
    }
    Write-Host 'Starting the backend.'
    if ((Invoke-Compose -Arguments @('up', '--build', '--detach', '--wait', 'postgres', 'backend')) -ne 0) {
        Write-Host 'The backend did not start. Its log: docker compose logs backend'
        exit $ExitInfrastructure
    }

    if ($ResetOnboarding) {
        $uri = "$($launcherConfig.backend.baseUrl)/v1/dev/accounts/$([uri]::EscapeDataString($ResetOnboarding))/reset-onboarding"
        $null = Invoke-RestMethod -Method Post -Uri $uri -SkipHttpErrorCheck -StatusCodeVariable 'resetStatus' -TimeoutSec $launcherConfig.http.timeoutSeconds
        if ($resetStatus -ne 204) {
            Write-Host "The backend did not reset $ResetOnboarding's onboarding (HTTP $resetStatus). The accounts are in Backend/config/local.json, devLogin.accounts."
            exit $ExitInfrastructure
        }
        Write-Host "$ResetOnboarding is new again: it chooses a starter after signing in."
    }

    # This run's launcher configuration: the committed one, with the game in a window and its log here.
    $launcherConfig.game.buildManifest = $buildManifest
    $launcherConfig.game.arguments = @($launcherConfig.game.arguments) + $Window + "-ABSLOG=$clientLogPath"
    $runConfigPath = Join-Path $logDir 'Launcher.json'
    $launcherConfig | ConvertTo-Json -Depth 10 | Set-Content -LiteralPath $runConfigPath -Encoding utf8NoBOM

    $opponentProcess = $null
    if ($Opponent) {
        # The launcher's headless twin signs the opponent in and starts a headless game that plays it.
        $launchCli = Join-Path $launcherDir 'target\release\veyra-launch-cli.exe'
        if (-not (Test-Path -LiteralPath $launchCli -PathType Leaf)) {
            Write-Host 'The launcher''s headless twin is not built and cargo was not found. Install Rust with rustup (stable, MSVC).'
            exit $ExitInfrastructure
        }
        $opponentLogPath = Join-Path $logDir 'Opponent.log'
        Write-Host "Starting your opponent: $opponentAccount, headless."
        $said = @(& $launchCli '--config' $launcherConfigPath '--account' $opponentAccount '--' '-nullrhi' '-nosound' '-nosplash' '-unattended' "-ABSLOG=$opponentLogPath" '-VeyraSmokeFlow=opponent' 2>&1 | ForEach-Object { "$_" })
        $said | Set-Content -LiteralPath (Join-Path $logDir 'OpponentLauncher.log') -Encoding utf8NoBOM
        $signedIn = $said | Select-String -Pattern '^veyra-launch signed-in pid (\d+)$' | Select-Object -First 1
        if ($LASTEXITCODE -eq 0 -and $signedIn) {
            $opponentProcess = Get-Process -Id ([int]$signedIn.Matches[0].Groups[1].Value) -ErrorAction SilentlyContinue
        }
        if (-not $opponentProcess) {
            $said | Select-String -Pattern '^veyra-launch: ' | Select-Object -Last 1 | ForEach-Object { Write-Host "  $($_.Line)" }
            Write-Host "The opponent did not start. Its logs: $logDir"
            exit $ExitInfrastructure
        }
    }

    Write-Host ''
    Write-Host 'The launcher is opening.'
    if ($Opponent) {
        Write-Host "  1. Choose $($devAccounts[0]), not $opponentAccount (your opponent), and press Play. The launcher closes once the game has signed in."
        Write-Host '  2. A new account chooses its starter Vanguard, once.'
        Write-Host '  3. Play, then Casual Select; Ready, then Find Match. Your opponent is already queued: Accept the match found.'
        Write-Host '  4. Champion select: pick a Vanguard and Lock In. Your opponent locks a different one after you.'
        Write-Host '  5. In the match your opponent stands still as a target. Controls:'
        Write-MatchControls -Indent '       '
        Write-Host '       Esc                  the menu: Resume, or End Match (Developer) to end it for both'
        Write-Host '  6. The results follow; Continue returns to the shell, where you can queue again. Quit, or close the window, to finish.'
    }
    else {
        Write-Host '  1. Choose a development account and press Play. The launcher closes once the game has signed in.'
        Write-Host '  2. A new account chooses its starter Vanguard, once.'
        Write-Host '  3. Play, Custom, Practice; then pick a Vanguard and Lock In before the countdown ends.'
        Write-Host '  4. In the match: the practice bots play their lanes as Beginner AI: they last-hit, fight, retreat, recall and buy items.'
        Write-Host '     The panel at the bottom left says what your passive and each ability do. Controls:'
        Write-MatchControls -Indent '       '
        Write-Host '       Esc                  the menu: Resume, or End Custom Match to leave'
        Write-Host '  5. The results follow; Continue returns to the shell. Quit, or close the window, to finish.'
        Write-Host '  To play a matchmade 1v1 against a scripted opponent: Play.ps1 -Opponent.'
    }
    Write-Host '  To skip the launcher and try a kit at once: Play.ps1 -Direct -Vanguard <id> -Bots <n>.'
    Write-Host ''

    try {
        $launcherStarted = Get-Date
        $launcherProcess = Start-Process -FilePath $launcherExecutable -ArgumentList @('--config', "`"$runConfigPath`"") -PassThru
        $launcherProcess.WaitForExit()
        $game = Get-Process -Name 'VeyraClient' -ErrorAction SilentlyContinue |
            Where-Object { $_.StartTime -ge $launcherStarted -and (-not $opponentProcess -or $_.Id -ne $opponentProcess.Id) } | Select-Object -First 1
        if (-not $game) {
            Write-Host 'The launcher closed without starting the game.'
        }
        else {
            Write-Host 'The game is running. This script waits until it closes.'
            $game.WaitForExit()
            Write-Host "The game closed. Its log: $clientLogPath"
        }
    }
    finally {
        if ($opponentProcess -and -not $opponentProcess.HasExited) {
            # Still queued, its party is taken out of matchmaking when its next match found goes unanswered.
            $opponentProcess.Kill($true)
            Write-Host "Closed your opponent. Its log: $opponentLogPath"
        }
    }
    Write-Host 'The backend is still running, so a match you left can still report its result. To stop it: docker compose stop backend postgres'
    exit $ExitPlayed
}

# -Direct: a local match server and a window that joins it at once.

# Harness settings, not gameplay: where the server listens, the line it logs once it accepts
# players, how long to wait for that line, and how long a -Check may take.
$ServerPort = 7777
$ServerAddress = "127.0.0.1:$ServerPort"
$ServerReadyLine = 'Match server ready'
$ServerReadyTimeoutSeconds = 180
$CheckTimeoutMinutes = 5
# The kit check was built on the grey box's single lane, so it keeps it unless told otherwise.
if (-not $Map) { $Map = $(if ($Check) { 'Greybox' } else { 'Battleground' }) }
$MapUrl = $(if ($Map -eq 'Greybox') { '/Game/Veyra/Developer/Maps/L_Greybox' } else { '/Game/Veyra/World/Maps/L_Battleground' })

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

$serverLogPath = Join-Path $logDir 'Server.log'

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
        $env:VEYRA_MATCH_MAP = $MapUrl
        if ((Invoke-Compose -Arguments @('--profile', 'match-server', 'up', '--build', '--detach', 'match-server')) -ne 0) {
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
        Write-MatchControls
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
