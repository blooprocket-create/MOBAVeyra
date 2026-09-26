#Requires -Version 7.0
<#
.SYNOPSIS
    Regenerates the grey-box test map from its layout file.
.DESCRIPTION
    Runs the VeyraGreyboxMap commandlet headless. It reads
    Game/Source/VeyraDeveloper/Greybox/Greybox.json and saves
    Game/Content/Veyra/Developer/Maps/L_Greybox.umap, replacing the file. The editor must already be
    built (Build.ps1 -Target VeyraEditor).

    The map is a lockable Git LFS file (ADR-006 §9): lock it before committing a new version.
    The log goes to Game/Saved/Logs/BuildGreyboxMap.log.
.PARAMETER EngineRoot
    Engine folder to use instead of the one registered for the project's EngineAssociation.
.EXAMPLE
    ./Game/Scripts/BuildGreyboxMap.ps1
#>
[CmdletBinding()]
param(
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
    "-ABSLOG=`"$logFile`""
    '-unattended'
    '-nullrhi'
    '-nosplash'
    '-nosound'
) -join ' '

Write-Host 'Building the grey-box map.'
$process = Start-Process -FilePath $editor -ArgumentList $arguments -NoNewWindow -PassThru -Wait
if ($process.ExitCode -ne 0) {
    Write-Host "The commandlet failed with exit code $($process.ExitCode). Log: $logFile"
    exit 1
}
Write-Host "Saved the grey-box map. Log: $logFile"
exit 0
