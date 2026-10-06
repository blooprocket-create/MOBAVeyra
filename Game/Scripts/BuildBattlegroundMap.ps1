#Requires -Version 7.0
<#
.SYNOPSIS
    Regenerates the battleground map from its layout.
.DESCRIPTION
    Runs the VeyraBattlegroundMap commandlet headless. It reads the layout in Game/Tuning/World.json
    and the VeyraWorldTools style profile, then replaces L_Battleground.umap with Landscape,
    Water, baked PCG environment, review cameras, team starts, navigation bounds and the
    server's battleground marker. The editor must already be built (Build.ps1 -Target VeyraEditor).

    The map is a lockable Git LFS file (ADR-006 §9): lock it before committing a new version.
    The log goes to Game/Saved/Logs/BuildBattlegroundMap.log.
.PARAMETER EngineRoot
    Engine folder to use instead of the one registered for the project's EngineAssociation.
.EXAMPLE
    ./Game/Scripts/BuildBattlegroundMap.ps1
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
$logFile = Join-Path $gameDir 'Saved\Logs\BuildBattlegroundMap.log'
New-Item -ItemType Directory -Force -Path (Split-Path -Parent $logFile) | Out-Null

$arguments = @(
    "`"$projectFile`""
    '-run=VeyraBattlegroundMap'
    "-ABSLOG=`"$logFile`""
    '-unattended'
    '-AllowCommandletRendering'
    '-RenderOffscreen'
    '-NoTextureStreaming'
    '-nosplash'
    '-nosound'
) -join ' '

Write-Host 'Building the battleground map.'
$process = Start-Process -FilePath $editor -ArgumentList $arguments -WindowStyle Hidden -PassThru -Wait
if ($process.ExitCode -ne 0) {
    Write-Host "The commandlet failed with exit code $($process.ExitCode). Log: $logFile"
    exit 1
}
$repoDir = Split-Path -Parent $gameDir
$sourcePaths = @('Game/Tuning/World.json', 'Game/Plugins/VeyraWorldTools/Config/CrucibleStyle.json', 'Game/ArtSource/Environment/CrucibleKit.json', 'Game/ArtSource/Environment/Terrain/TerrainTextures.json')
$inputs = @($sourcePaths | ForEach-Object {
    @{ path = $_; sha256 = (Get-FileHash -LiteralPath (Join-Path $repoDir $_) -Algorithm SHA256).Hash.ToLowerInvariant() }
})
# Each dressing region's manifest, when the style profile dresses the map.
$regionDir = Join-Path $gameDir 'Saved/WorldGeneration/Regions'
$regions = @(if (Test-Path -LiteralPath $regionDir) { Get-ChildItem -LiteralPath $regionDir -Filter '*.json' | Sort-Object Name | ForEach-Object { Get-Content -LiteralPath $_.FullName -Raw | ConvertFrom-Json } })
$map = Join-Path $gameDir 'Content/Veyra/World/Maps/L_Battleground.umap'
$manifest = @{
    generator = 'VeyraWorldTools'; version = 1; status = 'generated';
    generatedUtc = [DateTime]::UtcNow.ToString('o');
    inputs = $inputs; regions = $regions;
    output = @{ path = 'Game/Content/Veyra/World/Maps/L_Battleground.umap'; sha256 = (Get-FileHash -LiteralPath $map -Algorithm SHA256).Hash.ToLowerInvariant() };
    validation = 'Generation only; visual, navigation, performance and package acceptance are separate checks.'
}
New-Item -ItemType Directory -Force -Path (Join-Path $gameDir 'Saved/WorldGeneration') | Out-Null
$manifest | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath (Join-Path $gameDir 'Saved/WorldGeneration/manifest.json') -Encoding utf8
Write-Host "Saved the battleground map. Log: $logFile"
exit 0
