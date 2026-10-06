#Requires -Version 7.0
<#
.SYNOPSIS
    Captures Vanguards' generated bodies in the lit Crucible, from the gameplay camera's angle (ADR-064 section 4).
.DESCRIPTION
    Opens L_Battleground in the editor and stands each Vanguard named on the Bottom lane in its animations' key poses:
    one row turned three-quarters toward the camera, one in profile. Each is captured at 1920x1080 from the gameplay
    camera's pitch (VeyraCameraSettings in DefaultGame.ini): <Body>_Game.png at the camera's own distance and field of
    view, the size players see, and <Body>_Close.png through a narrow lens from the same angle, to judge the silhouette.
    Images and a manifest go to Game/Saved/VanguardKit/Review. Nothing is saved to the map.
.PARAMETER Vanguards
    The Vanguards to capture, by ID, with bodies imported by BuildVanguardBodies.ps1.
#>
[CmdletBinding()]
param(
    [Parameter(Mandatory)]
    [ValidatePattern('^[a-z0-9_]+(,[a-z0-9_]+)*$')]
    [string[]]$Vanguards,
    [string]$EngineRoot
)
$ErrorActionPreference = 'Stop'
Import-Module (Join-Path $PSScriptRoot 'VeyraProject.psm1') -Force
$project = Get-VeyraProjectFile
$engine = Resolve-VeyraEngineRoot -ProjectFile $project -EngineRoot $EngineRoot
$game = Split-Path -Parent $project
$output = Join-Path $game 'Saved/VanguardKit/Review'
New-Item -ItemType Directory -Force -Path $output | Out-Null
$manifest = Join-Path $output 'manifest.json'
if (Test-Path -LiteralPath $manifest) { Remove-Item -LiteralPath $manifest }
# The gameplay camera, from the one place it is set.
$section = $false
$camera = @{}
foreach ($line in Get-Content (Join-Path $game 'Config/DefaultGame.ini')) {
    if ($line -match '^\[(.+)\]$') { $section = $Matches[1] -eq '/Script/VeyraMatch.VeyraCameraSettings'; continue }
    if ($section -and $line -match '^(Distance|PitchDegrees)=(.+)$') { $camera[$Matches[1]] = $Matches[2].Trim() }
}
if ($camera.Count -ne 2) { throw 'DefaultGame.ini has no VeyraCameraSettings Distance and PitchDegrees.' }
$env:VEYRA_VANGUARD_REVIEW_OUTPUT = $output
$env:VEYRA_VANGUARD_REVIEW_IDS = ($Vanguards | ForEach-Object { $_ -split ',' }) -join ','
$env:VEYRA_VANGUARD_REVIEW_CAMERA = ($camera.GetEnumerator() | ForEach-Object { "$($_.Key)=$($_.Value)" }) -join ';'
$script = Join-Path $PSScriptRoot 'CaptureVanguards.py'
$editor = Join-Path $engine 'Engine/Binaries/Win64/UnrealEditor.exe'
$arguments = @("`"$project`"", "-ExecutePythonScript=`"$script`"", '-EnablePlugins=PythonScriptPlugin', '-unattended', '-nosplash', '-nosound', '-RenderOffscreen', '-windowed', '-ResX=1920', '-ResY=1080', "-ABSLOG=`"$output/Capture.log`"")
$process = Start-Process -FilePath $editor -ArgumentList $arguments -WindowStyle Hidden -PassThru -Wait
# The manifest is written only once every view is captured; the editor's own exit code on quitting is not reliable.
if (-not (Test-Path -LiteralPath $manifest) -or (Get-Content -LiteralPath $manifest -Raw | ConvertFrom-Json).status -ne 'captured') { throw "Capture failed (editor exit code $($process.ExitCode)). Inspect $output/Capture.log and failure.txt." }
Write-Host "Vanguard review images and manifest: $output"
