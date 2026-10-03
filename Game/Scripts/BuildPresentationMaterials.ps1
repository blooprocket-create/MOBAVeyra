#Requires -Version 7.0
<#
.SYNOPSIS
    Builds the presentation's generated materials (ADR-063), such as the hit flash overlay.
.DESCRIPTION
    Runs BuildPresentationMaterials.py in an editor commandlet with the project's editor built. It reads
    Game/ArtSource/Presentation/PresentationMaterials.json and writes each material under
    /Game/Veyra/UI/Presentation. Regenerate rather than hand-edit them. An existing material is a binary
    asset: acquire its Git LFS lock before rebuilding it (ADR-006 section 9).
#>
[CmdletBinding()]
param(
    [string]$EngineRoot
)
$ErrorActionPreference = 'Stop'
Import-Module (Join-Path $PSScriptRoot 'VeyraProject.psm1') -Force
$project = Get-VeyraProjectFile
$game = Split-Path -Parent $project
$engine = Resolve-VeyraEngineRoot -ProjectFile $project -EngineRoot $EngineRoot
$saved = Join-Path $game 'Saved/PresentationMaterials'
New-Item -ItemType Directory -Force -Path $saved | Out-Null
$editor = Join-Path $engine 'Engine/Binaries/Win64/UnrealEditor-Cmd.exe'
$script = Join-Path $PSScriptRoot 'BuildPresentationMaterials.py'
$log = Join-Path $saved 'BuildPresentationMaterials.log'
& $editor $project '-run=pythonscript' "-script=$script" '-EnablePlugins=PythonScriptPlugin' '-unattended' '-nullrhi' '-nosplash' '-nosound' "-ABSLOG=$log" *> (Join-Path $saved 'BuildPresentationMaterials-console.log')
if ($LASTEXITCODE -ne 0 -or -not (Select-String -LiteralPath $log -SimpleMatch 'VEYRA_PRESENTATION_MATERIALS_PASSED' -Quiet)) {
    throw "The presentation materials were not built. See $log"
}
Select-String -LiteralPath $log -Pattern 'VEYRA_PRESENTATION_MATERIAL: .*' | ForEach-Object { Write-Host $_.Matches[0].Value }
