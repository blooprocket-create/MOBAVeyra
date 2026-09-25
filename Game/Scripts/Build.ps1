#Requires -Version 7.0
<#
.SYNOPSIS
    Builds a Veyra target with UnrealBuildTool.
.DESCRIPTION
    Runs the engine's Build.bat for Game/Veyra.uproject and keeps a copy of the output in
    Game/Saved/Logs/Build-<Target>-<Platform>-<Configuration>.log.

    Engine guard. By default the build passes -NoEngineChanges: UnrealBuildTool then stops, and
    lists the files, before it would overwrite or delete any existing file under the engine
    folder. VeyraEditor must never rebuild the engine (ADR-006 §2). New files under the engine
    folder are still allowed.

    Warning gate. A build that succeeds but reports a warning in Game/Source or Game/Plugins
    exits with code 1: Veyra modules compile with zero warnings (ADR-006 §10).

    Dry run. -DryRun passes -WriteOutdatedActions and summarises the actions a build would run,
    without compiling them. It is not a no-op: UnrealBuildTool still runs UnrealHeaderTool and
    deletes outdated outputs first, which is why the engine guard also applies to it.
.PARAMETER Target
    The target to build.
.PARAMETER Platform
    Win64, or Linux for VeyraServer (cross-compiled; needs LINUX_MULTIARCH_ROOT).
.PARAMETER Configuration
    The build configuration.
.PARAMETER AllowEngineChanges
    Omits -NoEngineChanges. Use it only after deciding the engine may be rebuilt.
.PARAMETER DryRun
    Lists and summarises the outdated actions instead of building.
.PARAMETER EngineRoot
    Engine folder to use instead of the one registered for the project's EngineAssociation.
.EXAMPLE
    ./Game/Scripts/Build.ps1 -Target VeyraEditor
.EXAMPLE
    ./Game/Scripts/Build.ps1 -Target VeyraServer -Platform Linux -DryRun
#>
[CmdletBinding()]
param(
    [Parameter(Mandatory)]
    [ValidateSet('Veyra', 'VeyraEditor', 'VeyraClient', 'VeyraServer')]
    [string]$Target,

    [ValidateSet('Win64', 'Linux')]
    [string]$Platform = 'Win64',

    [ValidateSet('Debug', 'DebugGame', 'Development', 'Test', 'Shipping')]
    [string]$Configuration = 'Development',

    [switch]$AllowEngineChanges,

    [switch]$DryRun,

    [string]$EngineRoot
)

$ErrorActionPreference = 'Stop'
Import-Module (Join-Path $PSScriptRoot 'VeyraProject.psm1') -Force

function Get-ActionModuleName {
    # Names the module an output file belongs to, from UnrealBuildTool's intermediate layout:
    # Intermediate/Build/<Platform>/.../<Configuration>/<Module>/... for compiled code and
    # Intermediate/Build/<Platform>/.../Inc/<Module>/... for UnrealHeaderTool output.
    param([string]$Path, [string]$Configuration)

    $segments = $Path -split '[\\/]'
    $buildIndex = -1
    for ($index = 0; $index -lt $segments.Count - 1; $index++) {
        if ($segments[$index] -eq 'Intermediate' -and $segments[$index + 1] -eq 'Build') {
            $buildIndex = $index + 1
            break
        }
    }
    if ($buildIndex -lt 0) {
        return $(if ($segments -contains 'Binaries') { '(binaries)' } else { '(other)' })
    }
    for ($index = $buildIndex + 1; $index -lt $segments.Count - 1; $index++) {
        if ($segments[$index] -eq $Configuration -or $segments[$index] -eq 'Inc') {
            return $segments[$index + 1]
        }
    }
    return '(other)'
}

