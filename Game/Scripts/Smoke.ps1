#Requires -Version 7.0
<#
.SYNOPSIS
    Plays a scripted match: two clients against the containerised Linux server.
.DESCRIPTION
    Starts the match-server service from the root compose.yaml (ADR-005 step 2), which builds its
    image from the packaged server (Package.ps1 -Target VeyraServer -Platform Linux). Then it starts
    two headless clients with -VeyraSmoke, which connect to 127.0.0.1:7777 as the developer test
    Vanguard (-VeyraVanguard=test_vanguard), wait for the match to go live, move their Vanguards and
    cast their Q ability at each other; the server must land both casts. The first client also pauses and resumes the match and checks that its own world stops
    (ADR-006 §8).

    Clients: 'Editor' runs the editor build as a game (UnrealEditor.exe -game); 'Packaged' runs
    the packaged client from Package.ps1 -Target VeyraClient -Platform Win64.

    Server: 'Container' is the done criterion's setup. 'Editor' runs the editor build as a local
    dedicated server instead, for when Docker is unavailable; it proves the same match flow, but
    not the Linux build or the container.

    -RecordReplay has the server record the match (ADR-006 §5 replay spike). The container is
    stopped gracefully so the replay is finished, the replay is copied out of it, and a client of
    the same kind plays it back with -VeyraReplayCheck, which must see both Vanguards, movement, a
    cast's damage and the pause. -NetStatsSeconds has the server log network statistics.
    -LoadTestBots and -LoadTestStandIns add bot participants and lane stand-ins when preparation
    begins (ADR-006 §5 bandwidth spike), and -ClientStaySeconds keeps the clients connected that
    long after their script, so the server can be measured in a steady state. -Screenshot renders the
    second client in a window and saves Greybox.png, a frame of the grey-box presentation taken
    once its cast has landed (ADR-008 §1), to the report folder.

    -Vanguards plays whole kits instead (ADR-008): one client per Vanguard named, joining teams in
    turn, so four names make a 2v2. Each client starts with -VeyraSmokeKit: it takes developer levels
    up to the first ultimate rank, ranks every ability, moves, casts Q, W, E and R at the nearest
    enemy and attacks it, and passes once an enemy has taken damage. The server must resolve every
    ability of every kit. There is no pause in this mode; with -RecordReplay, playback must show
    every Vanguard, a projectile and a delayed area.

    -Handoff -Practice plays a solo Custom practice match through the handoff instead (ADR-010 §7):
    one client, the practice match's host, moves and then ends it; the backend must record a
    host-ended result with no winner.

    -Handoff plays the match the way a player reaches one (ADR-007). It rebuilds the match-server
    image from the packaged server, starts the backend, signs in two dev accounts and asks the
    backend for a match between them. The backend starts the server container and writes its
    assignment to its standard input. Each packaged client starts with -VeyraLaunchCode=stdin and
    gets a fresh launch code through a pipe as soon as it waits for one. It redeems the code, finds
    its match waiting and presses Reconnect (-VeyraSmokeFlow=join), joins with its ticket and plays
    the script; the first client then ends the match once both casts have landed. The backend must
    record the result (a developer request, no winner, both participants joined and connected at the
    end) and remove the server. No log may hold a credential, except the join tickets the engine
    logs with each login on the server (ADR-007, known exposure). The backend is stopped afterwards
    only if this script started it.

    -Flow Practice plays the whole solo path through the client-state coordinator (ADR-010 §2)
    instead: the first dev account's onboarding is reset, and one packaged client signs in through
    the launch handshake and runs -VeyraSmokeFlow=practice, which chooses a starter, starts practice,
    hovers and locks a Vanguard, ends the match as its host, checks the verified result and returns
    to the shell, clicking the shell's and the in-match menu's buttons as a player would. The backend creates the match from the select and starts its server; the same
    checks as -Handoff -Practice follow. -Launcher Script means this script does the launcher's part:
    the dev login, the launch code and the handshake.

    -Flow Casual plays the matchmade path with two packaged clients, one per dev account, against
    the local 1v1 casual mode (ADR-010 §10). Each chooses the matchmade mode in Play, readies up,
    finds a match and accepts it; in the Casual Select they lock different Vanguards. The first
    client walks and ends the standard match from its menu's developer end; both check the verified
    result and return to the shell. The backend must record a developer-request end with no winner,
    both players joined and connected at the end, and remove the server.

    -Flow CasualDecline plays a match found that does not go ahead: both clients queue; the second
    declines once the first has accepted. The decliner must be back in the shell out of the queue;
    the first must be queued again in its place, then leaves the queue. No match is created.

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
    Leaves the server container running afterwards; with -Handoff, the backend.
.PARAMETER Handoff
    Plays the match through the backend's session handoff. Packaged clients and the container only.
.PARAMETER RecordReplay
    Records the match on the server and checks that a client can play it back. Container only.
.PARAMETER NetStatsSeconds
    When above 0, the server logs a VeyraNetStats line at this interval, in seconds.
.PARAMETER LoadTestBots
    Bot participants the server adds when preparation begins.
.PARAMETER LoadTestStandIns
    Lane stand-ins the server spawns when preparation begins.
.PARAMETER LoadTestStandInHz
    The stand-ins' network update rate; 0 keeps the engine's default for characters.
.PARAMETER ClientStaySeconds
    Seconds each client stays connected after its script before quitting.
.PARAMETER Screenshot
    Renders the second client and saves a screenshot of the grey-box presentation. Not with -Handoff.
    With -Flow, the client renders in a window and saves each screen it passes, as Flow-<screen>.png.
.PARAMETER Vanguards
    The Vanguards whose kits the clients play, one client each, from Vanguards.json. Not with -Handoff.
.PARAMETER Practice
    With -Handoff: a solo practice match that its host ends.
.PARAMETER Flow
    Plays a path through the client-state coordinator: Practice, the solo path; Casual, a matchmade
    1v1; CasualDecline, a declined match found. Packaged clients and container only.
.PARAMETER Launcher
    With -Flow: who plays the launcher's part. Script: this script. Cli: veyra-launch-cli, the launcher's
    headless twin (Launcher/), built in release, with Launcher/config/local.json.
.PARAMETER EngineRoot
    Engine folder to use instead of the one registered for the project's EngineAssociation.
