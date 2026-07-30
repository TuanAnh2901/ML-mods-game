param(
    [Parameter(Mandatory)] [int] $ProcessId,
    [Parameter(Mandatory)] [string] $OutputPath,
    [Parameter(Mandatory)] [UInt64] $ExpectedLength
)

$source = @'
using System;
using System.Runtime.InteropServices;

public static class NativeMemory
{
    [StructLayout(LayoutKind.Sequential)]
    public struct MemoryBasicInformation
    {
        public IntPtr BaseAddress;
        public IntPtr AllocationBase;
        public uint AllocationProtect;
        public UIntPtr RegionSize;
        public uint State;
        public uint Protect;
        public uint Type;
    }

    [DllImport("kernel32.dll", SetLastError = true)]
    public static extern IntPtr OpenProcess(uint access, bool inheritHandle, int processId);

    [DllImport("kernel32.dll", SetLastError = true)]
    public static extern bool CloseHandle(IntPtr handle);

    [DllImport("kernel32.dll", SetLastError = true)]
    public static extern UIntPtr VirtualQueryEx(IntPtr process, IntPtr address, out MemoryBasicInformation information, UIntPtr length);

    [DllImport("kernel32.dll", SetLastError = true)]
    public static extern bool ReadProcessMemory(IntPtr process, IntPtr address, byte[] buffer, UIntPtr size, out UIntPtr bytesRead);
}
'@

Add-Type -TypeDefinition $source

$process = [NativeMemory]::OpenProcess(0x0410, $false, $ProcessId)
if ($process -eq [IntPtr]::Zero) {
    throw "OpenProcess failed: $([Runtime.InteropServices.Marshal]::GetLastWin32Error())"
}

try {
    $magic = [byte[]](0xAF, 0x1B, 0xB1, 0xFA)
    $maxAddress = [UInt64]0x00007FFFFFFEFFFF
    $address = [UInt64]0
    $chunkSize = 1MB

    while ($address -lt $maxAddress) {
        $info = [NativeMemory+MemoryBasicInformation]::new()
        $infoSize = [UIntPtr]::new([UInt64][Runtime.InteropServices.Marshal]::SizeOf($info))
        $queried = [NativeMemory]::VirtualQueryEx($process, [IntPtr]::new([Int64]$address), [ref]$info, $infoSize)
        if ($queried -eq [UIntPtr]::Zero) {
            break
        }

        $base = [UInt64]$info.BaseAddress.ToInt64()
        $length = $info.RegionSize.ToUInt64()
        $readable = $info.State -eq 0x1000 -and (($info.Protect -band 0x101) -eq 0)
        if ($readable -and $length -ge 8) {
            $cursor = $base
            $remaining = $length
            while ($remaining -gt 0) {
                $count = [Math]::Min([UInt64]$chunkSize, $remaining)
                $buffer = [byte[]]::new([int]$count)
                $read = [UIntPtr]::Zero
                $readSize = [UIntPtr]::new([UInt64]$count)
                if ([NativeMemory]::ReadProcessMemory($process, [IntPtr]::new([Int64]$cursor), $buffer, $readSize, [ref]$read)) {
                    $readCount = [int]$read.ToUInt64()
                    $searchStart = 0
                    while ($searchStart -le $readCount - 8) {
                        $i = [Array]::IndexOf($buffer, $magic[0], $searchStart)
                        if ($i -lt 0 -or $i -gt $readCount - 8) {
                            break
                        }
                        $searchStart = $i + 1
                        if ($buffer[$i] -ne $magic[0] -or $buffer[$i + 1] -ne $magic[1] -or $buffer[$i + 2] -ne $magic[2] -or $buffer[$i + 3] -ne $magic[3]) {
                            continue
                        }

                        $candidate = $cursor + [UInt64]$i
                        $version = [BitConverter]::ToUInt32($buffer, $i + 4)
                        if ($version -lt 20 -or $version -gt 200) {
                            continue
                        }

                        $dump = [byte[]]::new([int]$ExpectedLength)
                        $dumpRead = [UIntPtr]::Zero
                        $dumpSize = [UIntPtr]::new($ExpectedLength)
                        if ([NativeMemory]::ReadProcessMemory($process, [IntPtr]::new([Int64]$candidate), $dump, $dumpSize, [ref]$dumpRead) -and $dumpRead.ToUInt64() -eq $ExpectedLength) {
                            [IO.File]::WriteAllBytes($OutputPath, $dump)
                            [pscustomobject]@{ Address = ('0x{0:X}' -f $candidate); Version = $version; Bytes = $ExpectedLength; Output = $OutputPath }
                            return
                        }
                    }
                }
                $cursor += $count
                $remaining -= $count
            }
        }

        $next = $base + $length
        if ($next -le $address) {
            break
        }
        $address = $next
    }

    throw 'No readable in-memory IL2CPP metadata header matched the expected magic/version.'
}
finally {
    [void][NativeMemory]::CloseHandle($process)
}
