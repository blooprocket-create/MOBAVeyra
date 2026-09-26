#Requires -Version 7.0
<#
.SYNOPSIS
    Generates the IDE project files for Game/Veyra.uproject.
.DESCRIPTION
    Runs the engine's RunUBT.bat in project-files mode for this project only. It deliberately does
    not call GenerateProjectFiles.bat, which waits for a key press when it fails.
.PARAMETER EngineRoot
    Engine folder to use instead of the one registered for the project's EngineAssociation.
.EXAMPLE
    ./Game/Scripts/GenerateProjectFiles.ps1
#>
[CmdletBinding()]
param(
    [string]$EngineRoot
)

$ErrorActionPreference = 'Stop'
Import-Module (Join-Path $PSScriptRoot 'VeyraProject.psm1') -Force

$projectFile = Get-VeyraProjectFile
$engineRoot = Resolve-VeyraEngineRoot -ProjectFile $projectFile -EngineRoot $EngineRoot

& (Join-Path $engineRoot 'Engine\Build\BatchFiles\RunUBT.bat') -ProjectFiles "-Project=$projectFile" -Game
exit $LASTEXITCODE