.EXAMPLE
    ./Game/Scripts/Smoke.ps1 -Clients Editor
.EXAMPLE
    ./Game/Scripts/Smoke.ps1 -Vanguards cairn,qazharr,oriel,bryn -RecordReplay
.EXAMPLE
    ./Game/Scripts/Smoke.ps1 -Handoff
.EXAMPLE
    ./Game/Scripts/Smoke.ps1 -Flow Practice -Launcher Script
.EXAMPLE
    ./Game/Scripts/Smoke.ps1 -Flow Casual -Launcher Cli
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

    [switch]$Handoff,

    [switch]$RecordReplay,

    [ValidateRange(0, 600)]
    [int]$NetStatsSeconds = 0,

    [ValidateRange(0, 8)]
    [int]$LoadTestBots = 0,

    [ValidateRange(0, 1000)]
    [int]$LoadTestStandIns = 0,

    [ValidateRange(0, 200)]
    [int]$LoadTestStandInHz = 0,

    [ValidateRange(0, 3600)]
    [int]$ClientStaySeconds = 0,

    [switch]$Screenshot,

    [string[]]$Vanguards = @(),

    [switch]$Practice,

    [ValidateSet('Practice', 'Casual', 'CasualDecline')]
    [string]$Flow,

    [ValidateSet('Script', 'Cli')]
    [string]$Launcher = 'Script',

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
$ReplayName = 'veyra_smoke'
$ReplayContainerDir = '/srv/veyra/Veyra/Saved/Demos'
# The clients play the developer test Vanguard, whose Q is the targeted ability the script casts (ADR-008 §8).
$SmokeVanguard = 'test_vanguard'
# -Screenshot: the rendering client's window, and how long it stays after its script so the
# screenshot it asked for is drawn and saved.
$ScreenshotWindow = @('-windowed', '-ResX=1280', '-ResY=720')
$ScreenshotStaySeconds = 5
$ScreenshotName = 'Greybox.png'
$SlotKeys = 'q', 'w', 'e', 'r'

# -Vanguards: one client per Vanguard. 'a,b' passed through pwsh -File arrives as one string.
$Vanguards = @($Vanguards | ForEach-Object { $_ -split ',' } | ForEach-Object { $_.Trim() } | Where-Object { $_ })
$kitMode = $Vanguards.Count -gt 0
$clientCount = $(if ($kitMode) { $Vanguards.Count } else { 2 })

# Developer options for the server's map URL.
$urlOptions = ''
if ($RecordReplay) {
    $urlOptions += "?DemoRec=$ReplayName"
}
if ($NetStatsSeconds -gt 0) {
    $urlOptions += "?VeyraNetStats=$NetStatsSeconds"
}
if ($LoadTestBots -gt 0) {
    $urlOptions += "?VeyraLoadBots=$LoadTestBots"
}
if ($LoadTestStandIns -gt 0) {
    $urlOptions += "?VeyraLoadStandIns=$LoadTestStandIns"
}
if ($LoadTestStandInHz -gt 0) {
    $urlOptions += "?VeyraLoadStandInHz=$LoadTestStandInHz"
}

# The editor server's map and options; compose.yaml gives the container the same ones.
$EditorServerArguments = @("/Game/Veyra/Developer/Maps/L_Greybox?VeyraExpectedPlayers=$clientCount$urlOptions",'-port=7777', '-server', '-log', '-nullrhi', '-unattended', '-nosplash', '-LogCmds="LogVeyraAbilities Verbose"')

if ($RecordReplay -and $Server -ne 'Container') {
    # Only the container can be stopped gracefully, which the replay needs to be finished.
    Write-Host '-RecordReplay needs the container server.'
    exit $ExitInfrastructure
}
if ($Handoff -and ($Clients -ne 'Packaged' -or $Server -ne 'Container' -or $urlOptions -or $ClientStaySeconds -gt 0 -or $Screenshot)) {
    # The backend starts the server with its own arguments, so no map URL options reach it.
    Write-Host '-Handoff runs packaged clients against a container the backend starts, without the replay, statistics, load-test, stay or screenshot options.'
    exit $ExitInfrastructure
}
if ($Handoff -and $kitMode) {
    # The backend's dev match pairs two accounts; the handoff proves the join, not the kits.
    Write-Host '-Handoff plays the two-client script; run -Vanguards without it.'
    exit $ExitInfrastructure
}
if ($Practice -and -not $Handoff) {
    Write-Host '-Practice plays a practice match through the handoff; add -Handoff.'
    exit $ExitInfrastructure
}
if ($Flow -and ($Handoff -or $kitMode -or $Clients -ne 'Packaged' -or $Server -ne 'Container' -or $urlOptions -or $ClientStaySeconds -gt 0)) {
    # The backend creates the match and starts its server, as for -Handoff.
    Write-Host '-Flow runs packaged clients against a container the backend starts, without -Handoff, -Vanguards or the replay, statistics, load-test or stay options.'
    exit $ExitInfrastructure
}

$projectFile = Get-VeyraProjectFile
$gameDir = Split-Path -Parent $projectFile
$repositoryDir = Split-Path -Parent $gameDir

# -Launcher Cli: the launcher's headless twin and its configuration (Launcher/).
$launchCli = Join-Path $repositoryDir 'Launcher\target\release\veyra-launch-cli.exe'
$launcherConfig = Join-Path $repositoryDir 'Launcher\config\local.json'
if ($Launcher -eq 'Cli' -and -not $Flow) {
    Write-Host '-Launcher Cli plays the launcher''s part in a -Flow run; add -Flow.'
    exit $ExitInfrastructure
}
if ($Launcher -eq 'Cli' -and -not (Test-Path -LiteralPath $launchCli -PathType Leaf)) {
    Write-Host "The launcher CLI was not found at '$launchCli'. Build it: Launcher/Check.ps1, or cargo build --release in Launcher."
    exit $ExitInfrastructure
}
$engineRoot = Resolve-VeyraEngineRoot -ProjectFile $projectFile -EngineRoot $EngineRoot

