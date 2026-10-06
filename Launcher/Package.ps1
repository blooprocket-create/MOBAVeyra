#Requires -Version 7.0
<#
.SYNOPSIS
    Builds Veyra Setup, the installer for the launcher (ADR-022 §2).
.DESCRIPTION
    Builds the launcher in release, draws Setup's bitmaps from the splash art that setup/art.json
    names (veyra-setup-art), and compiles setup/VeyraSetup.nsi with NSIS 3's makensis. Setup installs
    the launcher with a configuration beside it as VeyraLauncher.json, by -Config:
    - installed (the default), config/installed.json: a launcher that installs the game from the local
      release server (compose.yaml's releases service) and signs in to the local backend;
    - public, config/public.json: the Setup players get, whose launcher installs, signs in and plays
      through Veyra's public address (ADR-057), served while Game/Scripts/Host.ps1 runs on the host PC.

    Needs the Rust toolchain from rustup (stable, MSVC), as Check.ps1 does, and NSIS 3: makensis is
    looked for on PATH and then in the Program Files folders (winget install NSIS.NSIS).

    Setup goes to Launcher/target/setup/VeyraSetup-<version>.exe, the version being the launcher
    workspace's (Cargo.toml), or VeyraSetup-<version>-public.exe with -Config public.
.PARAMETER Config
    The launcher configuration Setup installs: installed (default) or public.
.PARAMETER Publish
    Also publishes Setup to the release store, Game/Saved/Releases, as the launcher release on the
    channel the configuration names (ADR-022 §11): every launcher installed from that channel then
    updates itself to it when it next opens. Publishing writes files only; serving them is Publish.ps1's
    releases service locally, or Game/Scripts/Host.ps1 for players.

    Exit codes: 0 Setup was built (and published); 1 a step failed; 2 infrastructure error.
.EXAMPLE
    ./Launcher/Package.ps1
.EXAMPLE
    ./Launcher/Package.ps1 -Config public -Publish
#>
[CmdletBinding()]
param(
    [ValidateSet('installed', 'public')]
    [string]$Config = 'installed',

    [switch]$Publish
)

$ErrorActionPreference = 'Stop'

$ExitPassed = 0
$ExitFailed = 1
$ExitInfrastructure = 2

$cargo = (Get-Command cargo -ErrorAction SilentlyContinue)?.Source
if (-not $cargo) {
    $cargo = Join-Path $env:USERPROFILE '.cargo\bin\cargo.exe'
}
if (-not (Test-Path -LiteralPath $cargo -PathType Leaf)) {
    Write-Host 'cargo was not found. Install Rust with rustup (stable, MSVC).'
    exit $ExitInfrastructure
}
$env:PATH = (Split-Path -Parent $cargo) + [System.IO.Path]::PathSeparator + $env:PATH

$makensis = (Get-Command makensis -ErrorAction SilentlyContinue)?.Source
if (-not $makensis) {
    $makensis = @(${env:ProgramFiles(x86)}, $env:ProgramFiles) |
        Where-Object { $_ } |
        ForEach-Object { Join-Path $_ 'NSIS\makensis.exe' } |
        Where-Object { Test-Path -LiteralPath $_ -PathType Leaf } |
        Select-Object -First 1
}
if (-not $makensis) {
    Write-Host 'makensis was not found. Install NSIS 3: winget install NSIS.NSIS'
    exit $ExitInfrastructure
}

$workspace = Get-Content -LiteralPath (Join-Path $PSScriptRoot 'Cargo.toml') -Raw
$versionMatch = [regex]::Match($workspace, '(?ms)^\[workspace\.package\].*?^version\s*=\s*"((\d+)\.(\d+)\.(\d+)[^"]*)"')
if (-not $versionMatch.Success) {
    Write-Host 'Launcher/Cargo.toml gives no [workspace.package] version of the form 1.2.3.'
    exit $ExitFailed
}
$version = $versionMatch.Groups[1].Value
# Windows' file properties take four numbers.
$versionQuad = '{0}.{1}.{2}.0' -f $versionMatch.Groups[2].Value, $versionMatch.Groups[3].Value, $versionMatch.Groups[4].Value

$targetDir = Join-Path $PSScriptRoot 'target'
$setupDir = Join-Path $targetDir 'setup'
$payloadDir = Join-Path $setupDir 'payload'
$artDir = Join-Path $setupDir 'art'
$outFile = Join-Path $setupDir $(if ($Config -eq 'public') { "VeyraSetup-$version-public.exe" } else { "VeyraSetup-$version.exe" })

Push-Location $PSScriptRoot
try {
    Write-Host 'Building the launcher, the art tool and the publisher in release.'
    & $cargo build --release --locked --package veyra-launcher --package veyra-setup-art --package veyra-publish
    if ($LASTEXITCODE -ne 0) {
        Write-Host 'The launcher did not build.'
        exit $ExitFailed
    }

    Write-Host 'Drawing Setup''s art.'
    $artTool = Join-Path $targetDir 'release\veyra-setup-art.exe'
    & $artTool --config (Join-Path $PSScriptRoot 'setup\art.json') --out $artDir
    if ($LASTEXITCODE -ne 0) {
        Write-Host 'Setup''s art could not be drawn.'
        exit $ExitFailed
    }

    Remove-Item -LiteralPath $payloadDir -Recurse -Force -ErrorAction SilentlyContinue
    New-Item -ItemType Directory -Force -Path $payloadDir | Out-Null
    Copy-Item -LiteralPath (Join-Path $targetDir 'release\veyra-launcher.exe') -Destination $payloadDir
    Copy-Item -LiteralPath (Join-Path $PSScriptRoot "config\$Config.json") -Destination (Join-Path $payloadDir 'VeyraLauncher.json')

    Write-Host "Compiling Veyra Setup $version."
    & $makensis /V2 /INPUTCHARSET UTF8 `
        "/DVERSION=$version" `
        "/DVERSION_QUAD=$versionQuad" `
        "/DPAYLOAD=$payloadDir" `
        "/DART=$artDir" `
        "/DICON=$(Join-Path $PSScriptRoot 'app\icons\icon.ico')" `
        "/DOUTFILE=$outFile" `
        (Join-Path $PSScriptRoot 'setup\VeyraSetup.nsi')
    if ($LASTEXITCODE -ne 0) {
        Write-Host 'makensis did not compile Setup.'
        exit $ExitFailed
    }

    if ($Publish) {
        $channel = (Get-Content -LiteralPath (Join-Path $PSScriptRoot "config\$Config.json") -Raw | ConvertFrom-Json).game.install.channel
        if (-not $channel) {
            Write-Host "config/$Config.json installs no game from a release store, so its launcher has no channel to publish to."
            exit $ExitFailed
        }
        $storeDir = Join-Path (Split-Path -Parent $PSScriptRoot) 'Game\Saved\Releases'
        New-Item -ItemType Directory -Force -Path $storeDir | Out-Null
        Write-Host "Publishing Setup $version to the release store, channel $channel."
        & (Join-Path $targetDir 'release\veyra-publish.exe') --setup $outFile --version $version --store $storeDir --channel $channel
        if ($LASTEXITCODE -ne 0) {
            Write-Host 'Setup was not published.'
            exit $ExitFailed
        }
    }
}
finally {
    Pop-Location
}
Write-Host "Veyra Setup is at $outFile."
exit $ExitPassed
