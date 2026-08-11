#!/usr/bin/env py -3.13
"""el_proxy.py — standalone HTTPS interception proxy for Everlusting Life.

Replicates the working proxy mechanics of the extracted battle_cheat tool:

  1. Redirects the selected game host to 127.0.0.1 in the Windows hosts file.
  2. Terminates TLS on :443 with the bundled adultchess cert/key.
  3. Forwards every request to the REAL server IP (Host header preserved).
  4. Mutates request JSON (profile/update ApplyCardRouletteSpinRewards) — same
     reward-rewrite rules as battle_cheat.
  5. Mutates response JSON — resource overrides + error-code blocking, the
     part that el_native's mitm_addon.py previously did inside mitmproxy.

Works side-by-side with EL_Native: the DLL observes local state, this script
owns the traffic. No mitmproxy required. Config: el_proxy.json (auto-created).

Usage:
    py -3.13 el_proxy.py --start [--config el_proxy.json]
    py -3.13 el_proxy.py --stop
    py -3.13 el_proxy.py --status
    py -3.13 el_proxy.py --test   (self-check, no admin/hosts changes)
"""

import argparse
import gzip
import json
import os
import re
import socket
import ssl
import subprocess
import sys
import threading
import time
import traceback
from http.server import BaseHTTPRequestHandler, HTTPServer
from pathlib import Path
from socketserver import ThreadingMixIn
from urllib import request as urlrequest
from urllib import error as urlerror

BASE_DIR = Path(__file__).resolve().parent
CONFIG_PATH = BASE_DIR / "el_proxy.json"
LOG_PATH = BASE_DIR / "el_proxy.log"
HOSTS_PATH = Path(r"C:\Windows\System32\drivers\etc\hosts")

DEFAULT_CONFIG = {
    "enabled": True,
    "server_host": "ga.adult-chess.com",
    "fallback_ips": {
        "ga.adult-chess.com": "166.117.25.255",
        "ru.adult-chess.com": "166.117.25.255",
        "adult-chess.com": "166.117.25.255",
    },
    "cert": str(BASE_DIR / "adultchess.crt"),
    "key": str(BASE_DIR / "adultchess.key"),
    "port": 443,
    "upstream_timeout": 30,
    # Response mutation (mitm_addon behaviour).
    "resource_overrides": {
        "gold": 999999,
        "gems": 999999,
        "energy": 999,
        "contribution": 999999,
        "elixir": 999999,
        "spin": 999,
    },
    "block_error_codes": [700, 701, 702, 703, 801, 802],
    # Request mutation (battle_cheat roulette behaviour).
    "roulette": {
        "enabled": False,
        "reward_type": "monster",
        "reward_id": "",
        "reward_qty": 1,
        "action_name": "ApplyCardRouletteSpinRewards",
    },
    "log_requests": True,
    "log_responses": False,
    "capture_dir": "",
}

FALLBACK_IPS = {
    "ga.adult-chess.com": "166.117.25.255",
    "ru.adult-chess.com": "166.117.25.255",
    "adult-chess.com": "166.117.25.255",
}


def load_config():
    if CONFIG_PATH.exists():
        try:
            user = json.loads(CONFIG_PATH.read_text(encoding="utf-8"))
            cfg = {**DEFAULT_CONFIG, **user}
            cfg["resource_overrides"] = {**DEFAULT_CONFIG["resource_overrides"],
                                         **user.get("resource_overrides", {})}
            cfg["roulette"] = {**DEFAULT_CONFIG["roulette"],
                               **user.get("roulette", {})}
            return cfg
        except Exception as e:
            log(f"[!] Config load error: {e}, using defaults")
    else:
        CONFIG_PATH.write_text(json.dumps(DEFAULT_CONFIG, indent=2, ensure_ascii=False),
                               encoding="utf-8")
    return dict(DEFAULT_CONFIG)


def log(msg):
    line = f"[{time.strftime('%H:%M:%S')}] {msg}"
    try:
        with LOG_PATH.open("a", encoding="utf-8") as f:
            f.write(line + "\n")
    except OSError:
        pass
    print(line, flush=True)


# Rewards observed to be accepted by the server on roulette claims.  hammer
# (id 130) and hammer_ranking (131) are ranking/event resources that the
# server rejects with 409 "validation failed"; keep a warning list so a
# misconfigured reward fails loudly instead of producing a silent 409 loop.
KNOWN_ROULETTE_REWARDS = {
    "resource": {"gold", "gems", "energy", "contribution", "elixir", "spin"},
    "item": {"relationship_gift_rare_3", "minievents_card_roulette_resource",
             "group:relationship_gifts_common", "scroll_common"},
}


