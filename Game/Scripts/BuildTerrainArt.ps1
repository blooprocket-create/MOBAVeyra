#Requires -Version 7.0
<#
.SYNOPSIS
    Generates and imports the Crucible's terrain textures and builds its Landscape material.
.DESCRIPTION
    Runs Game/Scripts/GenerateTerrainTextures.py (plain Python with numpy and Pillow) to write the tileable textures
    of Game/ArtSource/Environment/Terrain from TerrainTextures.json, then an editor commandlet that imports them and
    builds M_CrucibleTerrain, M_CrucibleWater and the river's ripple step M_CrucibleWaterStir (ImportTerrainArt.py).
    Does not modify maps or gameplay code. A
    texture whose PNG is unchanged since its last import, and a material whose values, graph code and textures are
    unchanged since it was built, are kept as they are, unwritten: the importer records what each was made from on its
    asset (Game/Scripts/TerrainArt/inputs.py). So only the assets that change need their Git LFS locks (ADR-006 §9),
    except that an asset recording nothing, as every one written before the importer recorded this does, is rewritten
    once: that first run needs the locks of every texture and the three materials. The editor must already be built.
.PARAMETER ImportOnly
    Reuse the textures already generated.
.PARAMETER WaterOnly
    Import only the water normal texture and rebuild the river material. Generation still validates the full profile.
.PARAMETER EngineRoot
    Engine folder to use instead of the one registered for the project's EngineAssociation.
.EXAMPLE
    ./Game/Scripts/BuildTerrainArt.ps1
#>
[CmdletBinding()]
param(
    [switch]$ImportOnly,
    [switch]$WaterOnly,
    [string]$EngineRoot
)
$ErrorActionPreference = 'Stop'
Import-Module (Join-Path $PSScriptRoot 'VeyraProject.psm1') -Force
$project = Get-VeyraProjectFile
$game = Split-Path -Parent $project
$engine = Resolve-VeyraEngineRoot -ProjectFile $project -EngineRoot $EngineRoot
$saved = Join-Path $game 'Saved/TerrainArt'
New-Item -ItemType Directory -Force -Path $saved | Out-Null
if (-not $ImportOnly) {
    & python (Join-Path $PSScriptRoot 'GenerateTerrainTextures.py') *> (Join-Path $saved 'Generate.log')
    if ($LASTEXITCODE -ne 0) { throw "Texture generation failed. See $saved/Generate.log" }
}
$editor = Join-Path $engine 'Engine/Binaries/Win64/UnrealEditor-Cmd.exe'
$script = Join-Path $PSScriptRoot 'ImportTerrainArt.py'
$log = Join-Path $saved 'Import.log'
$report = Join-Path $saved 'import.json'
if (Test-Path -LiteralPath $report) { Remove-Item -LiteralPath $report }
$scopeArguments = @()
if ($WaterOnly) { $scopeArguments += '-VeyraWaterOnly' }
& $editor $project '-run=pythonscript' "-script=$script" '-EnablePlugins=PythonScriptPlugin' '-unattended' '-AllowCommandletRendering' '-nosplash' '-nosound' "-ABSLOG=$log" @scopeArguments *> (Join-Path $saved 'Import-console.log')
if ($LASTEXITCODE -ne 0 -or -not (Test-Path -LiteralPath $report)) { throw "Terrain import failed. See $log" }
Write-Host "Requested terrain/water assets imported. Report: $report"
