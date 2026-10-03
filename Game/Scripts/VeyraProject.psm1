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

function Initialize-VeyraPlatformToolchain {
    <#
    .SYNOPSIS
        Makes the Linux cross-compile toolchain visible to this process. It is registered for the
        machine, and a process started before it was installed has not inherited it.
    #>
    param([Parameter(Mandatory)][string]$Platform)
    if ($Platform -eq 'Linux' -and -not $env:LINUX_MULTIARCH_ROOT) {
        $env:LINUX_MULTIARCH_ROOT = [Environment]::GetEnvironmentVariable('LINUX_MULTIARCH_ROOT', 'Machine')
    }
}

function Get-VeyraLaunchHandshake {
    <#
    .SYNOPSIS
        Returns the launch handshake's lines (ADR-010 §5) from their contract,
        Game/Source/VeyraServices/Contracts/LaunchHandshake.json.
    #>
    [CmdletBinding()]
    param()

    $gameDir = Split-Path -Parent (Get-VeyraProjectFile)
    return Get-Content -LiteralPath (Join-Path $gameDir 'Source\VeyraServices\Contracts\LaunchHandshake.json') -Raw | ConvertFrom-Json
}

function Start-VeyraHandshakeClient {
    <#
    .SYNOPSIS
        Starts a game that signs in through the launch handshake, with pipes for its standard input
        and output, and returns a handle for Step-VeyraHandshake.
    .DESCRIPTION
        The game must be started with -VeyraLaunchCode=stdin and without -log, which on Windows can
        replace the standard handles. Its standard error is discarded; its logs go wherever -ABSLOG
        says.
    #>
    [CmdletBinding()]
    param(
        [Parameter(Mandatory)][string]$Executable,
        [Parameter(Mandatory)][string[]]$Arguments
    )

    $startInfo = [System.Diagnostics.ProcessStartInfo]::new($Executable, ($Arguments -join ' '))
    $startInfo.UseShellExecute = $false
    $startInfo.CreateNoWindow = $true
    $startInfo.RedirectStandardInput = $true
    $startInfo.RedirectStandardOutput = $true
    $startInfo.RedirectStandardError = $true
    $startInfo.StandardInputEncoding = [System.Text.UTF8Encoding]::new($false)
    $process = [System.Diagnostics.Process]::Start($startInfo)
    $null = $process.StandardError.BaseStream.CopyToAsync([System.IO.Stream]::Null)
    return [pscustomobject]@{
        Process     = $process
        Handshake   = Get-VeyraLaunchHandshake
        PendingLine = $process.StandardOutput.ReadLineAsync()
        # Starting, AwaitingSignIn, SignedIn or Failed.
        State       = 'Starting'
        Failure     = $null
    }
}

function Step-VeyraHandshake {
    <#
    .SYNOPSIS
        Advances a client's launch handshake as far as its output allows, without waiting, and
        returns its state: Starting, AwaitingSignIn, SignedIn or Failed.
    .DESCRIPTION
        When the game says it awaits its launch code, IssueCode is called for one, which is written
        to the game's standard input, which is then closed: the code's short life starts only once the
        game can read it. The code is never printed. After the handshake the game's output is
        discarded.
    #>
    [CmdletBinding()]
    param(
        [Parameter(Mandatory)]$Client,
        [Parameter(Mandatory)][scriptblock]$IssueCode
    )

    $contract = $Client.Handshake
    while ($Client.State -in 'Starting', 'AwaitingSignIn' -and $Client.PendingLine.IsCompleted) {
        $line = $Client.PendingLine.Result
        if ($null -eq $line) {
            $Client.State = 'Failed'
            $Client.Failure = 'the game stopped before signing in'
            break
        }
        if ($line -eq $contract.awaitingLaunchCode -and $Client.State -eq 'Starting') {
            try {
                $code = & $IssueCode
                $Client.Process.StandardInput.Write($code + "`n")
                $Client.Process.StandardInput.Flush()
                $Client.Process.StandardInput.Close()
                $Client.State = 'AwaitingSignIn'
            }
            catch {
                $Client.Process.StandardInput.Close()
                $Client.State = 'Failed'
                $Client.Failure = "no launch code: $($_.Exception.Message)"
                break
            }
            finally {
                $code = $null
            }
        }
        elseif ($line -eq $contract.signedIn) {
            $Client.State = 'SignedIn'
            break
        }
        elseif ($line.StartsWith($contract.failedPrefix + ' ')) {
            $Client.State = 'Failed'
            $Client.Failure = $line.Substring($contract.failedPrefix.Length + 1)
            break
        }
        $Client.PendingLine = $Client.Process.StandardOutput.ReadLineAsync()
    }
    if ($Client.State -in 'SignedIn', 'Failed' -and $Client.PendingLine) {
        $Client.PendingLine = $null
        $null = $Client.Process.StandardOutput.BaseStream.CopyToAsync([System.IO.Stream]::Null)
    }
    return $Client.State
}

<#
.SYNOPSIS
    Chooses the config the local backend starts with, through VEYRA_BACKEND_CONFIG, which compose.yaml reads.
.DESCRIPTION
    Without -Mode, the committed Backend/config/local.json. With it, Backend/config/scripted.json: that config
    with Mode's queue sized to -HumansPerTeam, so a script's few clients fill a queue whose canon is five
    humans a side (Modes Bible §1, §4; ADR-039 §6). -DodgeRestriction also shortens the queue restriction
    a player who leaves champion select takes (ADR-060), so a script can watch it end. Run it before
    docker compose up.
#>
function Set-VeyraBackendConfig {
    param(
        [Parameter(Mandatory)][string]$RepositoryDir,
        [string]$Mode,
        [int]$HumansPerTeam,
        [string]$DodgeRestriction
    )
    $configDir = Join-Path $RepositoryDir 'Backend\config'
    if (-not $Mode) {
        $env:VEYRA_BACKEND_CONFIG = 'local.json'
        return
    }
    $config = Get-Content -LiteralPath (Join-Path $configDir 'local.json') -Raw | ConvertFrom-Json
    $entry = $config.modes | Where-Object { $_.id -eq $Mode }
    if (-not $entry) {
        throw "Backend/config/local.json has no mode $Mode."
    }
    $entry.humanPlayersPerTeam = $HumansPerTeam
    if ($DodgeRestriction) {
        $config.dodges.restriction = $DodgeRestriction
    }
    $config | ConvertTo-Json -Depth 32 | Set-Content -LiteralPath (Join-Path $configDir 'scripted.json') -Encoding utf8NoBOM
    $env:VEYRA_BACKEND_CONFIG = 'scripted.json'
    Write-Host "The backend runs $Mode with $HumansPerTeam human(s) a side (Backend/config/scripted.json)."
}

Export-ModuleMember -Function Get-VeyraProjectFile, Resolve-VeyraEngineRoot, Initialize-VeyraPlatformToolchain, Get-VeyraLaunchHandshake,
    Start-VeyraHandshakeClient, Step-VeyraHandshake, Set-VeyraBackendConfig
