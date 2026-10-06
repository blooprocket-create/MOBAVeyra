#Requires -Version 7.0
<#
.SYNOPSIS
    Checks the generated battleground's navigation, collision and fairness.
.DESCRIPTION
    Runs the VeyraWorldValidate commandlet headless (ADR-040; World Validation Standard gates 6, 7
    and 8). It loads L_Battleground, spawns the server's structures and walls, builds the navigation
    the server builds and measures each team's equivalent routes, every lane base to base, the
    terrain's half-turn symmetry, where every anchor stands and that generated presentation has no
    collision. The routes and tolerances are Game/Plugins/VeyraWorldTools/Config/CrucibleValidation.json.

    The report goes to Game/Saved/WorldGeneration/Validation.json and the log to
    Game/Saved/Logs/ValidateBattleground.log. Exit code 0 when every check passes, 1 on any finding.
    Build the editor and the map first (Build.ps1 -Target VeyraEditor, BuildBattlegroundMap.ps1).
.PARAMETER EngineRoot
    Engine folder to use instead of the one registered for the project's EngineAssociation.
.EXAMPLE
    ./Game/Scripts/ValidateBattleground.ps1
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
$logFile = Join-Path $gameDir 'Saved\Logs\ValidateBattleground.log'
$report = Join-Path $gameDir 'Saved\WorldGeneration\Validation.json'
New-Item -ItemType Directory -Force -Path (Split-Path -Parent $logFile), (Split-Path -Parent $report) | Out-Null
Remove-Item -LiteralPath $report -ErrorAction SilentlyContinue

$arguments = @(
    "`"$projectFile`""
    '-run=VeyraWorldValidate'
    "-ABSLOG=`"$logFile`""
    '-unattended'
    '-nullrhi'
    '-nosplash'
    '-nosound'
) -join ' '

Write-Host 'Validating the battleground map.'
$process = Start-Process -FilePath $editor -ArgumentList $arguments -WindowStyle Hidden -PassThru -Wait
if (-not (Test-Path -LiteralPath $report)) {
    Write-Host "The commandlet wrote no report (exit code $($process.ExitCode)). Log: $logFile"
    exit 1
}
$result = Get-Content -LiteralPath $report -Raw | ConvertFrom-Json
foreach ($finding in $result.findings) {
    Write-Host "  $finding"
}
if ($process.ExitCode -ne 0 -or -not $result.passed) {
    Write-Host "Validation failed: $(@($result.findings).Count) findings. Report: $report"
    exit 1
}
Write-Host "Validation passed: $(@($result.routes).Count) routes, $(@($result.lanes).Count) lanes, $($result.terrain.samples) terrain samples. Report: $report"
exit 0
