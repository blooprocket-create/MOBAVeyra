#Requires -Version 7.0
<#
.SYNOPSIS
    Captures the generated Crucible's named review cameras with the pinned editor renderer.
.DESCRIPTION
    Opens L_Battleground in the editor, pilots each named camera (Review_* beauty views and Play_* views
    at the player's camera settings) and saves a 1920x1080 image of each, with a manifest
    (World Validation Standard §13). No map asset is edited.
.PARAMETER Profile
    The graphics profile: high (default) or low.
.PARAMETER Views
    Camera names to capture, comma-separated, without the Review_ prefix (Play_ views keep theirs);
    every named camera when empty.
.PARAMETER Mode
    Lit (default): the scene as players see it, into Saved/WorldReview/<Profile>.
    Collision: what blocks a pawn, in which generated dressing must not appear.
    Dressing: the terrain hidden, so only the generated instances and the river remain.
    Value: the lit scene in luminance only, to judge the value hierarchy.
    Every mode but Lit goes into Saved/WorldReview/<Profile>/<Mode>. The navigation view is
    ValidateBattleground.ps1's map, drawn with the server's walls.
#>
[CmdletBinding()]
param(
    [string]$EngineRoot,
    [ValidateSet('high', 'low')][string]$Profile = 'high',
    [string]$Views = '',
    [ValidateSet('Lit', 'Collision', 'Dressing', 'Value')][string]$Mode = 'Lit'
)
$ErrorActionPreference = 'Stop'
Import-Module (Join-Path $PSScriptRoot 'VeyraProject.psm1') -Force
$project = Get-VeyraProjectFile
$engine = Resolve-VeyraEngineRoot -ProjectFile $project -EngineRoot $EngineRoot
$game = Split-Path -Parent $project
$output = Join-Path $game "Saved/WorldReview/$Profile"
if ($Mode -ne 'Lit') {
    $output = Join-Path $output $Mode
}
New-Item -ItemType Directory -Force -Path $output | Out-Null
$manifest = Join-Path $output 'manifest.json'
if (Test-Path -LiteralPath $manifest) { Remove-Item -LiteralPath $manifest }
$env:VEYRA_REVIEW_PROFILE = $Profile
$env:VEYRA_REVIEW_VIEWS = $Views
$env:VEYRA_REVIEW_OUTPUT = $output
# Value is the lit scene, reduced to luminance below.
$env:VEYRA_REVIEW_MODE = $(if ($Mode -eq 'Value') { 'Lit' } else { $Mode })
$script = Join-Path $game 'Plugins/VeyraWorldTools/Scripts/CaptureCrucible.py'
$editor = Join-Path $engine 'Engine/Binaries/Win64/UnrealEditor.exe'
$arguments = @("`"$project`"", "-ExecutePythonScript=`"$script`"", '-EnablePlugins=PythonScriptPlugin', '-unattended', '-nosplash', '-nosound', '-RenderOffscreen', '-NoTextureStreaming', '-windowed', '-ResX=1920', '-ResY=1080', "-ABSLOG=`"$output/Capture.log`"")
$process = Start-Process -FilePath $editor -ArgumentList $arguments -WindowStyle Hidden -PassThru
$process.WaitForExit()
# The manifest is written only once every view is captured; the editor's own exit code on quitting is not reliable.
if (-not (Test-Path -LiteralPath $manifest) -or (Get-Content -LiteralPath $manifest -Raw | ConvertFrom-Json).status -ne 'captured') { throw "Capture failed (editor exit code $($process.ExitCode)). Inspect $output/Capture.log and failure.txt." }
if ($Mode -eq 'Value') {
    # Rec. 709 luma: how light each place reads, without its hue.
    $convert = "import sys, pathlib`nfrom PIL import Image`nfor path in pathlib.Path(sys.argv[1]).glob('*.png'):`n    Image.open(path).convert('RGB').convert('L', (0.2126, 0.7152, 0.0722, 0)).save(path)"
    python3 -c $convert $output
    if ($LASTEXITCODE -ne 0) { throw 'Could not reduce the captures to luminance (python3 with Pillow).' }
}
Write-Host "Review images and manifest: $output"