function Write-OutdatedActionSummary {
    param([string]$ActionsFile, [string]$EngineRoot, [string]$Configuration, [string]$Label)

    $engineFolder = (Join-Path $EngineRoot 'Engine') + [System.IO.Path]::DirectorySeparatorChar
    $byType = @{}
    $byModule = @{}
    $actionCount = 0
    $engineOutputCount = 0

    # The action list can be hundreds of megabytes for a monolithic target, so it is read with
    # JsonDocument rather than ConvertFrom-Json.
    $stream = [System.IO.File]::OpenRead($ActionsFile)
    try {
        $document = [System.Text.Json.JsonDocument]::Parse($stream)
        try {
            foreach ($action in $document.RootElement.GetProperty('Actions').EnumerateArray()) {
                $actionCount++
                $type = $action.GetProperty('Type').ToString()
                $byType[$type] = 1 + ($byType[$type] ?? 0)

                $producedItems = @($action.GetProperty('ProducedItems').EnumerateArray() | ForEach-Object { $_.GetString() })
                if ($producedItems | Where-Object { $_.StartsWith($engineFolder, [System.StringComparison]::OrdinalIgnoreCase) }) {
                    $engineOutputCount++
                }
                $module = if ($producedItems.Count -gt 0) { Get-ActionModuleName -Path $producedItems[0] -Configuration $Configuration } else { '(no outputs)' }
                $byModule[$module] = 1 + ($byModule[$module] ?? 0)
            }
        }
        finally {
            $document.Dispose()
        }
    }
    finally {
        $stream.Dispose()
    }

    Write-Host ''
    Write-Host "Dry run: $actionCount outdated action(s) for $Label."
    Write-Host "  Action list: $ActionsFile"
    Write-Host "  Actions writing under the engine folder: $engineOutputCount"
    Write-Host '  By type:'
    $byType.GetEnumerator() | Sort-Object Value -Descending | ForEach-Object { Write-Host ('    {0,-28} {1,7}' -f $_.Key, $_.Value) }
    Write-Host '  By module (largest 25):'
    $byModule.GetEnumerator() | Sort-Object Value -Descending | Select-Object -First 25 | ForEach-Object { Write-Host ('    {0,-28} {1,7}' -f $_.Key, $_.Value) }
}

$projectFile = Get-VeyraProjectFile
$gameDir = Split-Path -Parent $projectFile
$engineRoot = Resolve-VeyraEngineRoot -ProjectFile $projectFile -EngineRoot $EngineRoot

$ubtArguments = @($Target, $Platform, $Configuration, "-Project=$projectFile", '-WaitMutex')
if (-not $AllowEngineChanges) {
    $ubtArguments += '-NoEngineChanges'
}
$actionsFile = $null
if ($DryRun) {
    $actionsFile = Join-Path $gameDir "Saved\Build\OutdatedActions-$Target-$Platform-$Configuration.json"
    if (Test-Path -LiteralPath $actionsFile) {
        Remove-Item -LiteralPath $actionsFile
    }
    $ubtArguments += "-WriteOutdatedActions=$actionsFile"
}

$logFile = Join-Path $gameDir "Saved\Logs\Build-$Target-$Platform-$Configuration.log"
New-Item -ItemType Directory -Force -Path (Split-Path -Parent $logFile) | Out-Null

Write-Host "Build.bat $($ubtArguments -join ' ')"
& (Join-Path $engineRoot 'Engine\Build\BatchFiles\Build.bat') @ubtArguments | Tee-Object -FilePath $logFile
$exitCode = $LASTEXITCODE

# Compiler and rules warnings that point into Veyra's own source.
$veyraSourceFolders = foreach ($folder in 'Source', 'Plugins') {
    $path = Join-Path $gameDir $folder
    $path
    $path.Replace('\', '/')
}
$veyraWarnings = @(Get-Content -LiteralPath $logFile | Where-Object {
    $line = $_
    ($line -match '\bwarning\b') -and ($veyraSourceFolders | Where-Object { $line.IndexOf($_, [System.StringComparison]::OrdinalIgnoreCase) -ge 0 })
})
if ($veyraWarnings.Count -gt 0) {
    Write-Host ''
    Write-Host "$($veyraWarnings.Count) warning(s) in Veyra source (zero are allowed):"
    $veyraWarnings | ForEach-Object { Write-Host "  $_" }
}

if ($exitCode -ne 0) {
    Write-Host "Build failed with exit code $exitCode. Full output: $logFile"
    exit $exitCode
}
if ($DryRun) {
    if (-not (Test-Path -LiteralPath $actionsFile)) {
        Write-Host "UnrealBuildTool succeeded but wrote no action list to $actionsFile."
        exit 2
    }
    Write-OutdatedActionSummary -ActionsFile $actionsFile -EngineRoot $engineRoot -Configuration $Configuration -Label "$Target $Platform $Configuration"
}
if ($veyraWarnings.Count -gt 0) {
    exit 1
}
exit 0