# The Vanguard definitions: each client's, and the abilities the server must resolve.
$vanguardDefinitions = (Get-Content -LiteralPath (Join-Path $gameDir 'Tuning\Vanguards.json') -Raw | ConvertFrom-Json).vanguards
$maxPlayers = 2 * (Get-Content -LiteralPath (Join-Path $gameDir 'Tuning\Match.json') -Raw | ConvertFrom-Json).teams.maxTeamSize
if ($kitMode) {
    $unknown = @($Vanguards | Where-Object { -not $vanguardDefinitions.PSObject.Properties[$_] })
    if ($unknown.Count -gt 0 -or $clientCount -lt 2 -or $clientCount -gt $maxPlayers) {
        Write-Host "-Vanguards needs 2 to $maxPlayers Vanguards that Vanguards.json defines; unknown: $($unknown -join ', ')."
        exit $ExitInfrastructure
    }
}

$reportDir = Join-Path $gameDir ('Saved\Smoke\' + (Get-Date -Format 'yyyyMMdd-HHmmss'))
New-Item -ItemType Directory -Force -Path $reportDir | Out-Null
Write-Host "Report folder: $reportDir"

if ($Clients -eq 'Editor') {
    $clientExecutable = Join-Path $engineRoot 'Engine\Binaries\Win64\UnrealEditor.exe'
    $clientPrefix = @("`"$projectFile`"", $ServerAddress, '-game')
    $playbackPrefix = @("`"$projectFile`"", '-game')
    $clientDemosDir = Join-Path $gameDir 'Saved\Demos'
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
    $playbackPrefix = @()
    # <package>\WindowsClient\Veyra\Binaries\Win64\VeyraClient.exe saves under <package>\WindowsClient\Veyra\Saved.
    $clientDemosDir = Join-Path (Split-Path -Parent (Split-Path -Parent (Split-Path -Parent $clientExecutable))) 'Saved\Demos'
}
if (-not (Test-Path -LiteralPath $clientExecutable -PathType Leaf)) {
    Write-Host "The client was not found at '$clientExecutable'."
    exit $ExitInfrastructure
}

$serverLogPath = Join-Path $reportDir 'Server.log'
$serverProcess = $null

function Invoke-Compose {
    param([string[]]$Arguments)
    # Plain progress: Docker's live display fails in a terminal once its output is piped.
    & docker compose --progress plain --project-directory $repositoryDir --profile match-server @Arguments | Out-Host
    return $LASTEXITCODE
}

if ($Handoff -or $Flow) {
    # The session handoff (ADR-007): the backend creates the match and starts its server; each client
    # redeems a launch code from its standard input and joins with its ticket. With -Flow the client's
    # own champion select creates the match (ADR-010 §8).

    # Harness settings, not gameplay: how long to wait for each stage.
    $LaunchCodeWaitSeconds = 60
    $MatchEndWaitSeconds = 60
    $ServerLogWaitSeconds = 30
    $RemovalMarginSeconds = 15
    $BackendRequestTimeoutSeconds = 10

    function ConvertFrom-GoDuration([string]$Text) {
        $seconds = 0.0
        foreach ($part in [regex]::Matches($Text, '(\d+(?:\.\d+)?)(h|ms|m|s)')) {
            $value = [double]$part.Groups[1].Value
            $seconds += switch ($part.Groups[2].Value) { 'h' { $value * 3600 } 'm' { $value * 60 } 's' { $value } 'ms' { $value / 1000 } }
        }
        return $seconds
    }

    # Calls the backend and returns the status and parsed body. A credential goes only in the header,
    # and nothing here prints a body, since bodies carry credentials.
    function Invoke-Backend {
        param([string]$Method, [string]$Path, [object]$Body = $null, [string]$Credential = '')
        $request = @{ Method = $Method; Uri = "$backendUrl$Path"; SkipHttpErrorCheck = $true; StatusCodeVariable = 'responseStatus'; TimeoutSec = $BackendRequestTimeoutSeconds }
        if ($Credential) {
            $request.Headers = @{ Authorization = "Bearer $Credential" }
        }
        if ($null -ne $Body) {
            $request.Body = $Body | ConvertTo-Json -Depth 5 -Compress
            $request.ContentType = 'application/json'
        }
        try {
            $content = Invoke-RestMethod @request
        }
        catch {
            return [pscustomobject]@{ Status = 0; Body = $null }
        }
        return [pscustomobject]@{ Status = [int]$responseStatus; Body = $content }
    }

    function Get-ErrorCode($Response) {
        if ($Response.Body -and $Response.Body.PSObject.Properties['error']) { return $Response.Body.error }
        return $(if ($Response.Status -eq 0) { 'no answer' } else { 'no error code' })
    }

    # The backend as the clients reach it, the build they report, and the backend's own settings.
    $gameConfig = Get-Content -LiteralPath (Join-Path $gameDir 'Config\DefaultGame.ini')
    $backendUrl = ($gameConfig | Select-String -Pattern '^BackendBaseUrl="?([^"]+)"?$' | Select-Object -First 1).Matches.Groups[1].Value
    $buildVersion = ($gameConfig | Select-String -Pattern '^ProjectVersion=(\S+)$' | Select-Object -First 1).Matches.Groups[1].Value
    $backendConfig = Get-Content -LiteralPath (Join-Path $repositoryDir 'Backend\config\local.json') -Raw | ConvertFrom-Json
    if (-not $backendUrl -or -not $buildVersion) {
        Write-Host 'Config/DefaultGame.ini gives no BackendBaseUrl or ProjectVersion.'
        exit $ExitInfrastructure
    }
    # A standard match pairs two accounts on opposite sides; a practice match is its host alone, on
    # the host's side. Standard players play the developer test Vanguard, whose Q the script casts;
    # the host plays a released Vanguard, as a player would, and with -Flow chooses it as the starter.
    $PracticeVanguard = 'cairn'
    # -Flow Casual: picks are unique in a matchmade select, so each player locks its own.
    $CasualVanguards = @('cairn', 'oriel')
    $isPractice = $Practice -or $Flow -eq 'Practice'
    $isMatchmade = $Flow -in 'Casual', 'CasualDecline'
    # Every run but a declined match found plays a match.
    $expectsMatch = $Flow -ne 'CasualDecline'
    $playerCount = $(if ($isPractice) { 1 } else { 2 })
    $mode = $(if ($isPractice) { $backendConfig.customPractice.mode } else { ($backendConfig.modes | Where-Object { $_.enabled } | Select-Object -First 1).id })
    if ($isMatchmade) {
        # Two players make one match only with the local 1v1 team size (ADR-010, provisional).
        $casualMode = $backendConfig.modes | Where-Object { $_.enabled -and $_.matchmaking -eq 'casualSelect' } | Select-Object -First 1
        if (-not $casualMode -or $casualMode.humanPlayersPerTeam -ne 1) {
            Write-Host "-Flow $Flow needs an enabled casualSelect mode of one human player per team in Backend/config/local.json."
            exit $ExitInfrastructure
        }
        $mode = $casualMode.id
    }
    $accounts = @($backendConfig.devLogin.accounts | Select-Object -First $playerCount)
    # -Flow: the client logs the match its select created as it joins it.
    $JoiningLinePattern = 'VeyraClientFlow: joining match ([0-9a-f-]{36}) at '
    $MatchIdWaitPollMilliseconds = 250

    Write-Host 'Building the match server image from the packaged server.'
    if ((Invoke-Compose -Arguments @('build', 'match-server')) -ne 0) {
        Write-Host 'The match server image did not build. Package the server first.'
        exit $ExitInfrastructure
    }
    $backendWasRunning = @(& docker compose --project-directory $repositoryDir ps --status running --services 2>$null) -contains 'backend'
    Write-Host 'Starting the backend.'
    if ((Invoke-Compose -Arguments @('up', '--build', '--detach', '--wait', 'postgres', 'backend')) -ne 0) {
        Write-Host 'The backend did not start.'
        exit $ExitInfrastructure
    }

    $participants = foreach ($index in 0..($playerCount - 1)) {
        $login = Invoke-Backend -Method Post -Path '/v1/dev/login' -Body @{ accountName = $accounts[$index] }
        if ($login.Status -ne 200) {
            Write-Host "Dev login as $($accounts[$index]) failed: HTTP $($login.Status) ($(Get-ErrorCode $login))."
            exit $ExitInfrastructure
        }
        [pscustomobject]@{ Name = $accounts[$index]; AccountId = $login.Body.account.id; LauncherSession = $login.Body.token
            Side = $(if ($isPractice) { $backendConfig.customPractice.hostSide } else { @('A', 'B')[$index] })
            Vanguard = $(if ($isPractice) { $PracticeVanguard } elseif ($isMatchmade) { $CasualVanguards[$index] } else { $SmokeVanguard }) }
    }
    if ($isMatchmade -and @($participants).Count -lt 2) {
        Write-Host "-Flow $Flow needs two dev accounts in Backend/config/local.json devLogin.accounts."
        exit $ExitInfrastructure
    }

    $matchId = $null
    $container = $null
    $serverErrorLogPath = Join-Path $reportDir 'Server.stderr.log'
    # The server's log, for as long as its container runs; docker replays it from the start.
    function Start-ServerLog {
        Start-Process -FilePath 'docker' -ArgumentList @('logs', '--follow', $container) -NoNewWindow -PassThru `
            -RedirectStandardOutput $serverLogPath -RedirectStandardError $serverErrorLogPath
    }

    if ($Flow -eq 'Practice') {
        # The player starts as new: the flow chooses a starter first (ADR-010 §6).
        $reset = Invoke-Backend -Method Post -Path "/v1/dev/accounts/$($participants[0].Name)/reset-onboarding"
        if ($reset.Status -ne 204) {
            Write-Host "The backend did not reset $($participants[0].Name)'s onboarding: HTTP $($reset.Status) ($(Get-ErrorCode $reset))."
            exit $ExitInfrastructure
        }
    }
    elseif ($Flow) {
        # The matchmade flows keep the accounts' onboarding; a new player chooses a starter on the way.
        # Matchmaking and the clients' champion select create any match.
        Write-Host "Queueing $($participants.Name -join ' and ') for $mode."
    }
    else {
        $matchRequest = @{
            mode         = $mode
            rules        = $(if ($isPractice) { 'practice' } else { 'standard' })
            participants = @($participants | ForEach-Object { @{ accountId = $_.AccountId; side = $_.Side; vanguardId = $_.Vanguard } })
        }
        if ($isPractice) {
            $matchRequest.hostAccountId = $participants[0].AccountId
        }
        $created = Invoke-Backend -Method Post -Path '/v1/dev/matches' -Body $matchRequest
        if ($created.Status -ne 201) {
            $code = Get-ErrorCode $created
            Write-Host "The backend did not create the match: HTTP $($created.Status) ($code)."
            if ($code -eq 'already_in_match') {
                Write-Host 'An account is still in a match from an earlier run. Its server ends an empty match after Match.json lifecycle.abandonAfterSeconds; try again then.'
            }
            exit $ExitInfrastructure
        }
        $matchId = $created.Body.match.id
        $container = $backendConfig.allocator.docker.containerNamePrefix + $matchId
        Write-Host "Match ${matchId}: server container $container, port $($created.Body.match.hostPort)."
    }

    $handoffClients = @()
    $serverLogProcess = $null
    $failed = $false
    try {
        if ($container) {
            $serverLogProcess = Start-ServerLog
        }

        # Each client reads its launch code from a pipe, and plays the Vanguard the roster names. In a
        # standard match the first plays the pause and ends the match, and the second waits for the end;
        # a practice match's host ends it. A match created before the client started waits behind
        # Reconnect, which -VeyraSmokeFlow=join presses; -VeyraSmokeFlow=practice plays the whole flow.
        # None uses -log, which on Windows can replace the standard handles. Not $clients: PowerShell
        # names ignore case, and that is the -Clients parameter.
        $handoffClients = foreach ($index in 0..($playerCount - 1)) {
            $log = Join-Path $reportDir "Client$($index + 1).log"
            # The script quotes paths for a command line of its own; the launcher CLI passes each
            # argument as it is.
            $quote = $(if ($Launcher -eq 'Cli') { '' } else { '"' })
            # With -Flow -Screenshot the first client renders in a window and saves each screen it passes.
            $clientArguments = @('-nosound', '-nosplash', '-unattended', "-ABSLOG=$quote$log$quote")
            $clientArguments += $(if ($Flow -and $Screenshot -and $index -eq 0) { $ScreenshotWindow + "-VeyraSmokeFlowScreenshots=$quote$reportDir$quote" } else { @('-nullrhi') })
            $clientArguments += $(if ($Flow -eq 'Practice') { @('-VeyraSmokeFlow=practice', "-VeyraSmokeFlowVanguard=$PracticeVanguard") }
                elseif ($Flow -eq 'Casual') { @('-VeyraSmokeFlow=casual', "-VeyraSmokeFlowVanguard=$($participants[$index].Vanguard)") + $(if ($index -eq 0) { @('-VeyraSmokeFlowEndsMatch') } else { @() }) }
                elseif ($Flow -eq 'CasualDecline') { @($(if ($index -eq 0) { '-VeyraSmokeFlow=requeue' } else { '-VeyraSmokeFlow=decline' })) }
                elseif ($isPractice) { @('-VeyraSmokeFlow=join', '-VeyraSmoke', '-VeyraSmokeEndCustomMatch') }
                elseif ($index -eq 0) { @('-VeyraSmokeFlow=join', '-VeyraSmoke', '-VeyraSmokePause', '-VeyraSmokeEndMatch') }
                else { @('-VeyraSmokeFlow=join', '-VeyraSmoke', '-VeyraSmokeWaitForEnd') })
            if ($Launcher -eq 'Cli') {
                # The launcher's headless twin does the launcher's part (ADR-010 §5): it signs in, starts
                # the game its configuration names (the package's VeyraBuild.json) with the launch-code
                # switch, hands it a code and exits once it signed in. The game runs on.
                $launcherLog = Join-Path $reportDir "Launcher$($index + 1).log"
                $said = @(& $launchCli '--config' $launcherConfig '--account' $participants[$index].Name '--' @clientArguments 2>&1 | ForEach-Object { "$_" })
                $said | Set-Content -LiteralPath $launcherLog -Encoding utf8NoBOM
                $signedIn = $said | Select-String -Pattern '^veyra-launch signed-in pid (\d+)$' | Select-Object -First 1
                $process = $null
                if ($LASTEXITCODE -eq 0 -and $signedIn) {
                    $process = Get-Process -Id ([int]$signedIn.Matches[0].Groups[1].Value) -ErrorAction SilentlyContinue
                }
                if (-not $process) {
                    $said | Select-String -Pattern '^veyra-launch: ' | Select-Object -Last 1 | ForEach-Object { Write-Host "Launcher CLI, client $($index + 1): $($_.Line)" }
                }
                [pscustomobject]@{ Number = $index + 1; Participant = $participants[$index]; Log = $log; Process = $process
                    Handshake = [pscustomobject]@{ State = $(if ($process) { 'SignedIn' } else { 'Failed' }); Failure = 'the launcher CLI did not launch it' } }
                continue
            }
            $handshake = Start-VeyraHandshakeClient -Executable $clientExecutable -Arguments (@('-VeyraLaunchCode=stdin') + $clientArguments)
            [pscustomobject]@{ Number = $index + 1; Participant = $participants[$index]; Log = $log; Process = $handshake.Process; Handshake = $handshake }
        }

        # The launch handshake (ADR-010 §5): a launch code lives only seconds, so each is issued when its
        # client says it is ready to read one, and the client then says whether it signed in.
        $deadline = (Get-Date).AddSeconds($LaunchCodeWaitSeconds)
        do {
            if ($Launcher -eq 'Cli') { break }
            $unsettled = 0
            foreach ($client in $handoffClients) {
                # The block runs here, in this script's scope, while $client is this client.
                $state = Step-VeyraHandshake -Client $client.Handshake -IssueCode {
                    $issued = Invoke-Backend -Method Post -Path '/v1/launch-codes' -Body @{ buildVersion = $buildVersion } -Credential $client.Participant.LauncherSession
                    if ($issued.Status -ne 200) { throw "HTTP $($issued.Status) ($(Get-ErrorCode $issued))" }
                    $issued.Body.token
                }
                if ($state -in 'Starting', 'AwaitingSignIn') { $unsettled++ }
            }
            if ($unsettled -gt 0) { Start-Sleep -Milliseconds 100 }
        } while ($unsettled -gt 0 -and (Get-Date) -lt $deadline)
        foreach ($client in $handoffClients) {
            switch ($client.Handshake.State) {
                'SignedIn' { }
                'Failed' { Write-Host "Client $($client.Number) did not sign in: $($client.Handshake.Failure)."; $failed = $true }
                default { Write-Host "Client $($client.Number) did not finish the launch handshake within $LaunchCodeWaitSeconds s ($($client.Handshake.State))."; $failed = $true }
            }
        }
        $participants | ForEach-Object { $_.LauncherSession = $null }

        # -Flow: the match exists once the client's select starts it; its server's log is followed
        # from then.
        $clientDeadline = (Get-Date).AddMinutes($TimeoutMinutes)
        while ($Flow -and $expectsMatch -and -not $matchId -and $handoffClients[0].Process -and -not $handoffClients[0].Process.HasExited -and (Get-Date) -lt $clientDeadline) {
            Start-Sleep -Milliseconds $MatchIdWaitPollMilliseconds
            $joining = if (Test-Path -LiteralPath $handoffClients[0].Log) { Select-String -LiteralPath $handoffClients[0].Log -Pattern $JoiningLinePattern | Select-Object -First 1 } else { $null }
            if ($joining) {
                $matchId = $joining.Matches[0].Groups[1].Value
                $container = $backendConfig.allocator.docker.containerNamePrefix + $matchId
                Write-Host "Match ${matchId}, created by the client's champion select: server container $container."
                $serverLogProcess = Start-ServerLog
            }
        }
        foreach ($client in @($handoffClients | Where-Object Process)) {
            if (-not $client.Process.WaitForExit([int][Math]::Max(0, ($clientDeadline - (Get-Date)).TotalMilliseconds))) {
                $client.Process.Kill($true)
                $failed = $true
            }
        }
        if ($expectsMatch -and -not $matchId) {
            Write-Host 'The client never joined a match.'
            $failed = $true
        }
        $writtenLogs = @($handoffClients.Log | Where-Object { Test-Path -LiteralPath $_ })
        if (-not $expectsMatch -and $writtenLogs.Count -gt 0 -and (Select-String -LiteralPath $writtenLogs -Pattern $JoiningLinePattern -Quiet)) {
            Write-Host 'A client joined a match, but the match found should not have gone ahead.'
            $failed = $true
        }

        # The server reports the result and quits; the backend then ends the match and, later, removes
        # the container.
        function Get-DevMatch {
            if (-not $matchId) { return $null }
            $answer = Invoke-Backend -Method Get -Path "/v1/dev/matches/$matchId"
            return $(if ($answer.Status -eq 200) { $answer.Body.match } else { $null })
        }
        $deadline = (Get-Date).AddSeconds($MatchEndWaitSeconds)
        do {
            $match = Get-DevMatch
            if (-not $matchId -or ($match -and $match.state -in 'ended', 'failed')) { break }
            Start-Sleep -Seconds 1
        } while ((Get-Date) -lt $deadline)
        if ($serverLogProcess -and -not $serverLogProcess.WaitForExit($ServerLogWaitSeconds * 1000)) {
            $serverLogProcess.Kill($true)
        }
        if ($match -and $match.state -in 'ended', 'failed') {
            $removalSeconds = (ConvertFrom-GoDuration $backendConfig.matches.removeServerAfter) + 2 * (ConvertFrom-GoDuration $backendConfig.matches.reapInterval) + $RemovalMarginSeconds
            Write-Host "The match is $($match.state); waiting up to $removalSeconds s for the backend to remove its server."
            $deadline = (Get-Date).AddSeconds($removalSeconds)
            while (-not $match.serverRemoved -and (Get-Date) -lt $deadline) {
                Start-Sleep -Seconds 2
                $match = Get-DevMatch
            }
        }
        # The backend's development view of the match, which holds no credential.
        $match | ConvertTo-Json -Depth 6 | Out-File -LiteralPath (Join-Path $reportDir 'BackendMatch.json') -Encoding utf8

        # The clients' verdicts and handoff.
        foreach ($client in $handoffClients) {
            $verdict = if (Test-Path -LiteralPath $client.Log) { Select-String -LiteralPath $client.Log -Pattern 'VeyraSmoke: (PASS|FAIL).*' | Select-Object -Last 1 } else { $null }
            Write-Host ("Client {0} ({1}): {2}" -f $client.Number, $client.Participant.Name, $(if ($verdict) { $verdict.Matches[0].Value } else { 'no verdict logged' }))
            if (-not $verdict -or $verdict.Matches[0].Value -notmatch 'PASS') {
                $failed = $true
            }
            if (Test-Path -LiteralPath $client.Log) {
                Select-String -LiteralPath $client.Log -Pattern 'VeyraClientFlow: (signing in failed|problem|the connection to match).*' | ForEach-Object { Write-Host "  $($_.Matches[0].Value)" }
                if ($matchId -and -not (Select-String -LiteralPath $client.Log -SimpleMatch "VeyraClientFlow: joining match $matchId at " -Quiet)) {
                    Write-Host "  It never joined match $matchId through the client flow."
                    $failed = $true
                }
                # The grey-box presentation must start in the packaged client (ADR-008 §1).
                foreach ($uiError in @(Select-String -LiteralPath $client.Log -Pattern 'LogVeyraUI: Error: .*')) {
                    Write-Host "  Presentation error: $($uiError.Matches[0].Value)"
                    $failed = $true
                }
                # The client must also close cleanly: a crash after its verdict still fails. A client the
                # launcher CLI started is not this script's child, so its log is where a crash shows.
                if (Select-String -LiteralPath $client.Log -SimpleMatch '=== Critical error: ===' -Quiet) {
                    $frame = Select-String -LiteralPath $client.Log -Pattern '\[Callstack\] \S+ (\S+)' | Select-Object -First 1
                    Write-Host "  It crashed$(if ($frame) { " in $($frame.Matches[0].Groups[1].Value)" })."
                    $failed = $true
                }
            }
        }

        # -Flow -Screenshot: each screen the first client passed was saved.
        if ($Flow -and $Screenshot) {
            $screens = switch ($Flow) {
                'Practice' { 'StarterChoice', 'Home', 'Play', 'ChampionSelect', 'MatchMenu', 'Results' }
                'Casual' { 'Home', 'Play', 'Party', 'Queue', 'MatchFound', 'ChampionSelect', 'MatchMenu', 'Results' }
                'CasualDecline' { 'Home', 'Play', 'Party', 'Queue', 'MatchFound', 'Requeued' }
            }
            foreach ($screen in $screens) {
                $shot = Join-Path $reportDir "Flow-$screen.png"
                if (Test-Path -LiteralPath $shot) {
                    Write-Host "Screenshot: $shot"
                }
                else {
                    Write-Host "The client saved no screenshot of $screen."
                    $failed = $true
                }
            }
        }

        # The result the backend recorded (ADR-007 §7). A match found that did not go ahead has none.
        if (-not $expectsMatch) {
        }
        elseif (-not $match -or $match.state -ne 'ended' -or -not $match.result) {
            Write-Host "The backend recorded no result; the match is $(if ($match) { "$($match.state) ($($match.failureReason))" } else { 'unknown' })."
            $failed = $true
        }
        else {
            $result = $match.result
            $expectedEnd = $(if ($isPractice) { 'host_ended' } else { 'developer_request' })
            Write-Host ("Result: {0}, winner {1}, {2:N1} s." -f $result.endReason, $(if ($null -eq $result.winner) { 'none' } else { $result.winner }), $result.durationSeconds)
            if ($result.endReason -ne $expectedEnd -or $null -ne $result.winner -or $result.durationSeconds -le 0) {
                Write-Host "Expected $expectedEnd with no winner and a positive duration."
                $failed = $true
            }
            $expectedAccounts = @($participants.AccountId | Sort-Object)
            $reportedAccounts = @($result.participants.accountId | Sort-Object)
            if (($expectedAccounts -join ',') -ne ($reportedAccounts -join ',') -or @($result.participants | Where-Object { -not ($_.joined -and $_.connectedAtEnd) }).Count -gt 0) {
                Write-Host 'Expected every participant to have joined and to be connected at the end.'
                $failed = $true
            }
            $rostered = @($match.participants | Where-Object { $_.vanguardId -ne ($participants | Where-Object AccountId -eq $_.accountId).Vanguard })
            $expectedRules = $(if ($isPractice) { 'practice' } else { 'standard' })
            if ($rostered.Count -gt 0 -or $match.rules -ne $expectedRules -or ($isPractice -and $match.hostAccountId -ne $participants[0].AccountId)) {
                Write-Host 'The backend did not keep the requested rules, host or Vanguards.'
                $failed = $true
            }
            if ($isMatchmade -and $match.mode -ne $mode) {
                Write-Host "The match's mode is $($match.mode), not $mode."
                $failed = $true
            }
        }
        if ($expectsMatch -and -not ($match -and $match.serverRemoved)) {
            Write-Host 'The backend did not remove the match server.'
            $failed = $true
        }

        # What the server logged: each participant playing the Vanguard its roster names. A match that
        # never started leaves an empty log, whose every line is missing.
        if (-not (Test-Path -LiteralPath $serverLogPath)) {
            New-Item -ItemType File -Path $serverLogPath | Out-Null
        }
        $expectedServerLines =@('VeyraHandoff: took the assignment', 'VeyraHandoff: reported ready', "Preparation begins with $playerCount player(s)", 'The match is live',
            'VeyraHandoff: reported result') + @($participants | ForEach-Object { "$($_.Name) plays $($_.Vanguard)." })
        $expectedServerLines += $(if ($isPractice) { @('the host, ended the custom match', 'The match ended (host ended') }
            elseif ($isMatchmade) { @('The match ended (developer request') }
            else { @('Match paused', 'Match resumed', 'The match ended (developer request') })
        if (-not $expectsMatch) {
            $expectedServerLines = @()
        }
        # A practice match adds the practice bots the backend's configuration lists (ADR-010 §7).
        $practiceBots = @($backendConfig.customPractice.bots)
        if ($isPractice -and $practiceBots.Count -gt 0) {
            $expectedServerLines += "Added $($practiceBots.Count) of the assignment's $($practiceBots.Count) bot(s)."
        }
        foreach ($expected in $expectedServerLines) {
            if (-not (Select-String -LiteralPath $serverLogPath -SimpleMatch $expected -Quiet)) {
                Write-Host "The server log never says '$expected'."
                $failed = $true
            }
        }
        foreach ($serverError in @(Select-String -LiteralPath $serverLogPath -Pattern 'LogVeyra\w*: Error: .*')) {
            Write-Host "The server logged an error: $($serverError.Matches[0].Value)"
            $failed = $true
        }
        if (-not $isPractice -and -not $Flow) {
            $abilityQ = @($vanguardDefinitions.$SmokeVanguard.abilities.q)[0]
            $casts = @(Select-String -LiteralPath $serverLogPath -SimpleMatch " cast $abilityQ at ").Count
            if ($casts -lt 2) {
                Write-Host "The server log shows $casts cast(s) of $abilityQ; expected one from each client."
                $failed = $true
            }
        }

        # No credential in any log. The engine logs each connection's login options on the server, in its
        # "Login request" and "Join request" lines, so a join ticket appears there: ADR-007's known
        # exposure, until tickets move into the handshake. Nowhere else.
        $credentialPattern = '(vls|vgs|vlc|vms)_[A-Za-z0-9_-]{8,}'
        $ticketPattern = 'vjt_[A-Za-z0-9_-]{8,}'
        $engineLoginLine = 'LogNet: (Login|Join) request: '
        $launcherLogs = @(Get-ChildItem -LiteralPath $reportDir -Filter 'Launcher*.log' | ForEach-Object FullName)
        foreach ($log in @($handoffClients.Log) + $launcherLogs + @($serverLogPath, $serverErrorLogPath)) {
            if (-not (Test-Path -LiteralPath $log)) { continue }
            $name = Split-Path -Leaf $log
            $credentialLines = @(Select-String -LiteralPath $log -Pattern $credentialPattern).Count
            $ticketLines = @(Select-String -LiteralPath $log -Pattern $ticketPattern | Where-Object { $log -ne $serverLogPath -or $_.Line -notmatch $engineLoginLine }).Count
            if ($credentialLines + $ticketLines -gt 0) {
                Write-Host "$name holds a credential on $($credentialLines + $ticketLines) line(s)."
                $failed = $true
            }
        }
        $exposed = @(Select-String -LiteralPath $serverLogPath -Pattern $ticketPattern).Count
        Write-Host "Join tickets in the server's own login and join lines: $exposed (the known exposure in ADR-007)."
    }
    finally {
        # Leave nothing behind, even after an error: a client or log reader still running, and a
        # server the backend did not remove.
        foreach ($client in @($handoffClients)) {
            if ($client -and $client.Process -and -not $client.Process.HasExited) {
                $client.Process.Kill($true)
            }
        }
        if ($serverLogProcess -and -not $serverLogProcess.HasExited) {
            $serverLogProcess.Kill($true)
        }
        if ($container -and (& docker ps --all --quiet --filter "name=^/$container$")) {
            Write-Host "Removing $container."
            & docker rm --force $container | Out-Null
        }
    }
    # And a backend this script started.
    if (-not $backendWasRunning -and -not $KeepServer) {
        $null = Invoke-Compose -Arguments @('stop', 'backend', 'postgres')
    }

    $smokeName = $(if ($Flow) { "$Flow flow" } else { 'handoff' })
    if ($failed) {
        Write-Host "The $smokeName smoke test failed. Logs: $reportDir"
        exit $ExitFailed
    }
    Write-Host "The $smokeName smoke test passed. Logs: $reportDir"
    exit $ExitPassed
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
    if ($RecordReplay) {
        # A graceful stop lets the server finish the replay before it is copied out.
        $null = Invoke-Compose -Arguments @('stop', 'match-server')
        $demosDir = Join-Path $reportDir 'Demos'
        New-Item -ItemType Directory -Force -Path $demosDir | Out-Null
        & docker compose --progress plain --project-directory $repositoryDir --profile match-server cp "match-server:$ReplayContainerDir/." $demosDir | Out-Host
    }
    Get-ServerLog | Out-File -LiteralPath $serverLogPath -Encoding utf8
    if (-not $KeepServer) {
        # Only the match server: 'down' would also stop the backend and its database.
        $null = Invoke-Compose -Arguments @('rm', '--stop', '--force', 'match-server')
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
    $env:VEYRA_EXPECTED_PLAYERS = $clientCount
    $env:VEYRA_MATCH_URL_OPTIONS = $urlOptions
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

$screenshotPath = Join-Path $reportDir $ScreenshotName
$clientProcesses = foreach ($index in 1..$clientCount) {
    # With -Screenshot the second client renders; the first, which checks the pause, stays headless.
    $renders = $Screenshot -and $index -eq 2
    $clientArguments = $clientPrefix + @(
        '-VeyraSmoke'
        "-VeyraVanguard=$(if ($kitMode) { $Vanguards[$index - 1] } else { $SmokeVanguard })"
        '-nosound'
        '-nosplash'
        '-unattended'
        "-ABSLOG=`"$(Join-Path $reportDir "Client$index.log")`""
    )
    $clientArguments += $(if ($renders) { $ScreenshotWindow + "-VeyraSmokeScreenshot=`"$screenshotPath`"" } else { '-nullrhi' })
    if ($kitMode) {
        $clientArguments += '-VeyraSmokeKit'
    }
    elseif ($index -eq 1) {
        $clientArguments += '-VeyraSmokePause'
    }
    $stay = $(if ($renders) { [Math]::Max($ClientStaySeconds, $ScreenshotStaySeconds) } else { $ClientStaySeconds })
    if ($stay -gt 0) {
        $clientArguments += "-VeyraSmokeStay=$stay"
    }
    $process = Start-Process -FilePath $clientExecutable -ArgumentList ($clientArguments -join ' ') -PassThru
    $null = $process.Handle # Keeps the exit code readable after the process ends.
    $process
}

$failed = $false
foreach ($process in $clientProcesses) {
    if (-not $process.WaitForExit([TimeSpan]::FromMinutes($TimeoutMinutes) + [TimeSpan]::FromSeconds([Math]::Max($ClientStaySeconds, $ScreenshotStaySeconds)))) {
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
    # The grey-box presentation logs an error and stays off when its settings or assets are missing,
    # as in a package that did not cook them (ADR-008 §1).
    if (Test-Path -LiteralPath $log) {
        foreach ($uiError in @(Select-String -LiteralPath $log -Pattern 'LogVeyraUI: Error: .*')) {
            Write-Host "Client $index logged a presentation error: $($uiError.Matches[0].Value)"
            $failed = $true
        }
    }
}
if ($Screenshot) {
    if (Test-Path -LiteralPath $screenshotPath) {
        Write-Host "Grey-box screenshot: $screenshotPath"
    }
    else {
        Write-Host 'The rendering client saved no screenshot.'
        $failed = $true
    }
}

$expectedServerLines = @("Preparation begins with $clientCount player(s)", 'The match is live')
if (-not $kitMode) {
    $expectedServerLines += 'Match paused', 'Match resumed'
}
foreach ($expected in $expectedServerLines) {
    if (-not (Select-String -LiteralPath $serverLogPath -SimpleMatch $expected -Quiet)) {
        Write-Host "The server log never says '$expected'."
        $failed = $true
    }
}
# No Veyra code on the server may log an error during the match.
$serverErrors = @(Select-String -LiteralPath $serverLogPath -Pattern 'LogVeyra\w*: Error: .*')
foreach ($serverError in $serverErrors) {
    Write-Host "The server logged an error: $($serverError.Matches[0].Value)"
    $failed = $true
}
# Each client's casts, as the server resolved them (VeyraAbilities logs them at Verbose).
if ($kitMode) {
    foreach ($vanguard in $Vanguards) {
        foreach ($slot in $SlotKeys) {
            $ability = @($vanguardDefinitions.$vanguard.abilities.$slot)[0]
            if (-not (Select-String -LiteralPath $serverLogPath -SimpleMatch " cast $ability at " -Quiet)) {
                Write-Host "The server log shows no cast of $ability ($vanguard's $($slot.ToUpper()))."
                $failed = $true
            }
        }
    }
}
else {
    $abilityQ = @($vanguardDefinitions.$SmokeVanguard.abilities.q)[0]
    $casts = @(Select-String -LiteralPath $serverLogPath -SimpleMatch " cast $abilityQ at ").Count
    if ($casts -lt 2) {
        Write-Host "The server log shows $casts cast(s) of $abilityQ; expected one from each client."
        $failed = $true
    }
}

if ($NetStatsSeconds -gt 0) {
    Write-Host 'Server network statistics:'
    Select-String -LiteralPath $serverLogPath -Pattern 'VeyraNetStats: window=.*' | ForEach-Object { Write-Host "  $($_.Matches[0].Value)" }
}

if ($RecordReplay) {
    $replayFile = Join-Path $reportDir "Demos\$ReplayName.replay"
    if (-not (Test-Path -LiteralPath $replayFile)) {
        Write-Host "The server left no replay at $ReplayContainerDir/$ReplayName.replay."
        $failed = $true
    }
    else {
        Write-Host ("Replay: {0:N0} KB." -f ((Get-Item -LiteralPath $replayFile).Length / 1KB))
        New-Item -ItemType Directory -Force -Path $clientDemosDir | Out-Null
        Copy-Item -LiteralPath $replayFile -Destination $clientDemosDir -Force
        $playbackLog = Join-Path $reportDir 'Playback.log'
        $playbackArguments = $playbackPrefix + @("-VeyraReplayCheck=$ReplayName", "-VeyraReplayVanguards=$clientCount", '-nullrhi', '-nosound', '-nosplash', '-unattended', "-ABSLOG=`"$playbackLog`"")
        if ($kitMode) {
            # The kits pause nothing, and their skillshots, ranged attacks and delayed areas must replay.
            $playbackArguments += '-VeyraReplaySkipPause', '-VeyraReplayDeliveries'
        }
        $playback = Start-Process -FilePath $clientExecutable -ArgumentList ($playbackArguments -join ' ') -PassThru
        if (-not $playback.WaitForExit([TimeSpan]::FromMinutes($TimeoutMinutes))) {
            $playback.Kill($true)
        }
        $verdict = if (Test-Path -LiteralPath $playbackLog) { Select-String -LiteralPath $playbackLog -Pattern 'VeyraReplayCheck: (PASS|FAIL).*' | Select-Object -Last 1 } else { $null }
        Write-Host ("Playback: {0}" -f $(if ($verdict) { $verdict.Matches[0].Value } else { 'no verdict logged' }))
        if (-not $verdict -or $verdict.Matches[0].Value -notmatch 'PASS') {
            $failed = $true
        }
    }
}

if ($failed) {
    Write-Host "The smoke test failed. Logs: $reportDir"
    exit $ExitFailed
}
Write-Host "The smoke test passed. Logs: $reportDir"
exit $ExitPassed
