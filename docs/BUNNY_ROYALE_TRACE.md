# Bunny Royale Event Trace

This setup combines a read-only Frida managed-method trace with a read-only
mitmproxy HTTP(S) trace. Bunny Royale is traced as its roulette flow, not as
the unrelated Multichest or journey reward flow. It uses the IL2CPP resolver
shipped in `battle_cheat.exe_extracted`, but does not load `battle_cheat.js` or
invoke any game method.

## Install

```powershell
py -3 -m pip install --user mitmproxy
```

## Run

From the game's main menu, run this single command:

```powershell
powershell -ExecutionPolicy Bypass -File D:\VSCode\EL_Native\tools\run_bunny_royale_trace.ps1 -Seconds 180
```

The launcher starts `mitmdump`, changes a direct WinHTTP configuration to
`127.0.0.1:8080`, attaches Frida, and restores the original direct WinHTTP
setting when the trace stops or fails. When Windows denies that machine-level
change, it falls back to a temporary current-user Internet proxy and restores
the saved user settings on exit. If WinHTTP already uses a proxy, the launcher
leaves that setting intact. Enter the Bunny Royale event after the launcher
reports that the Frida trace is attached.

The default minimum interval is 1000 ms for each managed callback and each
HTTP/WebSocket flow key. To retain more detail, lower it; to disable the
debounce, use zero:

```powershell
powershell -ExecutionPolicy Bypass -File D:\VSCode\EL_Native\tools\run_bunny_royale_trace.ps1 -Seconds 1800 -MinIntervalMs 250
```

For a game process that is not already running, let the launcher start and
attach to it:

```powershell
powershell -ExecutionPolicy Bypass -File .\tools\run_bunny_royale_trace.ps1 -Seconds 180
    -SpawnGame
```

Use `-NoAutoProxy` only when an existing proxy configuration is intentional.

## Outputs

- `artifacts/bunny_royale_trace/frida.jsonl`: managed method entry/leave events,
  resolved addresses/RVAs, arguments as pointers, and dynamic event candidates.
- `artifacts/bunny_royale_trace/mitm.jsonl`: request/response and WebSocket
  metadata with sanitized JSON bodies. It records traffic only; it does not
  rewrite request or response bytes.

The Frida agent hooks the extracted roulette methods:
`CardRouletteModel.ParseRewards`, `OnSpinResponseReceived`,
`OnProgressResponseReceived`, and `ChooseRewardRouletteEventModule`'s lot,
choice, and delivery methods. It also scans classes containing
`CardRoulette`, `ChooseReward`, or `MiniEvent`; dynamic hooks are capped at 200
methods and reported in `candidate_classes` and `trace_ready` records.

For card-state correlation, the trace also catalogs and hooks the deck
controller, individual card element, and black-mark window. Each
`card_signal` record includes small integer arguments as `cardArgs`. When
the client passes an index from 0 through 3 to a selection method, the next
`OnSpinResponseReceived` record includes it as `lastCardSelection`. Compare
that record with the following reward parsing and black-mark window signals to
identify the selected card and its observed outcome.

Stop with Ctrl+C. The launcher stops only its own mitmproxy child and leaves an
already-running game process alone.
