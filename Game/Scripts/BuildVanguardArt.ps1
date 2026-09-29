#Requires -Version 7.0
<#
.SYNOPSIS
    Regenerates the Vanguards' champion-select art from their hero illustrations.
.DESCRIPTION
    Converts each playable Vanguard's ConceptArt/Vanguards/<id>/hero.webp to PNG (ConvertVanguardArt.py,
    which needs Python with Pillow), then runs the VeyraVanguardArt commandlet headless, which saves
    each as a UI texture under Game/Content/Veyra/UI/Vanguards, replacing the file. The editor must
    already be built (Build.ps1 -Target VeyraEditor).

    The textures are lockable Git LFS files (ADR-006 §9): lock them before committing new versions.
    The log goes to Game/Saved/Logs/BuildVanguardArt.log.
.PARAMETER Vanguards
    Only these Vanguards' art, such as those newly released: the textures already committed are
    read-only until locked, so leave them out unless their art changed and they are locked.
.PARAMETER EngineRoot
    Engine folder to use instead of the one registered for the project's EngineAssociation.
.EXAMPLE
    ./Game/Scripts/BuildVanguardArt.ps1
.EXAMPLE
    ./Game/Scripts/BuildVanguardArt.ps1 -Vanguards kade, vera, mimzi
#>
[CmdletBinding()]
param(
    [string[]]$Vanguards = @(),
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
$converted = Join-Path $gameDir 'Intermediate\VanguardArt'
Write-Host 'Converting the hero illustrations.'
& python (Join-Path $PSScriptRoot 'ConvertVanguardArt.py') $repository $converted
if ($LASTEXITCODE -ne 0) {
    Write-Host 'Converting the hero illustrations failed.'
    exit 1
}

$logFile = Join-Path $gameDir 'Saved\Logs\BuildVanguardArt.log'
New-Item -ItemType Directory -Force -Path (Split-Path -Parent $logFile) | Out-Null
$arguments = @(
    "`"$projectFile`""
    '-run=VeyraVanguardArt'
    "-Source=`"$converted`""
    "-ABSLOG=`"$logFile`""
    '-unattended'
    '-nullrhi'
    '-nosplash'
    '-nosound'
) -join ' '

if ($Vanguards.Count -gt 0) {
    Get-ChildItem -LiteralPath $converted -Filter '*.png' | Where-Object { $_.BaseName -notin $Vanguards } | Remove-Item -Force
}
Write-Host 'Importing the Vanguard art.'
$process = Start-Process -FilePath $editor -ArgumentList $arguments -NoNewWindow -PassThru -Wait
if ($process.ExitCode -ne 0) {
    Write-Host "The commandlet failed with exit code $($process.ExitCode). Log: $logFile"
    exit 1
}
Write-Host "Saved the Vanguard art. Log: $logFile"
exit 0
