#Requires -Version 7.0
<#
.SYNOPSIS
    Regenerates the profile backgrounds' region panoramas from their source art.
.DESCRIPTION
    Copies each region panorama, ConceptArt/ProfileBackgrounds/<region-name>.png, to <region_name>.png (the
    catalog's region ID: lowercase, words joined by underscores), then runs the VeyraVanguardArt commandlet
    headless with -Kind=Backgrounds, which saves each as a UI texture in the shell style's BackgroundArtFolder
    (Game/Content/Veyra/UI/Backgrounds), replacing the file (ADR-082 §10). The backend's profile catalog
    names each as region_<region_name>.

    The editor must already be built (Build.ps1 -Target VeyraEditor). The textures are lockable Git LFS
    files (ADR-006 §9): lock them before committing new versions. The log goes to
    Game/Saved/Logs/BuildBackgroundArt.log.
.PARAMETER Regions
    Only these regions, by ID, such as those newly painted: the textures already committed are read-only
    until locked, so leave them out unless their art changed and they are locked.
.PARAMETER EngineRoot
    Engine folder to use instead of the one registered for the project's EngineAssociation.
.EXAMPLE
    ./Game/Scripts/BuildBackgroundArt.ps1
.EXAMPLE
    ./Game/Scripts/BuildBackgroundArt.ps1 -Regions iron_march
#>
[CmdletBinding()]
param(
    [string[]]$Regions = @(),
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
$sourceDir = Join-Path $repository 'ConceptArt\ProfileBackgrounds'
$staged = Join-Path $gameDir 'Intermediate\BackgroundArt'
if (Test-Path -LiteralPath $staged) {
    Remove-Item -LiteralPath $staged -Recurse -Force
}
New-Item -ItemType Directory -Force -Path $staged | Out-Null
$copied = 0
foreach ($file in Get-ChildItem -LiteralPath $sourceDir -Filter '*.png') {
    $id = $file.BaseName.ToLowerInvariant() -replace '[^a-z0-9]+', '_'
    if ($Regions.Count -gt 0 -and $id -notin $Regions) {
        continue
    }
    Copy-Item -LiteralPath $file.FullName -Destination (Join-Path $staged "$id.png")
    $copied++
}
if ($copied -eq 0) {
    Write-Host "No region panorama to import from $sourceDir."
    exit 1
}

$logFile = Join-Path $gameDir 'Saved\Logs\BuildBackgroundArt.log'
New-Item -ItemType Directory -Force -Path (Split-Path -Parent $logFile) | Out-Null
$arguments = @(
    "`"$projectFile`""
    '-run=VeyraVanguardArt'
    '-Kind=Backgrounds'
    "-Source=`"$staged`""
    "-ABSLOG=`"$logFile`""
    '-unattended'
    '-nullrhi'
    '-nosplash'
    '-nosound'
) -join ' '

Write-Host "Importing $copied region panorama(s)."
$process = Start-Process -FilePath $editor -ArgumentList $arguments -NoNewWindow -PassThru -Wait
if ($process.ExitCode -ne 0) {
    Write-Host "The commandlet failed with exit code $($process.ExitCode). Log: $logFile"
    exit 1
}
Write-Host "Saved the region panoramas. Log: $logFile"
exit 0
