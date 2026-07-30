import csv
import io
import json
import pathlib
import subprocess
import threading

import frida

GAME_ROOT = pathlib.Path(r"D:\SteamLibrary\steamapps\common\Everlusting Life")
GAME_EXE = GAME_ROOT / "Everlusting Life.exe"
SCRIPT_PATH = GAME_ROOT / "Analysis" / "frida_custom_header_memory_access.js"
OUTPUT_PATH = GAME_ROOT / "Analysis" / "frida_custom_header_memory_access.jsonl"
TIMEOUT_SECONDS = 90


def running_game_pids() -> set[int]:
    result = subprocess.run(
        ["tasklist", "/FI", "IMAGENAME eq Everlusting Life.exe", "/FO", "CSV", "/NH"],
        check=False,
        capture_output=True,
        text=True,
    )
    return {
        int(row[1])
        for row in csv.reader(io.StringIO(result.stdout))
        if len(row) >= 2 and row[1].isdigit()
    }


def main() -> None:
    complete = threading.Event()
    device = frida.get_local_device()
    preexisting_game_pids = running_game_pids()
    pid = device.spawn([str(GAME_EXE)], cwd=str(GAME_ROOT))
    session = device.attach(pid)
    try:
        with OUTPUT_PATH.open("w", encoding="utf-8") as output:
            def on_message(message, _data) -> None:
                output.write(json.dumps(message, ensure_ascii=True) + "\n")
                output.flush()
                if message.get("type") == "send" and message.get("payload", {}).get("event") == "runtime_init_leave":
                    complete.set()

            script = session.create_script(SCRIPT_PATH.read_text(encoding="utf-8"))
            script.on("message", on_message)
            script.load()
            device.resume(pid)
            complete.wait(TIMEOUT_SECONDS)
    finally:
        session.detach()
        try:
            device.kill(pid)
        finally:
            subprocess.run(["taskkill", "/PID", str(pid), "/T", "/F"], check=False, capture_output=True)
            for game_pid in running_game_pids() - preexisting_game_pids:
                subprocess.run(["taskkill", "/PID", str(game_pid), "/T", "/F"], check=False, capture_output=True)


if __name__ == "__main__":
    main()
