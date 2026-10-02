#Requires -Version 7.0
<#
.SYNOPSIS
    Writes the art kits' unit art sets: the Data Assets the grey-box presentation reads each kit's meshes from.
.DESCRIPTION
    Runs BuildUnitArtSets.py in an editor commandlet with the project's editor built. It reads each kit's
    manifest under Game/ArtSource and the meshes the kit's import made, and changes no mesh or material.
    An existing set is a binary asset: acquire its Git LFS lock before rewriting it (ADR-006 section 9).
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
$saved = Join-Path $game 'Saved/UnitArtSets'
New-Item -ItemType Directory -Force -Path $saved | Out-Null
$editor = Join-Path $engine 'Engine/Binaries/Win64/UnrealEditor-Cmd.exe'
$script = Join-Path $PSScriptRoot 'BuildUnitArtSets.py'
$log = Join-Path $saved 'BuildUnitArtSets.log'
& $editor $project '-run=pythonscript' "-script=$script" '-EnablePlugins=PythonScriptPlugin' '-unattended' '-nullrhi' '-nosplash' '-nosound' "-ABSLOG=$log" *> (Join-Path $saved 'BuildUnitArtSets-console.log')
if ($LASTEXITCODE -ne 0 -or -not (Select-String -LiteralPath $log -SimpleMatch 'VEYRA_ART_SETS_PASSED' -Quiet)) {
    throw "The art sets were not written. See $log"
}
Select-String -LiteralPath $log -Pattern 'VEYRA_ART_SET: .*' | ForEach-Object { Write-Host $_.Matches[0].Value }
