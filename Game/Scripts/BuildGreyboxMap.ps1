#Requires -Version 7.0
<#
.SYNOPSIS
    Regenerates the grey-box test map, or the skill showcase's stage, from its layout file.
.DESCRIPTION
    Runs the VeyraGreyboxMap commandlet headless. It reads
    Game/Source/VeyraDeveloper/Greybox/Greybox.json and saves
    Game/Content/Veyra/Developer/Maps/L_Greybox.umap, replacing the file. With -Showcase it reads
    Showcase.json and saves L_Showcase.umap instead, the stage RecordShowcase.ps1 records on (ADR-074 §1).
    The editor must already be built (Build.ps1 -Target VeyraEditor).

    The map is a lockable Git LFS file (ADR-006 §9): lock it before committing a new version.
    The log goes to Game/Saved/Logs/BuildGreyboxMap.log.
.PARAMETER Showcase
    Builds the skill showcase's stage instead of the test map.
.PARAMETER EngineRoot
    Engine folder to use instead of the one registered for the project's EngineAssociation.
.EXAMPLE
    ./Game/Scripts/BuildGreyboxMap.ps1
#>
[CmdletBinding()]
param(
    [switch]$Showcase,
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
$logFile = Join-Path $gameDir 'Saved\Logs\BuildGreyboxMap.log'
New-Item -ItemType Directory -Force -Path (Split-Path -Parent $logFile) | Out-Null

$arguments = @(
    "`"$projectFile`""
    '-run=VeyraGreyboxMap'
    $(if ($Showcase) { '-Layout=Showcase.json -Map=/Game/Veyra/Developer/Maps/L_Showcase' })
    "-ABSLOG=`"$logFile`""
    '-unattended'
    '-nullrhi'
    '-nosplash'
    '-nosound'
) -join ' '

$what = $(if ($Showcase) { "the skill showcase's stage" } else { 'the grey-box map' })
Write-Host "Building $what."
$process = Start-Process -FilePath $editor -ArgumentList $arguments -NoNewWindow -PassThru -Wait
if ($process.ExitCode -ne 0) {
    Write-Host "The commandlet failed with exit code $($process.ExitCode). Log: $logFile"
    exit 1
}
Write-Host "Saved $what. Log: $logFile"
exit 0