def warn_roulette_config(cfg):
    roulette = cfg.get("roulette", {})
    if not roulette.get("enabled"):
        return
    rtype = roulette.get("reward_type", "")
    rid = roulette.get("reward_id", "")
    known = KNOWN_ROULETTE_REWARDS.get(rtype, set())
    if rid and rid not in known:
        log(f"[!] WARNING roulette reward {rtype}/{rid} not in known-accepted "
            f"set {KNOWN_ROULETTE_REWARDS.get(rtype, '?')}; server may reply 409")


# ── Hosts redirect (mirrors battle_cheat _start_proxy) ───────────────────────

def _norm(line):
    return " ".join(line.lstrip().lstrip("#").split())


def _write_hosts_safely(text):
    if sys.platform == "win32":
        subprocess.run(["attrib", "-s", "-h", str(HOSTS_PATH)],
                       capture_output=True, check=False)
        try:
            HOSTS_PATH.write_bytes(text)
            return
        except PermissionError:
            pass
        tmp = HOSTS_PATH.with_suffix(".tmp")
        tmp.write_bytes(text)
        try:
            os.replace(tmp, HOSTS_PATH)
            return
        except OSError:
            tmp.unlink(missing_ok=True)
            raise
    HOSTS_PATH.write_bytes(text)


def update_hosts(server_host, add):
    entries = [f"127.0.0.1 {server_host}"]
    raw = HOSTS_PATH.read_bytes().decode("latin-1")
    lines = raw.splitlines(keepends=True)
    active = {_norm(l) for l in lines if not l.lstrip().startswith("#")}
    new_lines = list(lines)
    changed = []
    for entry in entries:
        if add:
            if entry in active:
                continue
            replaced = False
            for i, l in enumerate(new_lines):
                stripped = l.lstrip()
                if stripped.startswith("#") and _norm(stripped.lstrip("#")) == entry:
                    eol = "\r\n" if l.endswith("\r\n") else ("\n" if l.endswith("\n") else "")
                    new_lines[i] = entry + eol
                    changed.append(f"uncomment {entry}")
                    replaced = True
                    break
            if not replaced:
                if new_lines and not new_lines[-1].endswith(("\n", "\r\n")):
                    new_lines[-1] += "\n"
                new_lines.append(entry + "\n")
                changed.append(f"add {entry}")
        else:
            new_lines = [l for l in new_lines if _norm(l) != entry]
            changed.append(f"remove {entry}")
    if changed:
        _write_hosts_safely("".join(new_lines).encode("latin-1"))
    return changed


def resolve_real_ip(server_host, fallback_ips):
    try:
        ip = socket.gethostbyname(server_host)
    except OSError:
        ip = None
    if not ip or ip.startswith("127.") or ip == "0.0.0.0":
        fb = fallback_ips.get(server_host) or FALLBACK_IPS.get(server_host)
        if not fb:
            raise RuntimeError(f"no fallback IP for {server_host}")
        log(f"[*] DNS loopback -> fallback {fb}")
        return fb
    return ip


def ensure_cert(cert, key):
    if not Path(cert).is_file() or not Path(key).is_file():
        raise RuntimeError("cert/key not found — copy adultchess.crt/.key from "
                           "battle_cheat.exe_extracted next to el_proxy.py")


# ── Response mutation (mitm_addon logic) ─────────────────────────────────────

NESTED_RESOURCE_PATHS = [
    ["userData", "resources"],
    ["profile", "resources"],
    ["data", "resources"],
    ["data", "userData", "resources"],
    ["result", "resources"],
    ["result", "userData", "resources"],
    ["resources"],
]
ERROR_PATHS = [["error_code"], ["error", "code"], ["result", "error", "code"],
               ["result", "code"], ["data", "error", "code"], ["code"], ["err_code"]]
# Server sends PnkClient.Core.PnkRequestError: {code, message, title}.  Log the
# full debugging info for any blocked/seen error so the user can read what the
# server actually rejected instead of a rewritten success.
def _extract_error(data):
    """Return (code, message, title) from any known error layout or None."""
    if not isinstance(data, dict):
        return None
    for p in ERROR_PATHS:
        obj = data
        for key in p[:-1]:
            obj = obj.get(key) if isinstance(obj, dict) else None
            if obj is None:
                break
        if isinstance(obj, dict) and isinstance(obj.get(p[-1]), int):
            return (obj[p[-1]],
                    obj.get("message", ""),
                    obj.get("title", ""))
    return None


