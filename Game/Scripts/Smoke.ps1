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
    gets a fresh launch code through a pipe as soon as it waits for one. It redeems the code, waits
    for its match, joins with its ticket and plays the script; the first client then ends the match
    once both casts have landed. The backend must record the result (a developer request, no winner,
    both participants joined and connected at the end) and remove the server. No log may hold a
    credential, except the join tickets the engine logs with each login on the server (ADR-007,
    known exposure). The backend is stopped afterwards only if this script started it.

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
.PARAMETER Vanguards
    The Vanguards whose kits the clients play, one client each, from Vanguards.json. Not with -Handoff.
.PARAMETER Practice
    With -Handoff: a solo practice match that its host ends.
.PARAMETER EngineRoot
    Engine folder to use instead of the one registered for the project's EngineAssociation.
.EXAMPLE
    ./Game/Scripts/Smoke.ps1 -Clients Editor
.EXAMPLE
    ./Game/Scripts/Smoke.ps1 -Vanguards cairn,qazharr,oriel,bryn -RecordReplay
.EXAMPLE
    ./Game/Scripts/Smoke.ps1 -Handoff
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

$projectFile = Get-VeyraProjectFile
$gameDir = Split-Path -Parent $projectFile
$repositoryDir = Split-Path -Parent $gameDir
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
    & docker compose --project-directory $repositoryDir --profile match-server @Arguments | Out-Host
    return $LASTEXITCODE
}

