import json
import pathlib
import threading

import frida

GAME_ROOT = pathlib.Path(r"D:\SteamLibrary\steamapps\common\Everlusting Life")
GAME_EXE = GAME_ROOT / "Everlusting Life.exe"
SCRIPT_PATH = GAME_ROOT / "Analysis" / "frida_metadata_loader_trace.js"
OUTPUT_PATH = GAME_ROOT / "Analysis" / "frida_metadata_loader_trace.jsonl"
MAGIC_DUMP_PATH = GAME_ROOT / "Analysis" / "global-metadata.runtime.dat"
TIMEOUT_SECONDS = 120


def main() -> None:
    completed = threading.Event()
    device = frida.get_local_device()
    pid = device.spawn([str(GAME_EXE)], cwd=str(GAME_ROOT))
    session = device.attach(pid)

    try:
        with OUTPUT_PATH.open("w", encoding="utf-8") as output:
            def on_message(message, _data) -> None:
                output.write(json.dumps(message, ensure_ascii=True) + "\n")
                output.flush()
                if message.get("type") == "send" and message.get("payload", {}).get("event") == "metadata_magic_pre_free":
                    MAGIC_DUMP_PATH.write_bytes(_data)
                if message.get("type") == "send" and message.get("payload", {}).get("event") == "runtime_init_leave":
                    completed.set()

            script = session.create_script(SCRIPT_PATH.read_text(encoding="utf-8"))
            script.on("message", on_message)
            script.load()
            device.resume(pid)
            completed.wait(TIMEOUT_SECONDS)
    finally:
        session.detach()
        device.kill(pid)


if __name__ == "__main__":
    main()