def mutate_response(body_bytes, cfg):
    """Return (new_bytes_or_None, changed_count). None means no change."""
    try:
        data = json.loads(body_bytes.decode("utf-8"))
    except (UnicodeDecodeError, json.JSONDecodeError):
        return None, 0
    if not isinstance(data, dict):
        return None, 0
    changed = 0

    err = _extract_error(data)
    if err:
        code, msg, title = err
        if code in cfg["block_error_codes"]:
            log(f"[ERR] BLOCKING server error code={code} title={title!r} "
                f"msg={msg!r} body={json.dumps(data, ensure_ascii=False)[:500]}")
        else:
            log(f"[ERR] server error code={code} title={title!r} msg={msg!r} "
                f"(not blocked)")

    for key, value in cfg["resource_overrides"].items():
        if key in data and isinstance(data[key], (int, float)):
            data[key] = value
            changed += 1

    for path in NESTED_RESOURCE_PATHS:
        obj = data
        for p in path[:-1]:
            obj = obj.get(p) if isinstance(obj, dict) else None
            if obj is None:
                break
        if isinstance(obj, dict) and isinstance(obj.get(path[-1]), dict):
            for key, value in cfg["resource_overrides"].items():
                if key in obj[path[-1]] and isinstance(obj[path[-1]][key], (int, float)):
                    obj[path[-1]][key] = value
                    changed += 1

    for path in ERROR_PATHS:
        obj = data
        for p in path[:-1]:
            obj = obj.get(p) if isinstance(obj, dict) else None
            if obj is None:
                break
        if isinstance(obj, dict) and obj.get(path[-1]) in cfg["block_error_codes"]:
            obj[path[-1]] = 0
            if isinstance(data, dict):
                data["success"] = True
                data["message"] = "OK"
            changed += 1

    if changed:
        return json.dumps(data, separators=(",", ":"), ensure_ascii=False).encode("utf-8"), changed
    return None, 0


# ── Request mutation (battle_cheat roulette) ─────────────────────────────────

_DECK_LOCK = threading.Lock()
_last_deck = []  # cards from the most recent cached_spin response


def _set_last_deck(rewards):
    global _last_deck
    with _DECK_LOCK:
        _last_deck = list(rewards)


def _find_pick_index(rt, rid):
    with _DECK_LOCK:
        deck = list(_last_deck)
    for i, c in enumerate(deck):
        if c.get("type") == rt and c.get("id") == rid:
            return i + 1, c
    return None, None


def mutate_request(body_bytes, headers, cfg):
    """Rewrite ApplyCardRouletteSpinRewards actions. Returns (new_body, headers, changed)."""
    roulette = cfg.get("roulette", {})
    if not roulette.get("enabled"):
        return body_bytes, headers, False
    action_name = roulette.get("action_name", "ApplyCardRouletteSpinRewards")
    rtype = roulette.get("reward_type", "monster")
    rid = roulette.get("reward_id", "")
    qty = int(roulette.get("reward_qty", 1))
    if not rid:
        return body_bytes, headers, False

    raw = body_bytes
    try:
        if headers.get("Content-Encoding", "").lower() == "gzip":
            raw = gzip.decompress(body_bytes)
        req = json.loads(raw.decode("utf-8"))
    except Exception:
        return body_bytes, headers, False

    actions = req.get("update", []) if isinstance(req, dict) else []
    modified = False
    seen = set()
    if actions:
        names = [a.get("action") for a in actions if isinstance(a, dict)]
        if action_name not in names:
            # Final-reward / quit actions carry the roulette state the server
            # validates; log their payload so a 700 after "claim" is readable.
            for a in actions:
                an = a.get("action") if isinstance(a, dict) else ""
                if an in ("TakeCardRouletteFinalReward", "TakeAndQuitCardRouletteRewards"):
                    log(f"[ROULETTE] {an} payload: "
                        f"{json.dumps(a.get('data', {}), ensure_ascii=False)[:400]}")
            log(f"[UPDATE] no roulette pick ({len(actions)} actions: {names[:6]})")
            return body_bytes, headers, False
    for action in actions:
        if not isinstance(action, dict) or action.get("action") != action_name:
            continue
        data = action.get("data", {})
        if not isinstance(data, dict):
            continue
        cur_type = data.get("reward_type")
        cur_id = data.get("reward_id")
        idx, _deck_card = _find_pick_index(cur_type, cur_id)
        pos = f" #[{idx}]" if idx is not None else ""
        if cur_type == "black_mark" or cur_id == "black_mark":
            log(f"[ROULETTE] BM pick{pos} - passing through untouched")
            continue
        step = data.get("step_id")
        if step is not None and step in seen:
            log(f"[ROULETTE] duplicate step_id {step} - skip")
            continue
        if step is not None:
            seen.add(step)
        old = f"{data.get('reward_type')}/{data.get('reward_id')} x{data.get('reward_qty', '?')}"
        log(f"[ROULETTE] step {step}: picked card{pos} ({old})")
        data["reward_type"] = rtype
        data["reward_id"] = rid
        data["reward_qty"] = qty
        modified = True
        log(f"[ROULETTE] step {step}: {rtype}/{rid} x{qty} (was {old})")

    if modified:
        new_body = json.dumps(req, separators=(",", ":")).encode("utf-8")
        headers = {k: v for k, v in headers.items() if k.lower() != "content-encoding"}
        return new_body, headers, True
    return body_bytes, headers, False


