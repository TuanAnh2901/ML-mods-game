# Everlusting Life — mitmproxy + Native DLL Setup Guide

## Overview
- **mitmproxy** intercepts HTTPS traffic between game and server
- Modifies sync responses to inject desired currency values
- Blocks error 700 (and similar) responses
- **Native DLL** (el_native.dll) monitors local state for display only

## Prerequisites
- Windows 10/11
- Python 3.10+ (recommend 3.13 to match Frida env)
- Game installed at `D:\SteamLibrary\steamapps\common\Everlusting Life\`
- Native DLL built at `D:\Temp\opencode\el_build\el_native.dll`

---

## Step 1: Install mitmproxy
```powershell
# Option A: pip (recommended)
py -3.13 -m pip install mitmproxy --upgrade

# Option B: Chocolatey
choco install mitmproxy

# Option C: Scoop
scoop install mitmproxy
```
Verify: `mitmproxy --version` → should show 10.x or 11.x

---

## Step 2: Install mitmproxy CA Certificate (CRITICAL)
The game uses HTTPS. mitmproxy must be trusted.

### 2A: Start mitmproxy once to generate certs
```powershell
mitmproxy
# Press 'q' to quit immediately after UI loads
```

### 2B: Trust the CA cert
```powershell
# Open cert manager
certmgr.msc
```
1. Navigate to **Trusted Root Certification Authorities** → **Certificates**
2. Right-click → **All Tasks** → **Import**
3. Browse to `%USERPROFILE%\.mitmproxy\mitmproxy-ca-cert.p12`
4. Password: **empty** (just press Enter)
5. Select "Place all certificates in the following store" → **Trusted Root Certification Authorities**
6. Finish → Restart game/launcher

**Alternative (PowerShell):**
```powershell
$cert = New-Object System.Security.Cryptography.X509Certificates.X509Certificate2(
    "$env:USERPROFILE\.mitmproxy\mitmproxy-ca-cert.p12", "", 
    [System.Security.Cryptography.X509Certificates.X509KeyStorageFlags]::Exportable
)
$store = New-Object System.Security.Cryptography.X509Certificates.X509Store(
    "Root", "LocalMachine"
)
$store.Open("ReadWrite")
$store.Add($cert)
$store.Close()
```

---

## Step 3: Configure System Proxy
The game must route traffic through mitmproxy (port 8080 by default).

### Option A: Windows System Proxy (affects all apps)
```powershell
# Enable
netsh winhttp set proxy 127.0.0.1:8080

# Disable after playing
netsh winhttp reset proxy
```

### Option B: Per-app via Proxifier / ProxyCap (recommended)
- Proxifier (paid) / ProxyCap (paid) / FreeCap (free, old)
- Rule: `Everlusting Life.exe` → `127.0.0.1:8080`

### Option C: Command line (if game respects HTTP_PROXY)
```cmd
set HTTP_PROXY=http://127.0.0.1:8080
set HTTPS_PROXY=http://127.0.0.1:8080
"D:\SteamLibrary\steamapps\common\Everlusting Life\Everlusting Life.exe"
```

---

## Step 4: Run mitmproxy with Script
```powershell
cd D:\VSCode\EL_Native
mitmproxy -s mitm_addon.py --set block_global=false
```
- `--set block_global=false` allows non-target traffic to pass through
- UI opens: press `q` to quit, `Enter` on a flow to inspect
- Logs show: `[EL] REQ #1 ...` and `[EL] RESP modified (total: 3)`

**Headless (no UI):**
```powershell
mitmdump -s mitm_addon.py --set block_global=false -q
```

---

## Step 5: Inject Native DLL
```cmd
cd D:\VSCode\EL_Native
D:\Temp\opencode\el_build\injector.exe D:\Temp\opencode\el_build\el_native.dll
```
- Game must be running
- Press **INSERT** to open overlay
- Enable **Currency** feature → shows monitored values
- **Real values** are set by mitmproxy (see config)

---

## Step 6: Customize Currency Values
Edit `D:\VSCode\EL_Native\mitm_config.json`:
```json
{
  "resource_overrides": {
    "gold": 999999,
    "gems": 999999,
    "energy": 999,
    "contribution": 999999,
    "elixir": 999999,
    "spin": 999
  }
}
```
- Change numbers → save → mitmproxy auto-reloads (or restart)
- Keys must match server response field names (case-sensitive)

---

## Troubleshooting

| Issue | Fix |
|-------|-----|
| Game fails to connect / cert error | Cert not trusted. Re-do Step 2B. Check `certmgr.msc` → Trusted Root → mitmproxy CA exists. |
| No `[EL]` logs in mitmproxy | Wrong host/port. Check game traffic in mitmproxy UI (flows list). Add host to `target_hosts` in config. |
| Error 700 still appears | Add `700` to `block_error_codes` (already default). Check mitmproxy logs for `[EL] Blocking error code 700`. |
| Currency values don't stick | Server sends different field names. Inspect flow in mitmproxy UI → Response → JSON → find actual field names → add to `resource_overrides`. |
| Game crashes on inject | Wrong DLL arch (x64 only). Rebuild: `cd D:\VSCode\EL_Native && build.bat`. |
| `mitmproxy` command not found | Add Python Scripts to PATH: `%USERPROFILE%\AppData\Local\Packages\PythonSoftwareFoundation.Python.3.13_...\LocalCache\local-packages\Python313\Scripts` |

---

## Quick Start Script (save as `start_el.ps1`)
```powershell
# start_el.ps1
cd D:\VSCode\EL_Native

# 1. Start mitmproxy in background
Start-Process mitmdump -ArgumentList "-s mitm_addon.py --set block_global=false -q" -WindowStyle Hidden

# 2. Set system proxy
netsh winhttp set proxy 127.0.0.1:8080

# 3. Launch game
Start-Process "D:\SteamLibrary\steamapps\common\Everlusting Life\Everlusting Life.exe"

# 4. Wait for game to start, then inject
Start-Sleep 10
& D:\Temp\opencode\el_build\injector.exe D:\Temp\opencode\el_build\el_native.dll

Write-Host "Running. Press Enter to cleanup..."
Read-Host

# Cleanup
netsh winhttp reset proxy
Get-Process mitmdump -ErrorAction SilentlyContinue | Stop-Process
```

Run: `powershell -ExecutionPolicy Bypass -File start_el.ps1`

---

## How It Works (Flow)

```
Game → HTTPS Request (profile/update) → mitmproxy → Server
Server → HTTPS Response (gold: 100) → mitmproxy → MODIFIES to gold: 999999 → Game
Game updates local ItemModule → ChangeResource fires → DLL logs it
Overlay shows: [1] gold = +999999
```

- Server never sees modified values (only receives normal requests)
- Game believes server gave it 999999 gold
- No error 700 because response is valid (we modified it before game sees it)
- Periodic full sync also gets modified → values persist

---

## Files
| File | Purpose |
|------|---------|
| `mitm_addon.py` | Main proxy script |
| `mitm_config.json` | User config (values, hosts, endpoints) |
| `el_native.dll` | Native mod (Currency monitor + other features) |
| `injector.exe` | DLL injector |