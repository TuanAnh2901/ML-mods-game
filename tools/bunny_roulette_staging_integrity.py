"""Loopback-only response-integrity test addon for staging Card Roulette.

This addon is intentionally restricted to a loopback staging host. It records
the original and modified response hashes so a QA run can establish whether
the current client accepts a malformed or altered server result.
"""

import hashlib
import json
import os
import socket
import threading
import time
from pathlib import Path

from mitmproxy import ctx, http


OUT = Path(os.environ.get("BUNNY_QA_OUT", "artifacts/bunny_royale_trace/staging_integrity.jsonl"))
PROFILE = os.environ.get("BUNNY_QA_PROFILE", "observe")
APPLY = os.environ.get("BUNNY_QA_APPLY", "0") == "1"
ALLOWED_HOSTS = {item.strip().lower() for item in os.environ.get("BUNNY_QA_ALLOWED_HOSTS", "localhost,127.0.0.1,::1").split(",") if item.strip()}
PROFILES = {"observe", "flip_black_mark", "increment_first_reward", "malformed_result"}
LOCK = threading.Lock()


def sha256_bytes(value):
    return hashlib.sha256(value).hexdigest()


def is_loopback_host(host):
    host = host.lower().strip().strip("[]")
    if host in {"localhost", "127.0.0.1", "::1"}:
        return True
    try:
        addresses = socket.getaddrinfo(host, None, type=socket.SOCK_STREAM)
    except socket.gaierror:
        return False
    return bool(addresses) and all(item[4][0] in {"127.0.0.1", "::1"} for item in addresses)


def is_target(flow):
    host = flow.request.pretty_host.lower()
    if host not in ALLOWED_HOSTS or not is_loopback_host(host):
        return False
    return bool(flow.response and flow.response.raw_content)


def mutate_payload(payload, profile):
    """Return a deep-copied QA fixture and the fields changed."""
    result = json.loads(json.dumps(payload))
    if not isinstance(result, dict) or not isinstance(result.get("result"), dict):
        return result, []
    data = result["result"]
    if profile == "observe":
        return result, []
    if profile == "flip_black_mark" and isinstance(data.get("is_black_mark"), bool):
        data["is_black_mark"] = not data["is_black_mark"]
        return result, ["result.is_black_mark"]
    if profile == "increment_first_reward":
        rewards = data.get("rewards")
        if isinstance(rewards, list) and rewards and isinstance(rewards[0], dict):
            quantity = rewards[0].get("quantity")
            if isinstance(quantity, str) and quantity.isdecimal():
                rewards[0]["quantity"] = str(int(quantity) + 1)
                return result, ["result.rewards[0].quantity"]
        return result, []
    if profile == "malformed_result":
        data.pop("rewards", None)
        data["is_black_mark"] = "qa-invalid-type"
        return result, ["result.rewards", "result.is_black_mark"]
    return result, []


def append_record(record):
    OUT.parent.mkdir(parents=True, exist_ok=True)
    with LOCK:
        with OUT.open("a", encoding="utf-8") as output:
            output.write(json.dumps(record, ensure_ascii=True, separators=(",", ":")) + "\n")


class StagingIntegrityAddon:
    def response(self, flow: http.HTTPFlow):
        if not is_target(flow):
            return
        original = flow.response.raw_content
        try:
            payload = json.loads(original.decode("utf-8"))
        except (UnicodeDecodeError, json.JSONDecodeError):
            return
        mutated, changed = mutate_payload(payload, PROFILE)
        modified = json.dumps(mutated, ensure_ascii=True, separators=(",", ":")).encode("utf-8")
        applied = APPLY and bool(changed)
        if applied:
            flow.response.content = modified
        record = {
            "source": "staging_integrity",
            "tsMs": int(time.time() * 1000),
            "flowId": flow.id,
            "host": flow.request.pretty_host,
            "method": flow.request.method,
            "path": flow.request.path,
            "status": flow.response.status_code,
            "profile": PROFILE,
            "applied": applied,
            "changedFields": changed,
            "originalSha256": sha256_bytes(original),
            "modifiedSha256": sha256_bytes(modified),
        }
        append_record(record)
        ctx.log.info("[BUNNY-QA] profile=%s applied=%s fields=%s path=%s", PROFILE, applied, changed, flow.request.path)


if PROFILE not in PROFILES:
    raise ValueError(f"Unsupported BUNNY_QA_PROFILE: {PROFILE}")

addons = [StagingIntegrityAddon()]
