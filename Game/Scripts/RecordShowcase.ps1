#Requires -Version 7.0
<#
.SYNOPSIS
    Records a Vanguard's skills in motion on the skill showcase's stage, and encodes each as a GIF and an MP4.
.DESCRIPTION
    The skill showcase recorder (ADR-074):
    1. Starts a match server from the editor build on the stage map (L_Showcase; BuildGreyboxMap.ps1 -Showcase makes
       it). The server seats a dummy Vanguard opposite the player.
    2. Starts one rendering game client that plays the Vanguard and records each ability in turn. Before each cast it
       resets the stage, so the dummy is whole and back at its start. It captures every other frame, with the HUD
       hidden and the camera on its Vanguard, as the showcase settings in DefaultGame.ini say.
    3. Stops the server once the client finishes.
    4. Encodes the frames into Game/Saved/Showcase/<vanguard>/<ability>.gif and .mp4 with
       Game/Scripts/Showcase/encode_showcase.py, whose dependencies Game/Scripts/Showcase/requirements.txt pins.

    The frames stay under Game/Saved/Showcase/<vanguard>/frames. Nothing is written into the repository. The editor
    must already be built (Build.ps1 -Target VeyraEditor). The client window opens on the desktop; leave it alone
    while it records.
.PARAMETER Vanguard
    The Vanguard to record, by its id in Game/Tuning/Vanguards.json.
.PARAMETER Dummy
    The Vanguard the dummy plays; the showcase settings' default without it.
.PARAMETER SkipEncode
    Records the frames only.
.PARAMETER EngineRoot
    Engine folder to use instead of the one registered for the project's EngineAssociation.
.EXAMPLE
    ./Game/Scripts/RecordShowcase.ps1 -Vanguard oriel
#>
[CmdletBinding()]
param(
    [Parameter(Mandatory)]
    [ValidatePattern('^[a-z0-9_]+$')]
    [string]$Vanguard,
    [ValidatePattern('^[a-z0-9_]+$')]
    [string]$Dummy,
    [switch]$SkipEncode,
    [string]$EngineRoot
)

$ErrorActionPreference = 'Stop'
Import-Module (Join-Path $PSScriptRoot 'VeyraProject.psm1') -Force

$ExitRecorded = 0
$ExitFailed = 1
$ExitInfrastructure = 2

# Harness settings, not gameplay: where the server listens, the line it logs once it accepts players, how long to wait
# for that line, and how much longer than the recorder's own time limit to wait for the client.
$ServerPort = 7777
$ServerReadyLine = 'Match server ready'
$ServerReadyTimeoutSeconds = 180
$ClientGraceSeconds = 60
$StageMap = '/Game/Veyra/Developer/Maps/L_Showcase'

$projectFile = Get-VeyraProjectFile
$gameDir = Split-Path -Parent $projectFile
$engineRoot = Resolve-VeyraEngineRoot -ProjectFile $projectFile -EngineRoot $EngineRoot
$editorExecutable = Join-Path $engineRoot 'Engine\Binaries\Win64\UnrealEditor.exe'
if (-not (Test-Path -LiteralPath $editorExecutable -PathType Leaf)) {
    Write-Host "UnrealEditor.exe was not found at '$editorExecutable'. Build VeyraEditor first."
    exit $ExitInfrastructure
}
if (-not (Test-Path -LiteralPath (Join-Path $gameDir 'Content\Veyra\Developer\Maps\L_Showcase.umap') -PathType Leaf)) {
    Write-Host 'The showcase stage is missing. Run BuildGreyboxMap.ps1 -Showcase first.'
    exit $ExitInfrastructure
}
$vanguards = (Get-Content -LiteralPath (Join-Path $gameDir 'Tuning\Vanguards.json') -Raw | ConvertFrom-Json).vanguards
foreach ($named in @($Vanguard, $Dummy) | Where-Object { $_ }) {
    if (-not $vanguards.PSObject.Properties[$named]) {
        Write-Host "Unknown Vanguard '$named'. Choose one of: $(($vanguards.PSObject.Properties.Name) -join ', ')."
        exit $ExitInfrastructure
    }
}
if (@(Get-NetUDPEndpoint -LocalPort $ServerPort -ErrorAction SilentlyContinue).Count -gt 0) {
    Write-Host "Something already listens on UDP port $ServerPort, perhaps a match server left running. Stop it first."
    exit $ExitInfrastructure
}

# The recorder's window opens at the size its frames are captured at, and the client gets its time limit and a grace.
$settings = Get-Content -LiteralPath (Join-Path $gameDir 'Config\DefaultGame.ini') -Raw
$size = [regex]::Match($settings, '(?ms)^\[/Script/VeyraDeveloper\.VeyraShowcaseSettings\].*?^CaptureSize=\(X=(\d+),Y=(\d+)\)')
$limit = [regex]::Match($settings, '(?ms)^\[/Script/VeyraDeveloper\.VeyraShowcaseSettings\].*?^TimeoutSeconds=([\d.]+)')
if (-not $size.Success -or -not $limit.Success) {
    Write-Host 'DefaultGame.ini has no showcase CaptureSize or TimeoutSeconds ([/Script/VeyraDeveloper.VeyraShowcaseSettings]).'
    exit $ExitInfrastructure
}
$clientLimitSeconds = [double]$limit.Groups[1].Value + $ClientGraceSeconds

