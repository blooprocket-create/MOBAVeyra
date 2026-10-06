#Requires -Version 7.0
<#
.SYNOPSIS
    Generates and imports the provisional Fluxborn meshes.
.DESCRIPTION
    Uses a separate background Blender and an editor commandlet. Does not modify maps,
    gameplay code or project plugins. Existing binary assets require Git LFS locks before
    reimport, per ADR-006 section 9. Review the source README before replacing assets.
.PARAMETER Materials
    Rebuilds only the kit's materials named (such as M_FluxbornFlux) from FluxbornKit.json,
    without running Blender, importing any mesh or checking the meshes against their source.
#>
[CmdletBinding()]
param(
    [string]$Blender,
    [string]$EngineRoot,
    [switch]$ImportOnly,
    [switch]$VerifyOnly,
    [ValidatePattern('^[A-Za-z0-9_]+$')]
    [string[]]$Materials
)
$ErrorActionPreference = 'Stop'
Import-Module (Join-Path $PSScriptRoot 'VeyraProject.psm1') -Force
$project = Get-VeyraProjectFile
$game = Split-Path -Parent $project
$engine = Resolve-VeyraEngineRoot -ProjectFile $project -EngineRoot $EngineRoot
$saved = Join-Path $game 'Saved/FluxbornKit'
New-Item -ItemType Directory -Force -Path $saved | Out-Null
if ($VerifyOnly -and $Materials) { throw 'Pass -VerifyOnly or -Materials, not both.' }
if (-not $ImportOnly -and -not $VerifyOnly -and -not $Materials) {
    if (-not $Blender) {
        $command = Get-Command blender -ErrorAction SilentlyContinue
        if (-not $command) { throw 'Pass -Blender <path to blender.exe> or add Blender to PATH.' }
        $Blender = $command.Source
    }
    & $Blender --background --factory-startup --python-exit-code 1 --python (Join-Path $PSScriptRoot 'GenerateFluxbornMeshes.py') *> (Join-Path $saved 'Blender.log')
    if ($LASTEXITCODE -ne 0) { throw "Blender failed. See $saved/Blender.log" }
}
$editor = Join-Path $engine 'Engine/Binaries/Win64/UnrealEditor-Cmd.exe'
$script = Join-Path $PSScriptRoot 'ImportFluxbornMeshes.py'
$log = Join-Path $saved $(if ($VerifyOnly) { 'Verify.log' } else { 'Import.log' })
$validation = Join-Path $saved $(if ($VerifyOnly) { 'unreal-verify.json' } else { 'unreal-validation.json' })
[string[]]$verifyArguments = if ($VerifyOnly) { @('-VeyraArtVerifyOnly') } else { @() }
[string[]]$named = if ($Materials) { @("-VeyraMaterials=$($Materials -join ',')") } else { @() }
if (Test-Path -LiteralPath $validation) { Remove-Item -LiteralPath $validation }
& $editor $project @verifyArguments '-run=pythonscript' "-script=$script" '-EnablePlugins=PythonScriptPlugin' '-unattended' '-nullrhi' '-nosplash' '-nosound' '-ExecCmds=Interchange.FeatureFlags.Import.FBX 0' "-ABSLOG=$log" @named *> (Join-Path $saved 'Import-console.log')
if ($LASTEXITCODE -ne 0 -or -not (Test-Path -LiteralPath $validation)) { throw "Fluxborn import failed. See $log" }
Select-String -LiteralPath $log -Pattern 'VEYRA_KIT_MATERIAL: .*' | ForEach-Object { Write-Host $_.Matches[0].Value }
Write-Host $(if ($Materials) { "Fluxborn materials rebuilt. Validation: $saved" } else { "Six Fluxborn meshes validated. Preview and validation: $saved" })
