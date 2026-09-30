#Requires -Version 7.0
<#
.SYNOPSIS
    Regenerates the items' or the abilities' icons from their source art.
.DESCRIPTION
    Copies each icon, <source>/<folder>/T_<id>_Icon.png, to <id>.png, then runs the VeyraVanguardArt
    commandlet headless with -Kind=<Kind>, which saves each as a UI texture, replacing the file:
    - Items, from ConceptArt/Items, into the shell style's ItemArtFolder (Game/Content/Veyra/UI/Items):
      the shop's tiles and the HUD's item bar.
    - Abilities, from ConceptArt/Skills (Flux Spells and Vanguards' passives and Q W E R), into the
      style's AbilityArtFolder (Game/Content/Veyra/UI/Abilities): the HUD's deck, champion select and
      the shop's Flux Spells.
    The editor must already be built (Build.ps1 -Target VeyraEditor). The textures are lockable Git LFS
    files (ADR-006 §9): lock them before committing new versions. The log goes to
    Game/Saved/Logs/BuildIconArt.log.
.PARAMETER Kind
    Items or Abilities.
.PARAMETER Ids
    Only these icons, such as those newly drawn: the textures already committed are read-only until
    locked, so leave them out unless their art changed and they are locked.
.PARAMETER EngineRoot
    Engine folder to use instead of the one registered for the project's EngineAssociation.
.EXAMPLE
    ./Game/Scripts/BuildIconArt.ps1 -Kind Items
.EXAMPLE
    ./Game/Scripts/BuildIconArt.ps1 -Kind Abilities -Ids blink, mend
#>
[CmdletBinding()]
param(
    [Parameter(Mandatory)]
    [ValidateSet('Items', 'Abilities')]
    [string]$Kind,
    [string[]]$Ids = @(),
    [string]$EngineRoot
)

$ErrorActionPreference = 'Stop'
Import-Module (Join-Path $PSScriptRoot 'VeyraProject.psm1') -Force

$projectFile = Get-VeyraProjectFile
$engineRoot = Resolve-VeyraEngineRoot -ProjectFile $projectFile -EngineRoot $EngineRoot
$editor = Join-Path $engineRoot 'Engine\Binaries\Win64\UnrealEditor-Cmd.exe'
if (-not (Test-Path -LiteralPath $editor -PathType Leaf)) {
    Write-Host "UnrealEditor-Cmd.exe was not found at '$editor'. Build the engine's editor first."
    exit 2
}

$gameDir = Split-Path -Parent $projectFile
$repository = Split-Path -Parent $gameDir
$sourceDir = Join-Path $repository ($Kind -eq 'Items' ? 'ConceptArt\Items' : 'ConceptArt\Skills')
$staged = Join-Path $gameDir "Intermediate\IconArt\$Kind"
if (Test-Path -LiteralPath $staged) {
    Remove-Item -LiteralPath $staged -Recurse -Force
}
New-Item -ItemType Directory -Force -Path $staged | Out-Null

# The commandlet names each texture after its file: T_<id>_Icon.png is staged as <id>.png.
$count = 0
foreach ($icon in Get-ChildItem -LiteralPath $sourceDir -Filter 'T_*_Icon.png' -Recurse) {
    $id = $icon.BaseName -replace '^T_', '' -replace '_Icon$', ''
    if ($Ids.Count -gt 0 -and $id -notin $Ids) {
        continue
    }
    Copy-Item -LiteralPath $icon.FullName -Destination (Join-Path $staged "$id.png")
    $count++
}
if ($count -eq 0) {
    Write-Host "No icon to import from '$sourceDir'."
    exit 1
}

$logFile = Join-Path $gameDir 'Saved\Logs\BuildIconArt.log'
New-Item -ItemType Directory -Force -Path (Split-Path -Parent $logFile) | Out-Null
$arguments = @(
    "`"$projectFile`""
    '-run=VeyraVanguardArt'
    "-Kind=$Kind"
    "-Source=`"$staged`""
    "-ABSLOG=`"$logFile`""
    '-unattended'
    '-nullrhi'
    '-nosplash'
    '-nosound'
) -join ' '

Write-Host "Importing $count $($Kind.ToLower()) icon(s)."
$process = Start-Process -FilePath $editor -ArgumentList $arguments -NoNewWindow -PassThru -Wait
if ($process.ExitCode -ne 0) {
    Write-Host "The commandlet failed with exit code $($process.ExitCode). Log: $logFile"
    exit 1
}
Write-Host "Saved the $($Kind.ToLower()) icons. Log: $logFile"
exit 0