# ── Proxy handler (mirrors battle_cheat _ProxyHandler) ───────────────────────

class ThreadedHTTPS(ThreadingMixIn, HTTPServer):
    daemon_threads = True
    allow_reuse_address = True


class ProxyState:
    config = None
    real_ip = None
    real_host = None


def _log_roulette_response(raw, ce, path):
    """Diagnostics for card_roulette endpoints — battle_cheat parity (lines
    1308-1347: decode + log only, no reward fields changed)."""
    try:
        txt = gzip.decompress(raw).decode("utf-8", "replace") if ce == "gzip" \
            else raw.decode("utf-8", "replace")
    except OSError:
        txt = raw.decode("utf-8", "replace")
    ep = path.split("/")[-1]
    log(f"[ROULETTE] {ep} -> {len(txt)}b")
    try:
        data = json.loads(txt)
    except json.JSONDecodeError:
        return
    result = data.get("result", {}) if isinstance(data, dict) else {}
    if "cached_spin" in path:
        if isinstance(result, dict):
            log(f"[SPIN] result keys: {list(result.keys())}")
            for k in ("rewards", "generated_rewards", "picked", "picked_index",
                      "selected", "selected_index", "index", "is_black_mark"):
                if k in result:
                    log(f"[SPIN] {k}={json.dumps(result[k], ensure_ascii=False)[:400]}")
        rewards = result.get("rewards", result.get("generated_rewards", []))
        _set_last_deck(rewards if isinstance(rewards, list) else [])
        if rewards:
            log(f"[SPIN] {len(rewards)} cards dealt:")
            for i, c in enumerate(rewards):
                ctype = c.get("type", "?")
                cid = c.get("id", "?")
                qty = c.get("quantity", "?")
                if ctype == "black_mark" or cid == "black_mark":
                    log(f"  [{i + 1}] {ctype}/{cid} x{qty}  <<BM>>  <- DON'T PICK THIS")
                else:
                    log(f"  [{i + 1}] {ctype}/{cid} x{qty}")
        else:
            log(f"[SPIN] no rewards in response: {txt[:300]}")
    elif "get_progress" in path:
        step = result.get("step_id", "?")
        acc = result.get("accumulated_rewards", [])
        hbm = result.get("has_black_mark", False)
        log(f"[PROGRESS] step={step} acc={len(acc)} has_bm={hbm}")
        for r in acc:
            log(f'  + {r.get("type", "?")}/{r.get("id", "?")} x{r.get("quantity", "?")}')