$recordingDir = Join-Path $gameDir "Saved\Showcase\$Vanguard"
$framesDir = Join-Path $recordingDir 'frames'
if (Test-Path -LiteralPath $framesDir) {
    Remove-Item -LiteralPath $framesDir -Recurse -Force
}
New-Item -ItemType Directory -Force -Path $framesDir | Out-Null
$serverLogPath = Join-Path $recordingDir 'Server.log'
$clientLogPath = Join-Path $recordingDir 'Client.log'

$urlOptions = "?VeyraExpectedPlayers=1?VeyraShowcase$(if ($Dummy) { "?VeyraShowcaseDummy=$Dummy" })"
$serverProcess = $null
$clientProcess = $null
try {
    Write-Host "Starting the showcase stage's server (editor build)."
    $serverArguments = @("`"$projectFile`"", "$StageMap$urlOptions", "-port=$ServerPort", '-server', '-nullrhi', '-unattended', '-nosplash',
        "-ABSLOG=`"$serverLogPath`"")
    $serverProcess = Start-Process -FilePath $editorExecutable -ArgumentList ($serverArguments -join ' ') -PassThru -WindowStyle Hidden

    $deadline = (Get-Date).AddSeconds($ServerReadyTimeoutSeconds)
    while (-not ((Test-Path -LiteralPath $serverLogPath) -and (Select-String -LiteralPath $serverLogPath -Pattern $ServerReadyLine -Quiet))) {
        if ((Get-Date) -gt $deadline -or $serverProcess.HasExited) {
            Write-Host "The server did not get ready. Its log: $serverLogPath"
            exit $ExitInfrastructure
        }
        Start-Sleep -Seconds 1
    }

    Write-Host "Recording $Vanguard's skills$(if ($Dummy) { " against $Dummy" }). Leave the window alone until it closes."
    $clientArguments = @("`"$projectFile`"", "127.0.0.1:$ServerPort", '-game', "-VeyraVanguard=$Vanguard", "-VeyraShowcase=`"$framesDir`"",
        '-windowed', "-ResX=$($size.Groups[1].Value)", "-ResY=$($size.Groups[2].Value)", '-nosplash', '-nosound', "-ABSLOG=`"$clientLogPath`"")
    $clientProcess = Start-Process -FilePath $editorExecutable -ArgumentList ($clientArguments -join ' ') -PassThru
    $null = $clientProcess.Handle # Keeps the exit code readable after the process ends.
    if (-not $clientProcess.WaitForExit([int]([TimeSpan]::FromSeconds($clientLimitSeconds).TotalMilliseconds))) {
        $clientProcess.Kill($true)
        Write-Host "The recorder did not finish in time. Its log: $clientLogPath"
    }
}
finally {
    if ($clientProcess -and -not $clientProcess.HasExited) {
        $clientProcess.Kill($true)
    }
    if ($serverProcess -and -not $serverProcess.HasExited) {
        $serverProcess.Kill($true)
        $serverProcess.WaitForExit()
    }
}

$verdict = if (Test-Path -LiteralPath $clientLogPath) { Select-String -LiteralPath $clientLogPath -Pattern 'VeyraShowcase: (DONE|FAIL).*' | Select-Object -Last 1 } else { $null }
Write-Host ("Recorder: {0}" -f $(if ($verdict) { $verdict.Matches[0].Value } else { 'no verdict logged' }))
$recorded = $verdict -and $verdict.Matches[0].Value -match 'DONE'
# A recording that left an ability out still wrote its manifest: the clips it made are encoded, and it still fails.
if (-not (Test-Path -LiteralPath (Join-Path $framesDir 'manifest.json'))) {
    Write-Host "Logs: $recordingDir"
    exit $ExitFailed
}
if ($SkipEncode) {
    Write-Host "Frames: $framesDir"
    exit $(if ($recorded) { $ExitRecorded } else { $ExitFailed })
}

& python (Join-Path $PSScriptRoot 'Showcase\encode_showcase.py') $framesDir --out $recordingDir
$encoded = $LASTEXITCODE -eq 0
if (-not $encoded) {
    Write-Host "Not every clip was encoded. If the encoder could not start, install its dependencies with: python -m pip install -r $(Join-Path $PSScriptRoot 'Showcase\requirements.txt')"
}
Write-Host "Clips: $recordingDir"
exit $(if ($recorded -and $encoded) { $ExitRecorded } else { $ExitFailed })
