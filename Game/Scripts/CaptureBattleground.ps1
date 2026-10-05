#Requires -Version 7.0
<# .SYNOPSIS
Captures the generated Crucible's named review cameras with the pinned editor renderer.
#>
[CmdletBinding()]
param([string]$EngineRoot, [ValidateSet('high','low')][string]$Profile = 'high', [string]$Views = '')
$ErrorActionPreference = 'Stop'
Import-Module (Join-Path $PSScriptRoot 'VeyraProject.psm1') -Force
$project = Get-VeyraProjectFile
$engine = Resolve-VeyraEngineRoot -ProjectFile $project -EngineRoot $EngineRoot
$game = Split-Path -Parent $project
$output = Join-Path $game "Saved/WorldReview/$Profile"
New-Item -ItemType Directory -Force -Path $output | Out-Null
$manifest = Join-Path $output 'manifest.json'
if (Test-Path -LiteralPath $manifest) { Remove-Item -LiteralPath $manifest }
$env:VEYRA_REVIEW_PROFILE = $Profile
$env:VEYRA_REVIEW_VIEWS = $Views
$script = Join-Path $game 'Plugins/VeyraWorldTools/Scripts/CaptureCrucible.py'
$editor = Join-Path $engine 'Engine/Binaries/Win64/UnrealEditor.exe'
$arguments = @("`"$project`"", "-ExecutePythonScript=`"$script`"", '-EnablePlugins=PythonScriptPlugin', '-unattended', '-nosplash', '-nosound', '-RenderOffscreen', '-NoTextureStreaming', '-windowed', '-ResX=1920', '-ResY=1080', "-ABSLOG=`"$output/Capture.log`"")
$process = Start-Process -FilePath $editor -ArgumentList $arguments -WindowStyle Hidden -PassThru -Wait
# The manifest is written only once every view is captured; the editor's own exit code on quitting is not reliable.
if (-not (Test-Path -LiteralPath $manifest) -or (Get-Content -LiteralPath $manifest -Raw | ConvertFrom-Json).status -ne 'captured') { throw "Capture failed (editor exit code $($process.ExitCode)). Inspect $output/Capture.log and failure.txt." }
Write-Host "Review images and manifest: $output"