class ProxyHandler(BaseHTTPRequestHandler):
    def log_message(self, fmt, *args):
        pass

    def _proxy(self):
        cfg = ProxyState.config
        path = self.path
        body = None
        cl = self.headers.get("Content-Length")
        if cl:
            body = self.rfile.read(int(cl))

        real_ip, real_host = ProxyState.real_ip, ProxyState.real_host
        if cfg.get("log_requests"):
            log(f"[REQ] {self.command} {path[:90]}")

        capture_dir = cfg.get("capture_dir", "")
        if capture_dir:
            try:
                Path(capture_dir).mkdir(parents=True, exist_ok=True)
                safe = re.sub(r"[^A-Za-z0-9_.-]", "_", path)[:80]
                tag = time.strftime("%Y%m%d_%H%M%S") + f"_{self.client_address[1]}"
                if body is not None:
                    (Path(capture_dir) / f"{tag}_{self.command}_{safe}.req").write_bytes(body)
            except OSError as e:
                log(f"[!] capture req: {e}")

        url = f"https://{real_ip}{path}"
        headers = {}
        for k in self.headers:
            if k.lower() in ("host", "content-length", "transfer-encoding"):
                continue
            headers[k] = self.headers[k]
        headers["Host"] = real_host

        if body is not None and "profile/update" in path:
            body, headers, changed = mutate_request(body, headers, cfg)
            if changed:
                log(f"[UPDATE] rewrote {cfg['roulette']['reward_type']}/"
                    f"{cfg['roulette']['reward_id']} x{cfg['roulette']['reward_qty']}")

        req = urlrequest.Request(url, data=body, headers=headers, method=self.command)
        ctx = ssl.create_default_context()
        ctx.check_hostname = False
        ctx.verify_mode = ssl.CERT_NONE

        status, raw, ct, ce = 502, b"", "text/plain", ""
        try:
            opener = urlrequest.build_opener(
                urlrequest.ProxyHandler({}),      # bypass system proxy
                urlrequest.HTTPSHandler(context=ctx))  # upstream cert is self-signed
            resp = opener.open(req, timeout=cfg.get("upstream_timeout", 30))
            raw = resp.read()
            ct = resp.headers.get("Content-Type", "")
            ce = resp.headers.get("Content-Encoding", "")
            status = resp.status
        except urlerror.HTTPError as e:
            raw = e.read()
            ct = e.headers.get("Content-Type", "") if e.headers else ""
            ce = e.headers.get("Content-Encoding", "") if e.headers else ""
            status = e.code
        except Exception as e:
            log(f"[!] upstream error: {e}")

        if capture_dir:
            try:
                safe = re.sub(r"[^A-Za-z0-9_.-]", "_", path)[:80]
                tag = time.strftime("%Y%m%d_%H%M%S") + f"_{self.client_address[1]}"
                (Path(capture_dir) / f"{tag}_{self.command}_{safe}.resp").write_bytes(raw)
            except OSError as e:
                log(f"[!] capture resp: {e}")

        # Response mutation (decompress-if-gzip, rewrite, re-gzip).
        if cfg.get("log_responses") and ("profile/update" in path or "ac_battle" in path):
            log(f"[RESP] {status} {path[:60]} ({len(raw)}b)")
        # Log server-side errors on roulette/profile paths regardless of HTTP
        # status.  Error responses with status != 200 bypass mutate_response,
        # so this is the only place the user can read what the server rejected.
        if "card_roulette" in path or "profile/update" in path:
            body_for_err = raw
            if ce.lower() == "gzip":
                try:
                    body_for_err = gzip.decompress(raw)
                except OSError:
                    pass
            try:
                err = _extract_error(json.loads(body_for_err.decode("utf-8", "replace")))
            except (UnicodeDecodeError, json.JSONDecodeError):
                err = None
            if err:
                code, msg, title = err
                log(f"[ERR] {status} {path[:60]} server error code={code} "
                    f"title={title!r} msg={msg!r}")
                # 409 validation failures on roulette claims are usually a
                # server-side whitelist rejection of the configured reward
                # (e.g. resource/hammer is not grantable via card roulette,
                # while gems/gold/relationship_gifts are).  Dump the full body
                # once so the exact rejected field is visible.
                if code == 409 and "valid" in msg.lower():
                    log(f"[ERR] 409 body={body_for_err.decode('utf-8', 'replace')[:800]}")
        if status == 200 and ("profile/update" in path or "ac_battle" in path):
            resp_body = raw
            re_gzip = False
            if ce.lower() == "gzip":
                try:
                    resp_body = gzip.decompress(raw)
                    re_gzip = True
                except OSError:
                    resp_body = raw
            new_body, changed = mutate_response(resp_body, cfg)
            if changed:
                log(f"[EL] RESP modified {changed} fields")
                if re_gzip:
                    raw = gzip.compress(new_body)
                    ce = "gzip"
                else:
                    raw = new_body

        if "card_roulette" in path:
            _log_roulette_response(raw, ce, path)

        self.send_response(status)
        self.send_header("Content-Type", ct)
        if ce:
            self.send_header("Content-Encoding", ce)
        self.send_header("Content-Length", str(len(raw)))
        self.end_headers()
        self.wfile.write(raw)

    do_GET = _proxy
    do_POST = _proxy
    do_PUT = _proxy


