#Requires -Version 7.0
<#
.SYNOPSIS
    Publishes the packaged client to the local release store, for the launcher to install (ADR-022).
.DESCRIPTION
    Runs veyra-publish (Launcher/publish) on the packaged client that
    Package.ps1 -Target VeyraClient -Platform Win64 writes to Game/Saved/Packages/VeyraClient-Win64.
    It cuts every file into content-defined chunks, adds the chunks the store lacks to
    Game/Saved/Releases, writes the release's manifest and moves the channel to it. Publishing a
    build again after a change adds only the changed chunks, which is all a launcher then downloads.

    Then it starts compose.yaml's releases service, which serves the store on 127.0.0.1:8090: the
    local stand-in for the download CDN. A launcher installed by Veyra Setup (Launcher/Package.ps1)
    installs or updates the game from there when it opens; so does
    Launcher/target/release/veyra-install.exe --config Launcher/config/installed.json install.

    How releases are cut (chunk sizes, compression) is Launcher/config/publish.json.

    Needs Rust from rustup to build veyra-publish, and Docker for the releases service.

    Exit codes: 0 published and served; 1 the publish failed; 2 infrastructure error.
.PARAMETER Channel
    The channel to move to the build: lowercase letters, digits and '-'. The installed launcher
    follows `local` (Launcher/config/installed.json).
.PARAMETER NoServe
    Publish only; leave the releases service as it is.
.EXAMPLE
    ./Game/Scripts/Publish.ps1
#>
[CmdletBinding()]
param(
    [ValidatePattern('^[a-z0-9][a-z0-9-]{0,31}$')]
    [string]$Channel = 'local',

    [switch]$NoServe
)

$ErrorActionPreference = 'Stop'

$ExitPassed = 0
$ExitFailed = 1
$ExitInfrastructure = 2

$repositoryDir = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$launcherDir = Join-Path $repositoryDir 'Launcher'
$buildDir = Join-Path $repositoryDir 'Game\Saved\Packages\VeyraClient-Win64'
$storeDir = Join-Path $repositoryDir 'Game\Saved\Releases'

if (-not (Test-Path -LiteralPath (Join-Path $buildDir 'VeyraBuild.json') -PathType Leaf)) {
    Write-Host "No packaged client at '$buildDir'. Run Build.ps1 -Target VeyraClient, then Package.ps1 -Target VeyraClient -Platform Win64."
    exit $ExitInfrastructure
}

$cargo = (Get-Command cargo -ErrorAction SilentlyContinue)?.Source
if (-not $cargo) {
    $cargo = Join-Path $env:USERPROFILE '.cargo\bin\cargo.exe'
}
if (-not (Test-Path -LiteralPath $cargo -PathType Leaf)) {
    Write-Host 'cargo was not found. Install Rust with rustup (stable, MSVC).'
    exit $ExitInfrastructure
}
Write-Host 'Building veyra-publish.'
& $cargo build --release --locked --manifest-path (Join-Path $launcherDir 'Cargo.toml') --package veyra-publish
if ($LASTEXITCODE -ne 0) {
    Write-Host 'veyra-publish did not build.'
    exit $ExitInfrastructure
}

New-Item -ItemType Directory -Force -Path $storeDir | Out-Null
$publish = Join-Path $launcherDir 'target\release\veyra-publish.exe'
& $publish --build $buildDir --store $storeDir --channel $Channel --config (Join-Path $launcherDir 'config\publish.json')
if ($LASTEXITCODE -ne 0) {
    Write-Host 'The build was not published.'
    exit $ExitFailed
}

if (-not $NoServe) {
    Write-Host 'Serving the release store on http://127.0.0.1:8090.'
    & docker compose --progress plain --project-directory $repositoryDir up --detach releases
    if ($LASTEXITCODE -ne 0) {
        Write-Host 'The releases service did not start. Check that Docker is running.'
        exit $ExitInfrastructure
    }
}
exit $ExitPassed