if ($Handoff) {
    # The session handoff (ADR-007): the backend creates the match and starts its server; each client
    # redeems a launch code from its standard input and joins with its ticket.

    # Harness settings, not gameplay: how long to wait for each stage.
    $LaunchCodeWaitSeconds = 60
    $MatchEndWaitSeconds = 60
    $ServerLogWaitSeconds = 30
    $RemovalMarginSeconds = 15
    $BackendRequestTimeoutSeconds = 10
    $WaitingForCodeLine = 'VeyraHandoff: waiting for the launch code'

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
    # the host plays a released Vanguard, as a player would.
    $PracticeVanguard = 'cairn'
    $playerCount = $(if ($Practice) { 1 } else { 2 })
    $mode = $(if ($Practice) { $backendConfig.customPractice.mode } else { ($backendConfig.modes | Where-Object { $_.enabled } | Select-Object -First 1).id })
    $accounts = @($backendConfig.devLogin.accounts | Select-Object -First $playerCount)

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
            Side = $(if ($Practice) { $backendConfig.customPractice.hostSide } else { @('A', 'B')[$index] })
            Vanguard = $(if ($Practice) { $PracticeVanguard } else { $SmokeVanguard }) }
    }
    $matchRequest = @{
        mode         = $mode
        rules        = $(if ($Practice) { 'practice' } else { 'standard' })
        participants = @($participants | ForEach-Object { @{ accountId = $_.AccountId; side = $_.Side; vanguardId = $_.Vanguard } })
    }
    if ($Practice) {
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

    $handoffClients = @()
    $serverLogProcess = $null
    $failed = $false
    try {
        # The server's log, for as long as its container runs.
        $serverErrorLogPath = Join-Path $reportDir 'Server.stderr.log'
        $serverLogProcess = Start-Process -FilePath 'docker' -ArgumentList @('logs', '--follow', $container) -NoNewWindow -PassThru `
            -RedirectStandardOutput $serverLogPath -RedirectStandardError $serverErrorLogPath

        # Each client reads its launch code from a pipe, and plays the Vanguard the roster names. In a
        # standard match the first plays the pause and ends the match, and the second waits for the end;
        # a practice match's host ends it. None uses -log, which on Windows can replace the standard
        # handles. Not $clients: PowerShell names ignore case, and that is the -Clients parameter.
        $handoffClients = foreach ($index in 0..($playerCount - 1)) {
            $log = Join-Path $reportDir "Client$($index + 1).log"
            $clientArguments = @('-VeyraLaunchCode=stdin', '-VeyraSmoke', '-nullrhi', '-nosound', '-nosplash', '-unattended', "-ABSLOG=`"$log`"")
            $clientArguments += $(if ($Practice) { @('-VeyraSmokeEndCustomMatch') } elseif ($index -eq 0) { @('-VeyraSmokePause', '-VeyraSmokeEndMatch') } else { @('-VeyraSmokeWaitForEnd') })
            $startInfo = [System.Diagnostics.ProcessStartInfo]::new($clientExecutable, ($clientArguments -join ' '))
            $startInfo.UseShellExecute = $false
            # No console of their own, and their output is discarded: their logs go to -ABSLOG.
            $startInfo.CreateNoWindow = $true
            $startInfo.RedirectStandardInput = $true
            $startInfo.RedirectStandardOutput = $true
            $startInfo.RedirectStandardError = $true
            $startInfo.StandardInputEncoding = [System.Text.UTF8Encoding]::new($false)
            $process = [System.Diagnostics.Process]::Start($startInfo)
            $null = $process.StandardOutput.BaseStream.CopyToAsync([System.IO.Stream]::Null)
            $null = $process.StandardError.BaseStream.CopyToAsync([System.IO.Stream]::Null)
            [pscustomobject]@{ Number = $index + 1; Participant = $participants[$index]; Log = $log; Process = $process }
        }

        # A launch code lives only seconds, so each is issued when its client is ready to read it.
        $pending = [System.Collections.Generic.List[object]]::new()
        $handoffClients | ForEach-Object { $pending.Add($_) }
        $deadline = (Get-Date).AddSeconds($LaunchCodeWaitSeconds)
        while ($pending.Count -gt 0 -and (Get-Date) -lt $deadline) {
            foreach ($client in @($pending)) {
                if ($client.Process.HasExited) {
                    $null = $pending.Remove($client)
                    continue
                }
                if ((Test-Path -LiteralPath $client.Log) -and (Select-String -LiteralPath $client.Log -SimpleMatch $WaitingForCodeLine -Quiet)) {
                    $issued = Invoke-Backend -Method Post -Path '/v1/launch-codes' -Body @{ buildVersion = $buildVersion } -Credential $client.Participant.LauncherSession
                    try {
                        if ($issued.Status -eq 200) {
                            $client.Process.StandardInput.Write($issued.Body.token + "`n")
                            $client.Process.StandardInput.Flush()
                        }
                        else {
                            Write-Host "Client $($client.Number) got no launch code: HTTP $($issued.Status) ($(Get-ErrorCode $issued))."
                            $failed = $true
                        }
                        $client.Process.StandardInput.Close()
                    }
                    catch {
                        Write-Host "Client $($client.Number) closed its standard input before its launch code arrived."
                        $failed = $true
                    }
                    $issued = $null
                    $null = $pending.Remove($client)
                }
            }
            Start-Sleep -Milliseconds 250
        }
        foreach ($client in $pending) {
            Write-Host "Client $($client.Number) never waited for its launch code."
            $client.Process.StandardInput.Close()
            $failed = $true
        }
        $participants | ForEach-Object { $_.LauncherSession = $null }

        foreach ($client in $handoffClients) {
            if (-not $client.Process.WaitForExit([int][TimeSpan]::FromMinutes($TimeoutMinutes).TotalMilliseconds)) {
                $client.Process.Kill($true)
                $failed = $true
            }
        }

        # The server reports the result and quits; the backend then ends the match and, later, removes
        # the container.
        function Get-DevMatch {
            $answer = Invoke-Backend -Method Get -Path "/v1/dev/matches/$matchId"
            return $(if ($answer.Status -eq 200) { $answer.Body.match } else { $null })
        }
        $deadline = (Get-Date).AddSeconds($MatchEndWaitSeconds)
        do {
            $match = Get-DevMatch
            if ($match -and $match.state -in 'ended', 'failed') { break }
            Start-Sleep -Seconds 1
        } while ((Get-Date) -lt $deadline)
        if (-not $serverLogProcess.WaitForExit($ServerLogWaitSeconds * 1000)) {
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
                Select-String -LiteralPath $client.Log -Pattern 'VeyraHandoff: FAIL.*' | ForEach-Object { Write-Host "  $($_.Matches[0].Value)" }
                if (-not (Select-String -LiteralPath $client.Log -SimpleMatch "VeyraHandoff: joining match $matchId" -Quiet)) {
                    Write-Host "  It never joined match $matchId through the handoff."
                    $failed = $true
                }
                # The grey-box presentation must start in the packaged client (ADR-008 §1).
                foreach ($uiError in @(Select-String -LiteralPath $client.Log -Pattern 'LogVeyraUI: Error: .*')) {
                    Write-Host "  Presentation error: $($uiError.Matches[0].Value)"
                    $failed = $true
                }
            }
        }

        # The result the backend recorded (ADR-007 §7).
        if (-not $match -or $match.state -ne 'ended' -or -not $match.result) {
            Write-Host "The backend recorded no result; the match is $(if ($match) { "$($match.state) ($($match.failureReason))" } else { 'unknown' })."
            $failed = $true
        }
        else {
            $result = $match.result
            $expectedEnd = $(if ($Practice) { 'host_ended' } else { 'developer_request' })
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
            if ($rostered.Count -gt 0 -or ($Practice -and ($match.rules -ne 'practice' -or $match.hostAccountId -ne $participants[0].AccountId))) {
                Write-Host 'The backend did not keep the requested rules, host or Vanguards.'
                $failed = $true
            }
        }
        if (-not ($match -and $match.serverRemoved)) {
            Write-Host 'The backend did not remove the match server.'
            $failed = $true
        }

        # What the server logged: each participant playing the Vanguard its roster names.
        $expectedServerLines = @('VeyraHandoff: took the assignment', 'VeyraHandoff: reported ready', "Preparation begins with $playerCount player(s)", 'The match is live',
            'VeyraHandoff: reported result') + @($participants | ForEach-Object { "$($_.Name) plays $($_.Vanguard)." })
        $expectedServerLines += $(if ($Practice) { @('the host, ended the custom match', 'The match ended (host ended') } else { @('Match paused', 'Match resumed', 'The match ended (developer request') })
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
        if (-not $Practice) {
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
        foreach ($log in @($handoffClients.Log) + @($serverLogPath, $serverErrorLogPath)) {
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
            if ($client -and -not $client.Process.HasExited) {
                $client.Process.Kill($true)
            }
        }
        if ($serverLogProcess -and -not $serverLogProcess.HasExited) {
            $serverLogProcess.Kill($true)
        }
        if (& docker ps --all --quiet --filter "name=^/$container$") {
            Write-Host "Removing $container."
            & docker rm --force $container | Out-Null
        }
    }
    # And a backend this script started.
    if (-not $backendWasRunning -and -not $KeepServer) {
        $null = Invoke-Compose -Arguments @('stop', 'backend', 'postgres')
    }

    if ($failed) {
        Write-Host "The handoff smoke test failed. Logs: $reportDir"
        exit $ExitFailed
    }
    Write-Host "The handoff smoke test passed. Logs: $reportDir"
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
        & docker compose --project-directory $repositoryDir --profile match-server cp "match-server:$ReplayContainerDir/." $demosDir | Out-Host
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
