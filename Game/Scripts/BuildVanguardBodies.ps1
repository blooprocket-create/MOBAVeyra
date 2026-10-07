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
    A body rebuilt as it was (its content unchanged) keeps its FBX and its imported assets; only the Vanguards whose
    bodies changed are imported again. A full build after a change to the generator's code is therefore cheap, and the
    preflight refuses bodies built by other code or another Blender (VanguardBodies/inputs.py). What changed is counted
    since the last import, not the last generator run: bodies generated on their own (a preview, a failed import) are
    imported by the next build that imports.
.PARAMETER Vanguards
    Builds only the Vanguards named, by ID; without it, every Vanguard in the kit.
.PARAMETER Blender
    The Blender 5.2 executable; without it, blender on PATH or Blender 5.2's default install.
.PARAMETER Preview
    Also renders each body in its key poses to Game/Saved/VanguardKit/Preview for review.
.PARAMETER ImportOnly
    Imports the FBX already written, without running Blender. As a full build does, it deletes the imported assets of
    bodies the generator dropped and imports what the generator left to import, or only the Vanguards it names.
#>
[CmdletBinding()]
param(
    [ValidatePattern('^[a-z0-9_]+(,[a-z0-9_]+)*$')]
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
$importNone = $false
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
    Select-String -LiteralPath (Join-Path $saved 'Blender.log') -Pattern 'VEYRA_VANGUARD_BODY(_UNCHANGED|_REMOVED)?: .*' | ForEach-Object { Write-Host $_.Matches[0].Value }
}
# What the generator has left to import and to delete since the last import, however it ran (it adds to these until an
# import takes them): the Vanguards whose bodies changed, and the bodies it dropped (removed or renamed in the kit),
# whose FBX went with them and whose imported assets go below, -ImportOnly or not.
$changedList = Join-Path $saved 'changed.json'
$removedList = Join-Path $saved 'removed.json'
$pending = @(if (Test-Path -LiteralPath $changedList) { Get-Content -LiteralPath $changedList -Raw | ConvertFrom-Json })
$removedNames = @(if (Test-Path -LiteralPath $removedList) { Get-Content -LiteralPath $removedList -Raw | ConvertFrom-Json })
if (-not $ImportOnly -or -not $Vanguards) {
    # A body rebuilt as it was kept its FBX, and keeps its imported assets: only the Vanguards whose bodies changed are
    # imported again (the art set is written whatever changed). -ImportOnly takes the same unless it names Vanguards.
    $Vanguards = $pending
    $importNone = $Vanguards.Count -eq 0
}
# Before anything is removed: every body the art set will hold was made from today's kit, or nothing is imported.
$python = if (Get-Command python3 -ErrorAction SilentlyContinue) { 'python3' } else { 'python' }
& $python (Join-Path $PSScriptRoot 'VanguardBodies/inputs.py') $game
if ($LASTEXITCODE -ne 0) { throw 'Stale Vanguard bodies: regenerate them before importing (see above). Nothing was removed or imported.' }
# A regenerated body may have new bones or stand differently, which a reimport onto its old skeleton does not take: its
# previous assets go, and it imports fresh. They are generated output; acquire their Git LFS locks first (ADR-006 section 9).
$kit = Get-Content (Join-Path $game 'ArtSource/Vanguards/VanguardKit.json') -Raw | ConvertFrom-Json
$manifest = Get-Content (Join-Path $game 'ArtSource/Vanguards/manifest.json') -Raw | ConvertFrom-Json
$selected = @($Vanguards | ForEach-Object { $_ -split ',' } | Where-Object { $_ })
$destination = Join-Path $game ('Content' + $kit.destination.Substring('/Game'.Length))
$replaced = @()
foreach ($asset in $manifest.assets) {
    if (($selected.Count -gt 0 -or $importNone) -and $asset.id -notin $selected) { continue }
    $folder = Join-Path $destination $asset.name.Substring('SK_'.Length)
    if (Test-Path -LiteralPath $folder) { $replaced += @(Get-ChildItem -LiteralPath $folder -Filter *.uasset) }
}
$dropped = @($removedNames | ForEach-Object { Join-Path $destination $_.Substring('SK_'.Length) } | Where-Object { Test-Path -LiteralPath $_ })
foreach ($folder in $dropped) { $replaced += @(Get-ChildItem -LiteralPath $folder -Filter *.uasset) }
# Every import also rewrites the bodies' material, their toon light and the art set (ImportVanguardBodies.py). Each file
# it will write is checked before any is removed, so a missing lock stops the build with nothing changed.
$shared = @('M_VeyraVanguardBody.uasset', "$($kit.bodyMaterial.toon.lightCollection).uasset", 'DA_VanguardArt.uasset') | ForEach-Object { Join-Path $destination $_ } |
    Where-Object { Test-Path -LiteralPath $_ } | ForEach-Object { Get-Item -LiteralPath $_ }
$locked = @(@($replaced) + @($shared) | Where-Object { $_.IsReadOnly })
if ($locked.Count -gt 0) {
    throw "Acquire the Git LFS locks before reimporting; nothing was removed or imported:`n$(($locked | ForEach-Object FullName) -join "`n")"
}
foreach ($file in $replaced) { Remove-Item -LiteralPath $file.FullName }
foreach ($folder in $dropped) { Remove-Item -LiteralPath $folder -Recurse }
$editor = Join-Path $engine 'Engine/Binaries/Win64/UnrealEditor-Cmd.exe'
$script = Join-Path $PSScriptRoot 'ImportVanguardBodies.py'
$log = Join-Path $saved 'Import.log'
# Named apart from -ImportOnly: PowerShell's names ignore case, so one called importOnly would overwrite the switch.
[string[]]$veyraOnly = if ($selected.Count -gt 0 -or $importNone) { @("-VeyraOnly=$($selected -join ',')") } else { @() }
& $editor $project '-run=pythonscript' "-script=$script" '-EnablePlugins=PythonScriptPlugin' '-unattended' '-nullrhi' '-nosplash' '-nosound' '-ExecCmds=Interchange.FeatureFlags.Import.FBX 0' "-ABSLOG=$log" @veyraOnly *> (Join-Path $saved 'Import-console.log')
if ($LASTEXITCODE -ne 0 -or -not (Select-String -LiteralPath $log -SimpleMatch 'VEYRA_VANGUARD_BODIES_IMPORTED' -Quiet)) {
    throw "The Vanguard bodies were not imported. See $log"
}
# Imported and deleted: what was waiting is taken, all of it but what an -ImportOnly naming some Vanguards left out.
$left = @(if ($selected.Count -gt 0) { $pending | Where-Object { $_ -notin $selected } })
Set-Content -LiteralPath $changedList -Value (ConvertTo-Json -InputObject $left -Compress)
Set-Content -LiteralPath $removedList -Value '[]'
Select-String -LiteralPath $log -Pattern 'VEYRA_VANGUARD_BODY_ASSET: .*' | ForEach-Object { Write-Host $_.Matches[0].Value }
