#Requires -Version 7.0
<#
.SYNOPSIS
    Captures the combat readability presentation in the lit Crucible, from the gameplay camera's angle (ADR-063).
.DESCRIPTION
    Opens L_Battleground in the editor and, on the Bottom lane under the map's own sun and manual exposure, stands
    the presentation's moments side by side: bodies unflashed, at the hit flash's peak and its Reduce Flashing peak,
    an enemy hovered (the outline) and an imported Vanguard body flashed; the impact, the cast flash and the death
    burst each frozen early, midway and late; a projectile's trail, the click marker's rings and a melee swing's arc.
    Each is captured at 1920x1080 from the gameplay camera's pitch (VeyraCameraSettings in DefaultGame.ini):
    Presentation_Game.png at the camera's own distance and field of view, the size players see, and
    Presentation_<Row>.png through a narrow lens for each row. Then the skills' own effects (ADR-072) stand alone on
    the middle row: Presentation_Skills.png. Images and a manifest go to
    Game/Saved/PresentationReview. Nothing is saved to the map.

    Build the presentation's materials and effects first (BuildPresentationMaterials.ps1, BuildEffects.ps1).
#>
[CmdletBinding()]
param(
    [string]$EngineRoot
)
$ErrorActionPreference = 'Stop'
Import-Module (Join-Path $PSScriptRoot 'VeyraProject.psm1') -Force
$project = Get-VeyraProjectFile
$engine = Resolve-VeyraEngineRoot -ProjectFile $project -EngineRoot $EngineRoot
$game = Split-Path -Parent $project
$output = Join-Path $game 'Saved/PresentationReview'
New-Item -ItemType Directory -Force -Path $output | Out-Null
$manifest = Join-Path $output 'manifest.json'
foreach ($stale in @($manifest, (Join-Path $output 'failure.txt'))) {
    if (Test-Path -LiteralPath $stale) { Remove-Item -LiteralPath $stale }
}
$env:VEYRA_PRESENTATION_REVIEW_OUTPUT = $output
$script = Join-Path $PSScriptRoot 'CapturePresentation.py'
$editor = Join-Path $engine 'Engine/Binaries/Win64/UnrealEditor.exe'
$arguments = @("`"$project`"", "-ExecutePythonScript=`"$script`"", '-EnablePlugins=PythonScriptPlugin', '-unattended', '-nosplash', '-nosound', '-RenderOffscreen', '-windowed', '-ResX=1920', '-ResY=1080', "-ABSLOG=`"$output/Capture.log`"")
$process = Start-Process -FilePath $editor -ArgumentList $arguments -WindowStyle Hidden -PassThru -Wait
# The manifest is written only once every view is captured; the editor's own exit code on quitting is not reliable.
if (-not (Test-Path -LiteralPath $manifest) -or (Get-Content -LiteralPath $manifest -Raw | ConvertFrom-Json).status -ne 'captured') { throw "Capture failed (editor exit code $($process.ExitCode)). Inspect $output/Capture.log and failure.txt." }
Write-Host "Presentation review images and manifest: $output"
