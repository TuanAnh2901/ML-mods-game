param(
    [int]$Seconds = 0,
    [Alias('Pid')]
    [int]$GamePid = 0,
    [ValidateRange(0, 60000)]
    [int]$MinIntervalMs = 1000,
    [ValidateSet('observe', 'flip_black_mark', 'increment_first_reward', 'malformed_result')]
    [string]$StagingIntegrityProfile = 'observe',
    [string]$StagingHost = 'localhost',
    [switch]$NoAutoProxy,
    [switch]$SpawnGame
)

$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
$out = Join-Path $root 'artifacts\bunny_royale_trace'
New-Item -ItemType Directory -Force -Path $out | Out-Null
$proxyChanged = $false
$proxyWasDirect = $false
$proxyMode = 'none'
$userProxyPath = 'HKCU:\Software\Microsoft\Windows\CurrentVersion\Internet Settings'
$userProxyBefore = @{}
$mitm = $null

function Test-LoopbackHost {
    param([string]$HostName)
    if ($HostName -in @('localhost', '127.0.0.1', '::1')) { return $true }
    try {
        $addresses = [System.Net.Dns]::GetHostAddresses($HostName)
        return $addresses.Count -gt 0 -and @($addresses | Where-Object { -not [System.Net.IPAddress]::IsLoopback($_) }).Count -eq 0
    }
    catch { return $false }
}

function Update-UserProxySettings {
    if (-not ('WinInetSettings' -as [type])) {
        Add-Type @'
using System;
using System.Runtime.InteropServices;
public static class WinInetSettings {
    [DllImport("wininet.dll", SetLastError = true)]
    public static extern bool InternetSetOption(IntPtr handle, int option, IntPtr buffer, int length);
}
'@
    }
    [WinInetSettings]::InternetSetOption([IntPtr]::Zero, 39, [IntPtr]::Zero, 0) | Out-Null
    [WinInetSettings]::InternetSetOption([IntPtr]::Zero, 37, [IntPtr]::Zero, 0) | Out-Null
}

function Save-UserProxySettings {
    foreach ($name in @('ProxyEnable', 'ProxyServer', 'ProxyOverride', 'AutoConfigURL')) {
        $property = Get-ItemProperty -LiteralPath $userProxyPath -Name $name -ErrorAction SilentlyContinue
        $userProxyBefore[$name] = [PSCustomObject]@{
            Exists = $null -ne $property
            Value = if ($null -ne $property) { $property.$name } else { $null }
        }
    }
}

function Restore-UserProxySettings {
    foreach ($name in $userProxyBefore.Keys) {
        $saved = $userProxyBefore[$name]
        if ($saved.Exists) {
            Set-ItemProperty -LiteralPath $userProxyPath -Name $name -Value $saved.Value
        }
        else {
            Remove-ItemProperty -LiteralPath $userProxyPath -Name $name -ErrorAction SilentlyContinue
        }
    }
    Update-UserProxySettings
}

try {
    if (-not $NoAutoProxy) {
        $proxyState = (& netsh winhttp show proxy | Out-String)
        $proxyWasDirect = $proxyState -match 'Direct access \(no proxy server\)'
        if ($proxyWasDirect) {
            & netsh winhttp set proxy '127.0.0.1:8080' '<local>' | Out-Host
            if ($LASTEXITCODE -eq 0) {
                $proxyChanged = $true
                $proxyMode = 'winhttp'
                Write-Host '[BUNNY-TRACE] WinHTTP proxy enabled at 127.0.0.1:8080.'
            }
            else {
                # netsh requires elevation; use the current-user WinINet setting instead.
                Write-Warning 'WinHTTP proxy requires elevation. Using a temporary current-user proxy instead.'
                Save-UserProxySettings
                Set-ItemProperty -LiteralPath $userProxyPath -Name ProxyEnable -Type DWord -Value 1
                Set-ItemProperty -LiteralPath $userProxyPath -Name ProxyServer -Value '127.0.0.1:8080'
                Set-ItemProperty -LiteralPath $userProxyPath -Name ProxyOverride -Value '<local>'
                Remove-ItemProperty -LiteralPath $userProxyPath -Name AutoConfigURL -ErrorAction SilentlyContinue
                Update-UserProxySettings
                $proxyChanged = $true
                $proxyMode = 'wininet'
                Write-Host '[BUNNY-TRACE] Current-user proxy enabled at 127.0.0.1:8080.'
            }
        }
        else {
            Write-Warning 'WinHTTP already has a proxy; leaving it unchanged. Use -NoAutoProxy to suppress this check.'
        }
    }

    $userSite = (& py -3 -c "import site; print(site.getusersitepackages())").Trim()
    $mitmdump = Join-Path (Split-Path $userSite -Parent) 'Scripts\mitmdump.exe'
    if (!(Test-Path -LiteralPath $mitmdump)) { throw "mitmdump.exe not found: $mitmdump" }
    $mitmScript = Join-Path $PSScriptRoot 'bunny_royale_mitm_trace.py'
    if ($StagingIntegrityProfile -ne 'observe') {
        if (-not (Test-LoopbackHost $StagingHost)) { throw 'StagingIntegrityProfile requires a localhost or loopback-resolved StagingHost.' }
        $mitmScript = Join-Path $PSScriptRoot 'bunny_roulette_staging_integrity.py'
    }
    $mitmArgs = @('-s', $mitmScript, '--listen-host', '127.0.0.1', '--listen-port', '8080', '--set', 'block_global=false', '-q')
    $mitm = Start-Job -ScriptBlock {
        param($exe, $args, $workingDirectory, $minIntervalMs, $integrityProfile, $integrityHost)
        Set-Location -LiteralPath $workingDirectory
        $env:BUNNY_TRACE_MIN_INTERVAL_MS = $minIntervalMs
        $env:BUNNY_QA_PROFILE = $integrityProfile
        $env:BUNNY_QA_APPLY = if ($integrityProfile -eq 'observe') { '0' } else { '1' }
        $env:BUNNY_QA_ALLOWED_HOSTS = $integrityHost
        $env:BUNNY_QA_OUT = Join-Path $workingDirectory 'artifacts\bunny_royale_trace\staging_integrity.jsonl'
        & $exe @args
    } -ArgumentList @($mitmdump, (,$mitmArgs), $root, $MinIntervalMs, $StagingIntegrityProfile, $StagingHost)

    $fridaArgs = @((Join-Path $PSScriptRoot 'run_bunny_royale_frida.py'))
    if ($GamePid -gt 0) { $fridaArgs += @('--pid', $GamePid) }
    if ($SpawnGame) { $fridaArgs += '--spawn' }
    if ($Seconds -gt 0) { $fridaArgs += @('--seconds', $Seconds) }
    $fridaArgs += @('--min-interval-ms', $MinIntervalMs)
    & py -3 @fridaArgs
    if ($LASTEXITCODE -ne 0) { throw "Frida runner exited with $LASTEXITCODE" }
}
finally {
    if ($mitm) {
        if ($mitm.State -eq 'Running') { Stop-Job -Job $mitm -ErrorAction SilentlyContinue }
        Remove-Job -Job $mitm -Force -ErrorAction SilentlyContinue
    }
    if ($proxyMode -eq 'winhttp' -and $proxyChanged -and $proxyWasDirect) {
        & netsh winhttp reset proxy | Out-Host
    }
    elseif ($proxyMode -eq 'wininet' -and $proxyChanged) {
        Restore-UserProxySettings
    }
}