# ── Lifecycle ─────────────────────────────────────────────────────────────────

_server = None
PID_PATH = BASE_DIR / "el_proxy.pid"


def _prepare():
    cfg = load_config()
    warn_roulette_config(cfg)
    ensure_cert(cfg["cert"], cfg["key"])
    server_host = cfg["server_host"]
    real_ip = resolve_real_ip(server_host, cfg.get("fallback_ips", {}))
    log(f"[*] Server: {server_host} ({real_ip})")

    if sys.platform == "win32":
        r = subprocess.run(["certutil", "-verify", cfg["cert"]],
                           capture_output=True, text=True)
        needs = "CERT_TRUST_IS_UNTRUSTED_ROOT" in r.stdout or r.returncode != 0
        if needs:
            r2 = subprocess.run(["certutil", "-addstore", "Root", cfg["cert"]],
                                capture_output=True, text=True)
            if r2.returncode != 0:
                log("[!] Failed to install cert — run as administrator")
                return None
            log("[*] Installed proxy cert into trusted root store")
        else:
            log("[*] Proxy cert already trusted")

    try:
        changed = update_hosts(server_host, add=True)
        for c in changed:
            log(f"[*] Hosts {c}")
    except PermissionError:
        log("[!] Cannot write hosts file. Run as admin, or clear ReadOnly: "
            "attrib -r C:\\Windows\\System32\\drivers\\etc\\hosts")
        return None

    ProxyState.config = cfg
    ProxyState.real_ip = real_ip
    ProxyState.real_host = server_host
    return cfg


def _free_port(port):
    """Kill any stale process listening on `port` (stale children from a prior
    run survive --stop when the pid file was overwritten). Elevated token required."""
    if sys.platform != "win32":
        return
    out = subprocess.run(["netstat", "-ano"], capture_output=True,
                         text=True, check=False).stdout
    for line in out.splitlines():
        if f":{port} " not in line or "LISTENING" not in line:
            continue
        parts = line.split()
        pid = parts[-1] if parts else ""
        if pid.isdigit():
            try:
                subprocess.run(["taskkill", "/PID", pid, "/F"],
                               capture_output=True, check=False)
                log(f"[*] Killed stale listener pid {pid} on :{port}")
            except OSError:
                pass


def _serve(cfg):
    global _server, _thread
    _free_port(int(cfg["port"]))
    try:
        _server = ThreadedHTTPS(("0.0.0.0", int(cfg["port"])), ProxyHandler)
    except OSError as e:
        log(f"[!] Cannot bind :{cfg['port']} — {e} (admin required)")
        return 1
    context = ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER)
    context.minimum_version = ssl.TLSVersion.TLSv1_2
    context.load_cert_chain(cfg["cert"], cfg["key"])
    _server.socket = context.wrap_socket(_server.socket, server_side=True)
    _thread = threading.Thread(target=_server.serve_forever, daemon=True)
    _thread.start()
    log(f"[*] Proxy running on :{cfg['port']} for {cfg['server_host']} — "
        f"{'roulette injection ON' if cfg.get('roulette', {}).get('enabled') else 'passthrough'}")
    return 0


def start():
    """Foreground: prepare + serve + block. Keeps the proxy alive in this terminal."""
    cfg = _prepare()
    if cfg is None:
        return 1
    rc = _serve(cfg)
    if rc:
        return rc
    try:
        while True:
            time.sleep(3600)
    except KeyboardInterrupt:
        pass
    return 0


def start_bg():
    """Background: spawn detached child (--serve), survives terminal close."""
    cfg = _prepare()
    if cfg is None:
        return 1
    if PID_PATH.exists():
        old = PID_PATH.read_text().strip()
        if old.isdigit() and _pid_alive(int(old)):
            log(f"[*] Proxy already running (pid {old})")
            return 0
        PID_PATH.unlink(missing_ok=True)
    flags = subprocess.DETACHED_PROCESS | subprocess.CREATE_NEW_PROCESS_GROUP
    proc = subprocess.Popen(
        [sys.executable, str(Path(__file__).resolve()), "--serve"],
        creationflags=flags, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL,
        close_fds=True)
    PID_PATH.write_text(str(proc.pid))
    time.sleep(2)
    if not _pid_alive(proc.pid):
        log("[!] Background proxy died on startup — see el_proxy.log")
        return 1
    log(f"[*] Proxy running in background (pid {proc.pid})")
    return 0


