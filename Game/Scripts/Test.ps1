#Requires -Version 7.0
<#
.SYNOPSIS
    Runs Veyra's automation tests headless and summarises the report.
.DESCRIPTION
    Starts UnrealEditor-Cmd with -nullrhi and runs "Automation RunTest <Filter>", which runs every
    test whose full name contains the filter (case-insensitive). The editor must already be built
    (Build.ps1 -Target VeyraEditor). The report (index.json), the editor log and the console
    output go to a new Game/Saved/Automation/Reports/<timestamp> folder.

    Exit codes:
      0  every matched test passed;
      1  a test failed or did not run, or no test matched the filter;
      2  infrastructure error: the editor could not start, failed, timed out or wrote no report.
.PARAMETER Filter
    Substring of the full test names to run. Letters, digits, '_' and '.' only.
.PARAMETER TimeoutMinutes
    Minutes to wait for the editor before stopping it and failing with exit code 2.
.PARAMETER EngineRoot
    Engine folder to use instead of the one registered for the project's EngineAssociation.
.EXAMPLE
    ./Game/Scripts/Test.ps1
.EXAMPLE
    ./Game/Scripts/Test.ps1 -Filter Veyra.Core.DamageTypeTags
#>
[CmdletBinding()]
param(
    [ValidatePattern('^[A-Za-z0-9_.]+$')]
    [string]$Filter = 'Veyra',

    [ValidateRange(1, 1440)]
    [int]$TimeoutMinutes = 30,

    [string]$EngineRoot
)

$ErrorActionPreference = 'Stop'
Import-Module (Join-Path $PSScriptRoot 'VeyraProject.psm1') -Force

$ExitPassed = 0
$ExitTestsFailed = 1
$ExitInfrastructure = 2

function Write-LogTail {
    param([string]$Path)
    if (Test-Path -LiteralPath $Path) {
        Write-Host "Last lines of ${Path}:"
        Get-Content -LiteralPath $Path -Tail 40 | ForEach-Object { Write-Host "  $_" }
    }
}

try {
    $projectFile = Get-VeyraProjectFile
    $engineRoot = Resolve-VeyraEngineRoot -ProjectFile $projectFile -EngineRoot $EngineRoot
}
catch {
    Write-Host $_.Exception.Message
    exit $ExitInfrastructure
}

$editor = Join-Path $engineRoot 'Engine\Binaries\Win64\UnrealEditor-Cmd.exe'
if (-not (Test-Path -LiteralPath $editor -PathType Leaf)) {
    Write-Host "UnrealEditor-Cmd.exe was not found at '$editor'. Build the engine's editor first."
    exit $ExitInfrastructure
}

$gameDir = Split-Path -Parent $projectFile
$reportDir = Join-Path $gameDir ('Saved\Automation\Reports\' + (Get-Date -Format 'yyyyMMdd-HHmmss'))
if (Test-Path -LiteralPath $reportDir) {
    Remove-Item -LiteralPath $reportDir -Recurse -Force
}
New-Item -ItemType Directory -Path $reportDir | Out-Null
$indexFile = Join-Path $reportDir 'index.json'
$logFile = Join-Path $reportDir 'Editor.log'
$consoleFile = Join-Path $reportDir 'Console.txt'
$consoleErrorFile = Join-Path $reportDir 'ConsoleErrors.txt'

# One pre-quoted string, so the editor receives each quoted value intact.
$editorArguments = @(
    "`"$projectFile`""
    "-ExecCmds=`"Automation RunTest $Filter;Quit`""
    '-TestExit="Automation Test Queue Empty"'
    "-ReportExportPath=`"$reportDir`""
    "-ABSLOG=`"$logFile`""
    '-unattended'
    '-nullrhi'
    '-nosplash'
    '-nosound'
) -join ' '

Write-Host "Running automation tests matching '$Filter' (timeout $TimeoutMinutes min)."
Write-Host "Report folder: $reportDir"
$process = Start-Process -FilePath $editor -ArgumentList $editorArguments -NoNewWindow -PassThru `
    -RedirectStandardOutput $consoleFile -RedirectStandardError $consoleErrorFile
$null = $process.Handle # Keeps the exit code readable after the process ends.

if (-not $process.WaitForExit([TimeSpan]::FromMinutes($TimeoutMinutes))) {
    $process.Kill($true)
    $process.WaitForExit()
    Write-Host "The editor did not finish within $TimeoutMinutes minute(s) and was stopped."
    Write-LogTail -Path $logFile
    exit $ExitInfrastructure
}
$editorExitCode = $process.ExitCode

if ((Test-Path -LiteralPath $logFile) -and (Select-String -LiteralPath $logFile -SimpleMatch 'No automation tests matched' -Quiet)) {
    Write-Host "No automation test matched '$Filter'."
    exit $ExitTestsFailed
}

# Automation tests register by class name. A second test class with the same name is dropped with
# only a warning, so its tests would silently never run.
$registrationFailures = @(if (Test-Path -LiteralPath $logFile) { Select-String -LiteralPath $logFile -SimpleMatch 'Failed to register test' })
if ($registrationFailures.Count -gt 0) {
    Write-Host 'Some tests were not registered, usually because two test classes share a name:'
    $registrationFailures | ForEach-Object { Write-Host "  $($_.Line)" }
    exit $ExitTestsFailed
}
if (-not (Test-Path -LiteralPath $indexFile)) {
    Write-Host "The editor exited with code $editorExitCode and wrote no report."
    Write-LogTail -Path $logFile
    exit $ExitInfrastructure
}

$report = Get-Content -LiteralPath $indexFile -Raw | ConvertFrom-Json
$tests = @($report.tests | Sort-Object fullTestPath)
$tests | Format-Table -AutoSize -Property @(
    @{ Name = 'Result'; Expression = { $_.state } }
    @{ Name = 'Test'; Expression = { $_.fullTestPath } }
    @{ Name = 'Seconds'; Expression = { '{0:N3}' -f $_.duration } }
) | Out-Host

$notPassed = @($tests | Where-Object { $_.state -ne 'Success' })
foreach ($test in $notPassed) {
    Write-Host "$($test.state): $($test.fullTestPath)"
    foreach ($entry in @($test.entries | Where-Object { $_.event.type -eq 'Error' })) {
        Write-Host "  $($entry.event.message)"
    }
}

Write-Host ("Succeeded {0}, with warnings {1}, failed {2}, not run {3}." -f `
    $report.succeeded, $report.succeededWithWarnings, $report.failed, $report.notRun)

if ($tests.Count -eq 0) {
    Write-Host "The report lists no tests for '$Filter'."
    exit $ExitTestsFailed
}
if ($notPassed.Count -gt 0 -or $report.failed -gt 0 -or $report.notRun -gt 0) {
    exit $ExitTestsFailed
}
if ($editorExitCode -ne 0) {
    Write-Host "Every test passed, but the editor exited with code $editorExitCode."
    Write-LogTail -Path $logFile
    exit $ExitInfrastructure
}
Write-Host "All $($tests.Count) test(s) passed."
exit $ExitPassed
