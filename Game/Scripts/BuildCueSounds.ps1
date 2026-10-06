#Requires -Version 7.0
<#
.SYNOPSIS
    Builds the presentation's placeholder cue sounds (ADR-063 section 5) from Game/ArtSource/Presentation/CueSounds.json.
.DESCRIPTION
    GenerateCueSounds.py synthesises each sound from the spec's seed into ArtSource/Presentation/Audio, with a
    manifest of their hashes; ImportCueSounds.py then imports them in an editor commandlet as sound assets under
    /Game/Veyra/UI/Presentation/Audio. Regenerate rather than hand-edit them. An existing sound is a binary asset:
    acquire its Git LFS lock before reimporting it (ADR-006 section 9).
.PARAMETER Sounds
    Imports only the sounds named, as the spec names them, such as a new one; without it, every sound.
#>
[CmdletBinding()]
param(
    [string[]]$Sounds,
    [string]$EngineRoot
)
$ErrorActionPreference = 'Stop'
Import-Module (Join-Path $PSScriptRoot 'VeyraProject.psm1') -Force
$project = Get-VeyraProjectFile
$game = Split-Path -Parent $project
$engine = Resolve-VeyraEngineRoot -ProjectFile $project -EngineRoot $EngineRoot
python (Join-Path $PSScriptRoot 'GenerateCueSounds.py')
if ($LASTEXITCODE -ne 0) {
    throw 'The cue sounds were not synthesised.'
}
$saved = Join-Path $game 'Saved/CueSounds'
New-Item -ItemType Directory -Force -Path $saved | Out-Null
$editor = Join-Path $engine 'Engine/Binaries/Win64/UnrealEditor-Cmd.exe'
$script = Join-Path $PSScriptRoot 'ImportCueSounds.py'
$log = Join-Path $saved 'ImportCueSounds.log'
$env:VEYRA_CUE_SOUNDS_ONLY = $Sounds -join ','
& $editor $project '-run=pythonscript' "-script=$script" '-EnablePlugins=PythonScriptPlugin' '-unattended' '-nullrhi' '-nosplash' "-ABSLOG=$log" *> (Join-Path $saved 'ImportCueSounds-console.log')
if ($LASTEXITCODE -ne 0 -or -not (Select-String -LiteralPath $log -SimpleMatch 'VEYRA_CUE_SOUNDS_PASSED' -Quiet)) {
    throw "The cue sounds were not imported. See $log"
}
Select-String -LiteralPath $log -Pattern 'VEYRA_CUE_SOUND_ASSET: .*' | ForEach-Object { Write-Host $_.Matches[0].Value }
