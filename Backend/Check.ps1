#Requires -Version 7.0
<#
.SYNOPSIS
    Runs the backend's checks in the official Go image, as CI does.
.DESCRIPTION
    Runs gofmt, go vet and go test -race inside golang:<version from go.mod>, so no Go toolchain
    is needed on Windows (ADR-007). The repository is mounted at /src, and two named volumes cache
    Go modules and build output between runs.

    -Postgres also runs the Postgres integration tests. It starts the compose postgres service,
    creates a separate veyra_test database (the tests truncate their tables), and joins the test
    container to the compose network. The local database password is read from the compose
    configuration, not repeated here.

    Exit codes: 0 every check passed; 1 a check failed; 2 infrastructure error.
.PARAMETER Postgres
    Also runs the Postgres integration tests against the compose postgres service.
.EXAMPLE
    ./Backend/Check.ps1
.EXAMPLE
    ./Backend/Check.ps1 -Postgres
#>
[CmdletBinding()]
param(
    [switch]$Postgres
)

$ErrorActionPreference = 'Stop'

$ExitPassed = 0
$ExitFailed = 1
$ExitInfrastructure = 2

$backend = $PSScriptRoot
$repo = Split-Path $backend -Parent
$compose = Join-Path $repo 'compose.yaml'

$goVersion = (Select-String -LiteralPath (Join-Path $backend 'go.mod') -Pattern '^go (\d+\.\d+)').Matches.Groups[1].Value
if (-not $goVersion) {
    Write-Host 'Could not read the Go version from go.mod.'
    exit $ExitInfrastructure
}
$image = "golang:$goVersion"

$dockerArguments = @(
    'run', '--rm',
    '-v', "${repo}:/src",
    '-v', 'veyra-go-mod:/go/pkg/mod',
    '-v', 'veyra-go-build:/root/.cache/go-build',
    '-w', '/src/Backend'
)

if ($Postgres) {
    $configuration = docker compose -f $compose config --format json | ConvertFrom-Json
    if ($LASTEXITCODE -ne 0) {
        Write-Host 'docker compose config failed.'
        exit $ExitInfrastructure
    }
    $database = $configuration.services.postgres.environment
    $network = $configuration.networks.default.name
    docker compose -f $compose up -d --wait postgres
    if ($LASTEXITCODE -ne 0) {
        Write-Host 'The compose postgres service did not start.'
        exit $ExitInfrastructure
    }
    $testDatabase = 'veyra_test'
    $exists = docker compose -f $compose exec -T postgres psql -U $database.POSTGRES_USER -d $database.POSTGRES_DB -tAc "SELECT 1 FROM pg_database WHERE datname = '$testDatabase'"
    if ($LASTEXITCODE -ne 0) {
        Write-Host 'Could not query the postgres service.'
        exit $ExitInfrastructure
    }
    if ("$exists".Trim() -ne '1') {
        docker compose -f $compose exec -T postgres createdb -U $database.POSTGRES_USER $testDatabase
        if ($LASTEXITCODE -ne 0) {
            Write-Host "Could not create the $testDatabase database."
            exit $ExitInfrastructure
        }
    }
    $url = 'postgres://{0}:{1}@postgres:5432/{2}?sslmode=disable' -f $database.POSTGRES_USER, $database.POSTGRES_PASSWORD, $testDatabase
    $dockerArguments += @('--network', $network, '-e', "VEYRA_TEST_DATABASE_URL=$url")
}

$script = @'
set -e
unformatted=$(gofmt -l .)
if [ -n "$unformatted" ]; then
    echo "gofmt would change:"
    echo "$unformatted"
    exit 1
fi
go vet ./...
go test -race -count=1 ./...
'@ -replace "`r", ''

Write-Host "Running gofmt, go vet and go test in $image$(if ($Postgres) { ', with Postgres' })."
docker @dockerArguments $image sh -c $script
if ($LASTEXITCODE -ne 0) {
    Write-Host 'The backend checks failed.'
    exit $ExitFailed
}
Write-Host 'The backend checks passed.'
exit $ExitPassed