def _pid_alive(pid):
    if sys.platform == "win32":
        out = subprocess.run(["tasklist", "/FI", f"PID eq {pid}"],
                             capture_output=True, text=True, check=False).stdout
        return re.search(rf"\b{pid}\b", out) is not None
    try:
        os.kill(pid, 0)
        return True
    except OSError:
        return False


def stop():
    global _server, _thread
    if _server:
        _server.shutdown()
        _server = None
    if PID_PATH.exists():
        pid = PID_PATH.read_text().strip()
        if pid.isdigit():
            if sys.platform == "win32":
                subprocess.run(["taskkill", "/PID", pid, "/F"],
                               capture_output=True, check=False)
            else:
                try:
                    os.kill(int(pid), 9)
                except OSError:
                    pass
        PID_PATH.unlink(missing_ok=True)
        log(f"[*] Killed background proxy pid {pid}")
    cfg = load_config()
    try:
        changed = update_hosts(cfg["server_host"], add=False)
        for c in changed:
            log(f"[*] Hosts {c}")
    except Exception as e:
        log(f"[!] Hosts cleanup: {e}")
    log("[*] Proxy stopped")
    return 0


def status():
    cfg = load_config()
    if _server:
        print(f"running (foreground) :{cfg['port']} -> {cfg['server_host']}")
        return
    if PID_PATH.exists():
        pid = PID_PATH.read_text().strip()
        if pid.isdigit() and _pid_alive(int(pid)):
            print(f"running (background pid {pid}) :{cfg['port']} -> {cfg['server_host']}")
            return
    print("stopped")


# ── Self-check (no admin, no hosts, no port bind) ────────────────────────────

def _test():
    cfg = load_config()
    assert Path(cfg["cert"]).is_file(), "cert missing"
    assert Path(cfg["key"]).is_file(), "key missing"

    body = json.dumps({"update": [{
        "action": cfg["roulette"]["action_name"],
        "data": {"reward_type": "black_mark", "reward_id": "black_mark", "reward_qty": 1},
    }]}).encode()
    out, _, changed = mutate_request(body, {}, cfg)
    assert not changed, "black-mark must pass through"

    body = json.dumps({"update": [{
        "action": cfg["roulette"]["action_name"],
        "data": {"reward_type": "gold", "reward_id": "1", "reward_qty": 5, "step_id": 1},
    }]}).encode()
    out, _, changed = mutate_request(body, {}, cfg)
    if cfg["roulette"]["enabled"] and cfg["roulette"]["reward_id"]:
        assert changed and b'"reward_id"' in out, "reward rewrite failed"

    resp = json.dumps({"gold": 100, "error_code": 700}).encode()
    out, n = mutate_response(resp, cfg)
    assert n >= 1 and json.loads(out)["gold"] == 999999
    assert json.loads(out)["error_code"] == 0

    ip = resolve_real_ip(cfg["server_host"], cfg.get("fallback_ips", {}))
    assert ip and not ip.startswith("127.")
    print("el_proxy self-check: PASS")
    return 0


def main(argv=None):
    argv = sys.argv[1:] if argv is None else argv
    ap = argparse.ArgumentParser(description="EL standalone HTTPS proxy")
    ap.add_argument("--start", action="store_true",
                    help="foreground: blocks this terminal")
    ap.add_argument("--start-bg", action="store_true",
                    help="background: survives terminal close")
    ap.add_argument("--serve", action="store_true",
                    help="internal: run server (used by --start-bg child)")
    ap.add_argument("--stop", action="store_true")
    ap.add_argument("--status", action="store_true")
    ap.add_argument("--test", action="store_true")
    args = ap.parse_args(argv)
    if args.test:
        return _test()
    if args.serve:
        try:
            return start()
        except Exception:
            log("[!] proxy child crashed:\n" + traceback.format_exc())
            return 1
    if args.start:
        return start()
    if args.start_bg:
        return start_bg()
    if args.stop:
        return stop()
    if args.status:
        return status()
    ap.print_help()
    return 0


if __name__ == "__main__":
    sys.exit(main())
