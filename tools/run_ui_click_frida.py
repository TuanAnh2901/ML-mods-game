import argparse
import json
import pathlib
import signal
import subprocess
import sys
import threading
import time

import frida


ROOT = pathlib.Path(__file__).resolve().parents[1]
GAME_ROOT = pathlib.Path(r"D:\SteamLibrary\steamapps\common\Everlusting Life")
EXTRACTED = GAME_ROOT / "battle_cheat.exe_extracted"
GAME_EXE = GAME_ROOT / "Everlusting Life.exe"
RESOLVER = EXTRACTED / "il2cpp_resolve.js"
AGENT = ROOT / "tools" / "ui_click_trace_frida.js"
DEFAULT_OUT = ROOT / "artifacts" / "ui_click_trace" / "frida.jsonl"


def find_game(device: frida.core.Device):
    for process in device.enumerate_processes():
        if process.name.lower() in {"everlusting life.exe", "everlusting life"}:
            return process.pid
    return None


def write_message(output, message, data):
    record = {
        "source": "frida",
        "receivedAtMs": int(time.time() * 1000),
        "message": message,
    }
    if data:
        record["dataLength"] = len(data)
    output.write(json.dumps(record, ensure_ascii=True) + "\n")
    output.flush()
    payload = message.get("payload")
    if isinstance(payload, dict):
        event = payload.get("event")
        if event in {"hook_installed", "enter", "leave", "trace_ready"}:
            print(json.dumps(payload, ensure_ascii=True), flush=True)


def main() -> int:
    parser = argparse.ArgumentParser(description="Attach a read-only Frida UI click + reward-flow trace")
    parser.add_argument("--pid", type=int, help="Existing game PID; otherwise find it by executable name")
    parser.add_argument("--spawn", action="store_true", help="Spawn the game when no existing process is found")
    parser.add_argument("--out", type=pathlib.Path, default=DEFAULT_OUT)
    parser.add_argument("--seconds", type=int, default=0, help="Stop after N seconds; 0 means Ctrl+C")
    parser.add_argument("--min-interval-ms", type=int, default=0, help="Minimum interval per managed callback")
    args = parser.parse_args()

    if not RESOLVER.is_file() or not AGENT.is_file():
        raise FileNotFoundError("Resolver or Frida agent is missing")

    args.out.parent.mkdir(parents=True, exist_ok=True)
    device = frida.get_local_device()
    pid = args.pid or find_game(device)
    spawned = False
    if pid is None and args.spawn:
        pid = device.spawn([str(GAME_EXE)], cwd=str(GAME_ROOT))
        spawned = True
    if pid is None:
        raise RuntimeError("Game process not found; start it or pass --spawn")

    if args.min_interval_ms < 0:
        parser.error("--min-interval-ms must be zero or greater")
    source = (
        RESOLVER.read_text(encoding="utf-8")
        + f"\nvar TRACE_MIN_INTERVAL_MS = {args.min_interval_ms};\n"
        + AGENT.read_text(encoding="utf-8")
    )
    stop = threading.Event()
    session = device.attach(pid)
    output = args.out.open("w", encoding="utf-8")
    try:
        script = session.create_script(source)
        script.on("message", lambda message, data: write_message(output, message, data))
        script.load()
        if spawned:
            device.resume(pid)
        print(f"Frida attached pid={pid}; output={args.out}", flush=True)
        if args.seconds > 0:
            stop.wait(args.seconds)
        else:
            signal.signal(signal.SIGINT, lambda *_: stop.set())
            signal.signal(signal.SIGTERM, lambda *_: stop.set())
            stop.wait()
    finally:
        output.close()
        session.detach()
        if spawned:
            try:
                device.kill(pid)
            except frida.InvalidOperationError:
                pass
    return 0


if __name__ == "__main__":
    sys.exit(main())
