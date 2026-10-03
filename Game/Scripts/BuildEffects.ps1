#Requires -Version 7.0
<#
.SYNOPSIS
    Builds the presentation's Niagara effects (ADR-063 section 4) from Game/ArtSource/Presentation/Effects.json.
.DESCRIPTION
    Runs the VeyraEffects commandlet in the project's built editor. Each system is rebuilt from the engine
    template its spec names, with every particle colour input linked to the spec's user colour. Regenerate
    rather than hand-edit them. An existing system is a binary asset: acquire its Git LFS lock before
    rebuilding it (ADR-006 section 9).
.PARAMETER Describe
    Changes nothing: writes each template's topology to Game/Saved/Effects, to write a spec against.
.PARAMETER Effects
    Builds only the systems named, as the spec names them; without it, every system.
#>
[CmdletBinding()]
param(
    [switch]$Describe,
    [ValidatePattern('^[A-Za-z0-9_]+$')]
    [string[]]$Effects,
    [string]$EngineRoot
)
$ErrorActionPreference = 'Stop'
Import-Module (Join-Path $PSScriptRoot 'VeyraProject.psm1') -Force
$project = Get-VeyraProjectFile
$game = Split-Path -Parent $project
$engine = Resolve-VeyraEngineRoot -ProjectFile $project -EngineRoot $EngineRoot
$saved = Join-Path $game 'Saved/Effects'
New-Item -ItemType Directory -Force -Path $saved | Out-Null
$editor = Join-Path $engine 'Engine/Binaries/Win64/UnrealEditor-Cmd.exe'
$log = Join-Path $saved 'BuildEffects.log'
[string[]]$mode = if ($Describe) { '-Describe' } else { @() }
if ($Effects) {
    $mode += "-Only=$($Effects -join ',')"
}
# Shaders compile for the systems' sprites, so the RHI stays on.
& $editor $project '-run=VeyraEffects' '-unattended' '-nosplash' '-nosound' "-ABSLOG=$log" @mode *> (Join-Path $saved 'BuildEffects-console.log')
if ($LASTEXITCODE -ne 0 -or -not (Select-String -LiteralPath $log -SimpleMatch 'VEYRA_EFFECTS_PASSED' -Quiet)) {
    throw "The effects were not built. See $log"
}
Select-String -LiteralPath $log -Pattern 'VEYRA_EFFECT(_DESCRIBED)?: .*' | ForEach-Object { Write-Host $_.Matches[0].Value }
