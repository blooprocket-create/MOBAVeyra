#Requires -Version 7.0
<#
.SYNOPSIS
    Generates and imports the provisional Crucible structure meshes.
.DESCRIPTION
    Uses a separate background Blender and an editor commandlet. Does not modify maps,
    gameplay code or project plugins. Existing binary assets require Git LFS locks before
    reimport, per ADR-006 section 9. Review the source README before replacing assets.
.PARAMETER Materials
    Rebuilds only the kit's materials named (such as M_CrucibleFlux) from StructureKit.json,
    without running Blender, importing any mesh or checking the meshes against their source.
#>
[CmdletBinding()]
param(
    [string]$Blender,
    [string]$EngineRoot,
    [switch]$ImportOnly,
    [ValidatePattern('^[A-Za-z0-9_]+$')]
    [string[]]$Materials
)
$ErrorActionPreference = 'Stop'
Import-Module (Join-Path $PSScriptRoot 'VeyraProject.psm1') -Force
$project = Get-VeyraProjectFile
$game = Split-Path -Parent $project
$engine = Resolve-VeyraEngineRoot -ProjectFile $project -EngineRoot $EngineRoot
$saved = Join-Path $game 'Saved/StructureKit'
New-Item -ItemType Directory -Force -Path $saved | Out-Null
if (-not $ImportOnly -and -not $Materials) {
    if (-not $Blender) {
        $command = Get-Command blender -ErrorAction SilentlyContinue
        if (-not $command) { throw 'Pass -Blender <path to blender.exe> or add Blender to PATH.' }
        $Blender = $command.Source
    }
    & $Blender --background --factory-startup --python-exit-code 1 --python (Join-Path $PSScriptRoot 'GenerateStructureMeshes.py') *> (Join-Path $saved 'Blender.log')
    if ($LASTEXITCODE -ne 0) { throw "Blender failed. See $saved/Blender.log" }
}
$editor = Join-Path $engine 'Engine/Binaries/Win64/UnrealEditor-Cmd.exe'
$script = Join-Path $PSScriptRoot 'ImportStructureMeshes.py'
$log = Join-Path $saved 'Import.log'
$validation = Join-Path $saved 'unreal-validation.json'
if (Test-Path -LiteralPath $validation) { Remove-Item -LiteralPath $validation }
[string[]]$named = if ($Materials) { @("-VeyraMaterials=$($Materials -join ',')") } else { @() }
& $editor $project '-run=pythonscript' "-script=$script" '-EnablePlugins=PythonScriptPlugin' '-unattended' '-nullrhi' '-nosplash' '-nosound' '-ExecCmds=Interchange.FeatureFlags.Import.FBX 0' "-ABSLOG=$log" @named *> (Join-Path $saved 'Import-console.log')
if ($LASTEXITCODE -ne 0 -or -not (Test-Path -LiteralPath $validation)) { throw "Structure import failed. See $log" }
Select-String -LiteralPath $log -Pattern 'VEYRA_STRUCTURE_MATERIAL: .*' | ForEach-Object { Write-Host $_.Matches[0].Value }
Write-Host $(if ($Materials) { "Structure materials rebuilt. Validation: $saved" } else { "Eight structure meshes imported and validated. Preview and validation: $saved" })
