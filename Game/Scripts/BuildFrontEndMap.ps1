#Requires -Version 7.0
<#
.SYNOPSIS
    Regenerates the front-end map.
.DESCRIPTION
    Runs the VeyraFrontEndMap commandlet headless. It saves
    Game/Content/Veyra/FrontEnd/Maps/L_FrontEnd.umap, replacing the file: an empty world whose game
    mode is AVeyraShellGameMode, where the shell's screens are drawn (ADR-010 §3). The editor must
    already be built (Build.ps1 -Target VeyraEditor).

    The map is a lockable Git LFS file (ADR-006 §9): lock it before committing a new version.
    The log goes to Game/Saved/Logs/BuildFrontEndMap.log.
.PARAMETER EngineRoot
    Engine folder to use instead of the one registered for the project's EngineAssociation.
.EXAMPLE
    ./Game/Scripts/BuildFrontEndMap.ps1
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
$logFile = Join-Path $gameDir 'Saved\Logs\BuildFrontEndMap.log'
New-Item -ItemType Directory -Force -Path (Split-Path -Parent $logFile) | Out-Null

$arguments = @(
    "`"$projectFile`""
    '-run=VeyraFrontEndMap'
    "-ABSLOG=`"$logFile`""
    '-unattended'
    '-nullrhi'
    '-nosplash'
    '-nosound'
) -join ' '

Write-Host 'Building the front-end map.'
$process = Start-Process -FilePath $editor -ArgumentList $arguments -NoNewWindow -PassThru -Wait
if ($process.ExitCode -ne 0) {
    Write-Host "The commandlet failed with exit code $($process.ExitCode). Log: $logFile"
    exit 1
}
Write-Host "Saved the front-end map. Log: $logFile"
exit 0
