# mitmproxy script for Everlusting Life currency manipulation
# Usage: mitmproxy -s mitm_addon.py --set block_global=false
# Config: mitm_config.json (auto-created on first run)

import json
import os
from pathlib import Path
from mitmproxy import http, ctx

CONFIG_PATH = Path(__file__).parent / "mitm_config.json"

DEFAULT_CONFIG = {
    "enabled": True,
    "target_hosts": [
        "ga.adult-chess.com",
        "inquiring-client.adult-chess.com"
    ],
    "endpoints": {
        "/gs_api/profile/update": {
            "method": "POST",
            "modify_response": True
        },
        "/v2/battles/ac/ac_battle": {
            "method": "POST",
            "modify_response": True
        }
    },
    "resource_overrides": {
        "gold": 999999,
        "gems": 999999,
        "energy": 999,
        "contribution": 999999,
        "elixir": 999999,
        "spin": 999
    },
    "block_error_codes": [700, 701, 702, 703],
    "log_requests": True,
    "log_responses": False
}

def load_config():
    if CONFIG_PATH.exists():
        try:
            with open(CONFIG_PATH, "r", encoding="utf-8") as f:
                user_config = json.load(f)
            # Merge with defaults
            config = DEFAULT_CONFIG.copy()
            config.update(user_config)
            if "resource_overrides" in user_config:
                config["resource_overrides"] = {**DEFAULT_CONFIG["resource_overrides"], **user_config["resource_overrides"]}
            return config
        except Exception as e:
            ctx.log.error(f"[EL] Config load error: {e}, using defaults")
    else:
        save_config(DEFAULT_CONFIG)
    return DEFAULT_CONFIG

def save_config(config):
    try:
        with open(CONFIG_PATH, "w", encoding="utf-8") as f:
            json.dump(config, f, indent=2, ensure_ascii=False)
        ctx.log.info(f"[EL] Config saved to {CONFIG_PATH}")
    except Exception as e:
        ctx.log.error(f"[EL] Config save error: {e}")

class EL_Currency_Mod:
    def __init__(self):
        self.config = load_config()
        self.request_count = 0
        self.modified_count = 0

    def is_target_host(self, host: str) -> bool:
        return any(h in host for h in self.config["target_hosts"])

    def is_target_endpoint(self, path: str) -> bool:
        for ep, cfg in self.config["endpoints"].items():
            if ep in path:
                return True
        return False

    def modify_response_body(self, body: str) -> str:
        try:
            data = json.loads(body)
        except json.JSONDecodeError:
            return body

        modified = False

        # Direct resource fields
        for key, value in self.config["resource_overrides"].items():
            if key in data and isinstance(data[key], (int, float)):
                if self.config["log_responses"]:
                    ctx.log.info(f"[EL] {key}: {data[key]} -> {value}")
                data[key] = value
                modified = True

        # Nested structures: userData, profile, resources, etc.
        nested_paths = [
            ["userData", "resources"],
            ["profile", "resources"],
            ["data", "resources"],
            ["data", "userData", "resources"],
            ["result", "resources"],
            ["result", "userData", "resources"],
            ["resources"]
        ]

        for path in nested_paths:
            obj = data
            for p in path[:-1]:
                obj = obj.get(p)
                if obj is None:
                    break
            if obj and isinstance(obj, dict):
                last = path[-1]
                if last in obj and isinstance(obj[last], dict):
                    for key, value in self.config["resource_overrides"].items():
                        if key in obj[last]:
                            obj[last][key] = value
                            modified = True

        # Block error codes
        for err_path in [["error_code"], ["error", "code"], ["code"], ["err_code"]]:
            obj = data
            for p in err_path[:-1]:
                obj = obj.get(p)
                if obj is None:
                    break
            if obj and err_path[-1] in obj:
                code = obj[err_path[-1]]
                if code in self.config["block_error_codes"]:
                    ctx.log.warn(f"[EL] Blocking error code {code}, replacing with success")
                    obj[err_path[-1]] = 0
                    # Add success fields
                    obj["success"] = True
                    obj["message"] = "OK"
                    modified = True

        if modified:
            self.modified_count += 1
            return json.dumps(data, separators=(",", ":"), ensure_ascii=False)
        return body

    def request(self, flow: http.HTTPFlow):
        if not self.config["enabled"]:
            return
        if not self.is_target_host(flow.request.pretty_host):
            return
        if not self.is_target_endpoint(flow.request.path):
            return

        self.request_count += 1
        if self.config["log_requests"]:
            ctx.log.info(f"[EL] REQ #{self.request_count} {flow.request.method} {flow.request.path}")

    def response(self, flow: http.HTTPFlow):
        if not self.config["enabled"]:
            return
        if not self.is_target_host(flow.request.pretty_host):
            return
        if not self.is_target_endpoint(flow.request.path):
            return

        if flow.response and flow.response.content:
            new_body = self.modify_response_body(flow.response.get_text())
            if new_body != flow.response.get_text():
                flow.response.set_text(new_body)
                ctx.log.info(f"[EL] RESP modified (total: {self.modified_count})")

    def done(self):
        ctx.log.info(f"[EL] Session done: {self.request_count} requests, {self.modified_count} modified")

addons = [EL_Currency_Mod()]