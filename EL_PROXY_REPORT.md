# el_proxy.py — EL_Native HTTPS Interception Proxy (battle_cheat mechanics)

Date: 2026-08-10
Status: DONE — self-check PASS + integration verification PASS

## Purpose
Replace the mitmproxy dependency with a standalone HTTPS proxy that replicates
the working mechanics of `battle_cheat.exe_extracted`, operating side-by-side
with EL_Native (DLL hooks traffic-local; this proxy owns the wire).

## Files
| File | Role |
|---|---|
| `el_proxy.py` | Standalone proxy (stdlib only: http.server, ssl, urllib, gzip) |
| `el_proxy.json` | Config, auto-created on first run |
| `adultchess.crt` / `.key` | Copied from `battle_cheat.exe_extracted` (SAN covers `*.adult-chess.com`) |
| `el_proxy.log` | Runtime log |

## Mechanics (verified against extracted evidence)
1. **Hosts redirect** — mirrors `_start_proxy`: comments/uncomments/removes
   `127.0.0.1 <host>` in `C:\Windows\System32\drivers\etc\hosts` (idempotent).
2. **DNS loopback fallback** — same `FALLBACK_IPS`
   (`ga/ru/adult-chess.com → 166.117.25.255`), `CERT_NONE` upstream TLS.
3. **TLS :443 terminate** — same cert/key as battle_cheat; auto-install to
   trusted root store via certutil (admin).
4. **Request mutation** — `profile/update` `ApplyCardRouletteSpinRewards`
   rewrite: black_mark passthrough, step_id dedup, reward_type/id/qty injection
   (disabled by default — `roulette.enabled`).
5. **Response mutation** — mitm_addon parity: nested resource paths
   (`userData/profile/data/result/resources`), error-code blocking
   (700/701/702/703/801/802 → success), gzip transparent re-encode.

## Parity evidence
- Request rules ↔ `ROULETTE_TRAFFIC_ANNOTATED.md` (lines 1155-1239)
- Response rules ↔ `mitm_addon.py` (field-for-field: nested_paths, error_code,
  success/message rewrite)
- Fallback IPs / hosts path / port 443 ↔ `start_proxy_dis.txt` + annotated md

## Verification (no admin, no prod contact)
```
el_proxy self-check: PASS          # cert/key present, mutation logic, DNS fallback
[1] TLS handshake + proxy handler OK (502 = upstream refused, expected)
[2] hosts add/remove/idempotency OK (temp file only)
[3] commented-entry uncomment OK
```

## Usage
```powershell
py -3.13 el_proxy.py --start      # foreground: blocks this terminal (keep open)
py -3.13 el_proxy.py --start-bg   # background: detached, survives terminal close
py -3.13 el_proxy.py --status     # shows running/stopped + pid
py -3.13 el_proxy.py --stop       # kills proxy + restores hosts
py -3.13 el_proxy.py --test       # self-check, no side effects
```

## Caveats / next steps
- `--start` before 2026-08-10 18:04 exited immediately (daemon thread died with
  process) — fixed: foreground now blocks, `--start-bg` spawns detached child
  with PID file (`el_proxy.pid`).
- Hosts file on this machine is `System+Hidden` (attrib shows `A SH`): proxy
  clears attributes elevated, falls back to atomic `hosts.tmp` replace.
- Upstream uses HTTP/1.1 (urllib) — game client speaks HTTP/1.1; HTTP/2 needed
  only if server negotiates it (not observed).
- Response mutation is best-effort JSON; non-JSON bodies pass through.
