#Requires -Version 7.0
<#
.SYNOPSIS
    Runs the launcher's checks, as CI does.
.DESCRIPTION
    Runs cargo fmt --check, cargo clippy with warnings as errors, and cargo test over the launcher's
    workspace (Launcher/: the core, the CLI and the Tauri app), then builds it in release. Needs the
    Rust toolchain from rustup (stable, MSVC, with clippy and rustfmt); cargo is looked for on PATH
    and then in %USERPROFILE%\.cargo\bin.

    Exit codes: 0 every check passed; 1 a check failed; 2 infrastructure error.
.EXAMPLE
    ./Launcher/Check.ps1
#>
[CmdletBinding()]
param()

$ErrorActionPreference = 'Stop'

$ExitPassed = 0
$ExitFailed = 1
$ExitInfrastructure = 2

$cargo = (Get-Command cargo -ErrorAction SilentlyContinue)?.Source
if (-not $cargo) {
    $cargo = Join-Path $env:USERPROFILE '.cargo\bin\cargo.exe'
}
if (-not (Test-Path -LiteralPath $cargo -PathType Leaf)) {
    Write-Host 'cargo was not found. Install Rust with rustup (stable, MSVC), with clippy and rustfmt.'
    exit $ExitInfrastructure
}
$env:PATH = (Split-Path -Parent $cargo) + [System.IO.Path]::PathSeparator + $env:PATH

$steps = @(
    @{ Name = 'cargo fmt'; Arguments = @('fmt', '--all', '--check') }
    @{ Name = 'cargo clippy'; Arguments = @('clippy', '--workspace', '--all-targets', '--locked', '--', '-D', 'warnings') }
    @{ Name = 'cargo test'; Arguments = @('test', '--workspace', '--locked') }
    @{ Name = 'cargo build --release'; Arguments = @('build', '--workspace', '--release', '--locked') }
)
Push-Location $PSScriptRoot
try {
    foreach ($step in $steps) {
        Write-Host "Running $($step.Name)."
        & $cargo @($step.Arguments)
        if ($LASTEXITCODE -ne 0) {
            Write-Host "The launcher checks failed at $($step.Name)."
            exit $ExitFailed
        }
    }
}
finally {
    Pop-Location
}
Write-Host 'The launcher checks passed.'
exit $ExitPassed
