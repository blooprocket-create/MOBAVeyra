#Requires -Version 7.0
<#
.SYNOPSIS
    Builds the Vanguards' generated, rigged and animated first-pass bodies (ADR-064).
.DESCRIPTION
    GenerateVanguardBodies.py runs in background Blender and writes one FBX per Vanguard to
    Game/ArtSource/Vanguards/FBX with a manifest of their hashes, from Game/ArtSource/Vanguards/VanguardKit.json
    and each Vanguard's capsule in Game/Tuning/Vanguards.json. ImportVanguardBodies.py then imports them in an editor
    commandlet as skeletal meshes, skeletons and animation sequences under /Game/Veyra/Vanguards. Regenerate rather
    than hand-edit them. An existing asset is a binary asset: acquire its Git LFS lock before reimporting it
    (ADR-006 section 9).
.PARAMETER Vanguards
    Builds only the Vanguards named, by ID; without it, every Vanguard in the kit.
.PARAMETER Blender
    The Blender 5.2 executable; without it, blender on PATH or Blender 5.2's default install.
.PARAMETER Preview
    Also renders each body in its key poses to Game/Saved/VanguardKit/Preview for review.
.PARAMETER ImportOnly
    Imports the FBX already written, without running Blender.
#>
[CmdletBinding()]
param(
    [ValidatePattern('^[a-z0-9_]+$')]
    [string[]]$Vanguards,
    [string]$Blender,
    [switch]$Preview,
    [switch]$ImportOnly,
    [string]$EngineRoot
)
$ErrorActionPreference = 'Stop'
Import-Module (Join-Path $PSScriptRoot 'VeyraProject.psm1') -Force
$project = Get-VeyraProjectFile
$game = Split-Path -Parent $project
$engine = Resolve-VeyraEngineRoot -ProjectFile $project -EngineRoot $EngineRoot
$saved = Join-Path $game 'Saved/VanguardKit'
New-Item -ItemType Directory -Force -Path $saved | Out-Null
if (-not $ImportOnly) {
    if (-not $Blender) {
        $command = Get-Command blender -ErrorAction SilentlyContinue
        $default = 'C:\Program Files\Blender Foundation\Blender 5.2\blender.exe'
        $Blender = if ($command) { $command.Source } elseif (Test-Path -LiteralPath $default) { $default } else { throw 'Pass -Blender <path to blender.exe> or add Blender 5.2 to PATH.' }
    }
    [string[]]$only = @('--') + $(if ($Vanguards) { @('--only', ($Vanguards -join ',')) } else { @() }) + $(if ($Preview) { @('--preview') } else { @() })
    & $Blender --background --factory-startup --python-exit-code 1 --python (Join-Path $PSScriptRoot 'GenerateVanguardBodies.py') @only *> (Join-Path $saved 'Blender.log')
    if ($LASTEXITCODE -ne 0 -or -not (Select-String -LiteralPath (Join-Path $saved 'Blender.log') -SimpleMatch 'VEYRA_VANGUARD_BODIES_PASSED' -Quiet)) {
        throw "Blender failed. See $saved/Blender.log"
    }
    Select-String -LiteralPath (Join-Path $saved 'Blender.log') -Pattern 'VEYRA_VANGUARD_BODY: .*' | ForEach-Object { Write-Host $_.Matches[0].Value }
}
$editor = Join-Path $engine 'Engine/Binaries/Win64/UnrealEditor-Cmd.exe'
$script = Join-Path $PSScriptRoot 'ImportVanguardBodies.py'
$log = Join-Path $saved 'Import.log'
[string[]]$importOnly = if ($Vanguards) { @("-VeyraOnly=$($Vanguards -join ',')") } else { @() }
& $editor $project '-run=pythonscript' "-script=$script" '-EnablePlugins=PythonScriptPlugin' '-unattended' '-nullrhi' '-nosplash' '-nosound' '-ExecCmds=Interchange.FeatureFlags.Import.FBX 0' "-ABSLOG=$log" @importOnly *> (Join-Path $saved 'Import-console.log')
if ($LASTEXITCODE -ne 0 -or -not (Select-String -LiteralPath $log -SimpleMatch 'VEYRA_VANGUARD_BODIES_IMPORTED' -Quiet)) {
    throw "The Vanguard bodies were not imported. See $log"
}
Select-String -LiteralPath $log -Pattern 'VEYRA_VANGUARD_BODY_ASSET: .*' | ForEach-Object { Write-Host $_.Matches[0].Value }
