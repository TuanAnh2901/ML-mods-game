import json
import pathlib
import threading
import time

import frida

GAME_ROOT = pathlib.Path(r"D:\SteamLibrary\steamapps\common\Everlusting Life")
GAME_EXE = GAME_ROOT / "Everlusting Life.exe"
SCRIPT_PATH = GAME_ROOT / "Analysis" / "frida_il2cpp_init_trace.js"
OUTPUT_PATH = GAME_ROOT / "Analysis" / "frida_il2cpp_init_trace.jsonl"
TIMEOUT_SECONDS = 120


def main() -> None:
    completed = threading.Event()
    output = OUTPUT_PATH.open("w", encoding="utf-8")
    device = frida.get_local_device()
    pid = device.spawn([str(GAME_EXE)], cwd=str(GAME_ROOT))
    session = device.attach(pid)

    def on_message(message, _data) -> None:
        output.write(json.dumps(message, ensure_ascii=True) + "\n")
        output.flush()
        if message.get("type") == "send" and message.get("payload", {}).get("event") == "il2cpp_init_leave":
            completed.set()

    script = session.create_script(SCRIPT_PATH.read_text(encoding="utf-8"))
    script.on("message", on_message)
    script.load()
    device.resume(pid)

    if not completed.wait(TIMEOUT_SECONDS):
        output.write(json.dumps({"type": "status", "payload": {"event": "timeout", "seconds": TIMEOUT_SECONDS}}) + "\n")
    output.close()
    session.detach()


if __name__ == "__main__":
    main()
