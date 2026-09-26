#Requires -Version 7.0
<#
.SYNOPSIS
    Cooks, stages and archives a packaged Veyra client or server.
.DESCRIPTION
    Runs the engine's BuildCookRun for an already built target: build it first with Build.ps1, which
    keeps the engine guard. BuildCookRun is never asked to build, so it cannot rebuild the engine.

    The package goes to Game/Saved/Packages/<Target>-<Platform>, and the log to
    Game/Saved/Logs/Package-<Target>-<Platform>.log.

    Tuning check. The packaged build must carry every tuning file (ADR-006 §6). The script lists
    the package's .pak files with UnrealPak and fails if any file in Game/Tuning is missing.
.PARAMETER Target
    VeyraServer or VeyraClient.
.PARAMETER Platform
    Linux or Win64 for VeyraServer (Linux is the shipping server platform); Win64 for VeyraClient.
.PARAMETER Configuration
    The build configuration.
.PARAMETER EngineRoot
    Engine folder to use instead of the one registered for the project's EngineAssociation.
.EXAMPLE
    ./Game/Scripts/Package.ps1 -Target VeyraServer -Platform Linux
.EXAMPLE
    ./Game/Scripts/Package.ps1 -Target VeyraClient -Platform Win64
#>
[CmdletBinding()]
param(
    [Parameter(Mandatory)]
    [ValidateSet('VeyraServer', 'VeyraClient')]
    [string]$Target,

    [Parameter(Mandatory)]
    [ValidateSet('Win64', 'Linux')]
    [string]$Platform,

    [ValidateSet('Development', 'Shipping')]
    [string]$Configuration = 'Development',

    [string]$EngineRoot
)

$ErrorActionPreference = 'Stop'
Import-Module (Join-Path $PSScriptRoot 'VeyraProject.psm1') -Force

if ($Target -eq 'VeyraClient' -and $Platform -ne 'Win64') {
    Write-Host 'VeyraClient is packaged for Win64 only (ADR-005).'
    exit 2
}

$projectFile = Get-VeyraProjectFile
$gameDir = Split-Path -Parent $projectFile
$engineRoot = Resolve-VeyraEngineRoot -ProjectFile $projectFile -EngineRoot $EngineRoot

# The Linux toolchain is registered for the machine; this process may not have inherited it.
if ($Platform -eq 'Linux' -and -not $env:LINUX_MULTIARCH_ROOT) {
    $env:LINUX_MULTIARCH_ROOT = [Environment]::GetEnvironmentVariable('LINUX_MULTIARCH_ROOT', 'Machine')
}

$packageDir = Join-Path $gameDir "Saved\Packages\$Target-$Platform"
$logFile = Join-Path $gameDir "Saved\Logs\Package-$Target-$Platform.log"
New-Item -ItemType Directory -Force -Path (Split-Path -Parent $logFile) | Out-Null

$uatArguments = @(
    'BuildCookRun'
    "-project=$projectFile"
    '-noP4'
    '-unattended'
    '-utf8output'
    '-nocompileeditor'
    '-cook'
    '-stage'
    '-pak'
    '-archive'
    "-archivedirectory=$packageDir"
    '-nodebuginfo'
)
if ($Target -eq 'VeyraServer') {
    $uatArguments += @('-server', '-noclient', "-serverplatform=$Platform", "-serverconfig=$Configuration")
}
else {
    $uatArguments += @('-client', "-platform=$Platform", "-clientconfig=$Configuration")
}

Write-Host "RunUAT.bat $($uatArguments -join ' ')"
& (Join-Path $engineRoot 'Engine\Build\BatchFiles\RunUAT.bat') @uatArguments | Tee-Object -FilePath $logFile | Out-Host
if ($LASTEXITCODE -ne 0) {
    Write-Host "BuildCookRun failed with exit code $LASTEXITCODE. Log: $logFile"
    exit 1
}

# Every tuning document and schema must be in the package's .pak files.
$unrealPak = Join-Path $engineRoot 'Engine\Binaries\Win64\UnrealPak.exe'
$pakFiles = @(Get-ChildItem -LiteralPath $packageDir -Recurse -Filter '*.pak')
if ($pakFiles.Count -eq 0) {
    Write-Host "No .pak file was found under $packageDir."
    exit 1
}
$packaged = foreach ($pak in $pakFiles) {
    & $unrealPak $pak.FullName -List 2>&1 | ForEach-Object { $_.ToString().Replace('\', '/') }
}
$tuningDir = Join-Path $gameDir 'Tuning'
$missing = foreach ($file in Get-ChildItem -LiteralPath $tuningDir -Recurse -Filter '*.json') {
    $relative = 'Veyra/Tuning/' + [System.IO.Path]::GetRelativePath($tuningDir, $file.FullName).Replace('\', '/')
    if (-not ($packaged | Where-Object { $_.Contains($relative) })) {
        $relative
    }
}
if (@($missing).Count -gt 0) {
    Write-Host 'The package is missing tuning files (ADR-006 §6):'
    $missing | ForEach-Object { Write-Host "  $_" }
    exit 1
}

Write-Host "Packaged $Target for $Platform in $packageDir; every tuning file is in the package."
exit 0
