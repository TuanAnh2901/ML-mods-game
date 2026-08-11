import hashlib
import json
import os
import re
import threading
import time
from pathlib import Path

from mitmproxy import ctx, http


OUT = Path(os.environ.get("BUNNY_TRACE_OUT", "artifacts/bunny_royale_trace/mitm.jsonl"))
HOST_RE = re.compile(os.environ.get("BUNNY_TRACE_HOST_RE", ".*"), re.I)
EVENT_RE = re.compile(r"bunny|royale|roulette|card|lot|delivery|black[_ -]?mark|choose[_ -]?reward|mini[_ -]?event", re.I)
SECRET_KEYS = re.compile(r"token|auth|cookie|session|jwt|password|secret|refresh|access", re.I)
LOCK = threading.Lock()
MIN_INTERVAL_MS = max(0, int(os.environ.get("BUNNY_TRACE_MIN_INTERVAL_MS", "1000")))
LAST_RECORD_AT = {}


def scrub(value):
    if isinstance(value, dict):
        return {key: "<redacted>" if SECRET_KEYS.search(str(key)) else scrub(item)
                for key, item in value.items()}
    if isinstance(value, list):
        return [scrub(item) for item in value]
    return value


def body_summary(content):
    if not content:
        return {"bytes": 0, "sha256": None, "json": None, "matched": []}
    text = content.decode("utf-8", errors="replace")
    parsed = None
    try:
        parsed = scrub(json.loads(text))
    except (ValueError, TypeError):
        pass
    matches = sorted(set(EVENT_RE.findall(text)))
    return {
        "bytes": len(content),
        "sha256": hashlib.sha256(content).hexdigest(),
        "json": parsed,
        "textMatched": matches,
    }


def should_record(direction, host, method, path):
    if MIN_INTERVAL_MS == 0:
        return True
    key = (direction, host, method, path)
    now = int(time.monotonic() * 1000)
    with LOCK:
        previous = LAST_RECORD_AT.get(key, 0)
        if now - previous < MIN_INTERVAL_MS:
            return False
        LAST_RECORD_AT[key] = now
    return True


def record(flow, direction):
    host = flow.request.pretty_host
    if not HOST_RE.search(host):
        return
    request = flow.request
    if not should_record(direction, host, request.method, request.path):
        return
    response = flow.response
    item = {
        "source": "mitm",
        "tsMs": int(time.time() * 1000),
        "direction": direction,
        "host": host,
        "method": request.method,
        "path": request.path,
        "request": body_summary(request.raw_content),
        "response": None if response is None else {
            "status": response.status_code,
            "contentType": response.headers.get("content-type", ""),
            "body": body_summary(response.raw_content),
        },
    }
    OUT.parent.mkdir(parents=True, exist_ok=True)
    with LOCK:
        with OUT.open("a", encoding="utf-8") as output:
            output.write(json.dumps(item, ensure_ascii=True, separators=(",", ":")) + "\n")
    matched = item["request"]["textMatched"]
    if item["response"]:
        matched += item["response"]["body"]["textMatched"]
    if matched:
            ctx.log.info("[BUNNY-TRACE] %s %s %s matched=%s status=%s",
                     request.method, host, request.path, sorted(set(matched)),
                     response.status_code if response else "-")


def record_websocket(flow, message):
    host = flow.request.pretty_host
    if not HOST_RE.search(host):
        return
    if not should_record("websocket_client" if message.from_client else "websocket_server", host, flow.request.method, flow.request.path):
        return
    content = message.content
    summary = body_summary(content)
    item = {
        "source": "mitm",
        "tsMs": int(time.time() * 1000),
        "direction": "websocket_client" if message.from_client else "websocket_server",
        "host": host,
        "method": flow.request.method,
        "path": flow.request.path,
        "body": summary,
    }
    OUT.parent.mkdir(parents=True, exist_ok=True)
    with LOCK:
        with OUT.open("a", encoding="utf-8") as output:
            output.write(json.dumps(item, ensure_ascii=True, separators=(",", ":")) + "\n")
    if summary["textMatched"]:
        ctx.log.info("[BUNNY-TRACE] websocket %s %s matched=%s",
                     host, flow.request.path, summary["textMatched"])


class BunnyRoyaleTrace:
    def request(self, flow: http.HTTPFlow):
        record(flow, "request")

    def response(self, flow: http.HTTPFlow):
        record(flow, "response")

    def websocket_message(self, flow: http.HTTPFlow):
        if flow.websocket and flow.websocket.messages:
            record_websocket(flow, flow.websocket.messages[-1])


addons = [BunnyRoyaleTrace()]
