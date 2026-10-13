#Requires -Version 7.0
<#
.SYNOPSIS
    Builds and captures the ADR-073 §6 prototype slice: Team A's keep, top lane and top inner and outer jungle, with
    masses extruded by Geometry Script from the plan's exact polygons.
.DESCRIPTION
    Writes Game/Saved/CrucibleSlice/slice.json from plan.py (python with shapely and numpy), then opens the editor with
    GeometryScripting enabled for this run only, builds a throwaway level from it and captures each gameplay-camera view
    to Game/Saved/CrucibleSlice/Captures. No asset is saved. Needs an up-to-date VeyraEditor build.
#>
[CmdletBinding()]
param([string]$EngineRoot)
$ErrorActionPreference = 'Stop'
$repo = Resolve-Path (Join-Path $PSScriptRoot '../../../..')
Import-Module (Join-Path $repo 'Game/Scripts/VeyraProject.psm1') -Force
$project = Get-VeyraProjectFile
$engine = Resolve-VeyraEngineRoot -ProjectFile $project -EngineRoot $EngineRoot
$game = Split-Path -Parent $project
$output = Join-Path $game 'Saved/CrucibleSlice/Captures'
New-Item -ItemType Directory -Force -Path $output | Out-Null
$manifest = Join-Path $output 'manifest.json'
if (Test-Path -LiteralPath $manifest) { Remove-Item -LiteralPath $manifest }

python (Join-Path $PSScriptRoot 'slice_data.py')
if ($LASTEXITCODE -ne 0) { throw 'slice_data.py failed' }

$script = Join-Path $PSScriptRoot 'BuildCrucibleSlice.py'
$editor = Join-Path $engine 'Engine/Binaries/Win64/UnrealEditor.exe'
$arguments = @("`"$project`"", "-ExecutePythonScript=`"$script`"", '-EnablePlugins=PythonScriptPlugin,GeometryScripting',
    '-unattended', '-nosplash', '-nosound', '-RenderOffscreen', '-NoTextureStreaming', '-windowed', '-ResX=1920', '-ResY=1080',
    "-ABSLOG=`"$output/Capture.log`"")
$process = Start-Process -FilePath $editor -ArgumentList $arguments -WindowStyle Hidden -PassThru
$process.WaitForExit()
if (-not (Test-Path -LiteralPath $manifest) -or (Get-Content -LiteralPath $manifest -Raw | ConvertFrom-Json).status -ne 'captured') {
    throw "Slice capture failed (editor exit code $($process.ExitCode)). Inspect $output/Capture.log and failure.txt."
}
Write-Host "Slice captures: $output"
