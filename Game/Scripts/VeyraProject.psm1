#Requires -Version 7.0
<#
    Shared functions for the Game/Scripts commands (ADR-006 §10). Humans, coding agents and the
    future CI runner all use these, so no script hard-codes a machine-specific path.
#>

Set-StrictMode -Version Latest

# Source engines registered for the current user, keyed by association identifier.
$script:EngineBuildsKey = 'HKCU:\SOFTWARE\Epic Games\Unreal Engine\Builds'

function Get-VeyraProjectFile {
    <#
    .SYNOPSIS
        Returns the absolute path of Game/Veyra.uproject, the parent of this Scripts folder.
    #>
    [CmdletBinding()]
    [OutputType([string])]
    param()

    $projectFile = [System.IO.Path]::GetFullPath((Join-Path (Split-Path -Parent $PSScriptRoot) 'Veyra.uproject'))
    if (-not (Test-Path -LiteralPath $projectFile -PathType Leaf)) {
        throw "Cannot find the Veyra project file at '$projectFile'."
    }
    return $projectFile
}

function Resolve-VeyraEngineRoot {
    <#
    .SYNOPSIS
        Returns the root folder of the engine that builds the project.
    .DESCRIPTION
        With -EngineRoot, validates and returns that folder. Otherwise reads EngineAssociation from
        the .uproject and looks it up among the source engines registered for the current user.
        Fails with a clear message when the engine cannot be found or is incomplete.
    #>
    [CmdletBinding()]
    [OutputType([string])]
    param(
        [Parameter(Mandatory)]
        [string]$ProjectFile,

        [string]$EngineRoot
    )

    if ($EngineRoot) {
        $candidate = $EngineRoot
        $origin = "-EngineRoot '$EngineRoot'"
    }
    else {
        $association = (Get-Content -LiteralPath $ProjectFile -Raw | ConvertFrom-Json).EngineAssociation
        if ([string]::IsNullOrWhiteSpace($association)) {
            throw "'$ProjectFile' has no EngineAssociation. Pass -EngineRoot <path>."
        }

        $registered = Get-ItemProperty -LiteralPath $script:EngineBuildsKey -ErrorAction SilentlyContinue
        $candidate = if ($registered) { $registered.PSObject.Properties[$association]?.Value } else { $null }
        if ([string]::IsNullOrWhiteSpace($candidate)) {
            throw "Engine association '$association' is not registered under '$script:EngineBuildsKey'. Register that source engine for this user, or pass -EngineRoot <path>."
        }
        $origin = "engine association '$association'"
    }

    $root = [System.IO.Path]::GetFullPath($candidate)
    foreach ($relativePath in 'Engine\Build\BatchFiles\Build.bat', 'Engine\Build\BatchFiles\RunUBT.bat') {
        if (-not (Test-Path -LiteralPath (Join-Path $root $relativePath) -PathType Leaf)) {
            throw "'$root' (from $origin) is not a usable Unreal Engine folder: '$relativePath' is missing."
        }
    }
    return $root
}

Export-ModuleMember -Function Get-VeyraProjectFile, Resolve-VeyraEngineRoot
