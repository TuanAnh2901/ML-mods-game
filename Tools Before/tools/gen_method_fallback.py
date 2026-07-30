#!/usr/bin/env py -3.13
"""Compact RVA fallback table generator.

Reads method-pointer-map.json, outputs el_native/method_fallback.inc
with critical methods only. No embedding 200k-entry map in the DLL.

Usage:
    py -3.13 tools/gen_method_fallback.py
    py -3.13 tools/gen_method_fallback.py --json path/to/method-pointer-map.json
"""

import json
import re
import sys
from pathlib import Path
from typing import Any

# ── paths ──────────────────────────────────────────────────────────────
ROOT = Path(__file__).resolve().parent.parent
DEFAULT_JSON = Path(
    r"D:\SteamLibrary\steamapps\common\Everlusting Life"
    r"\Analysis\Cpp2IL-method-map\method-pointer-map.json"
)
OUTPUT = ROOT / "el_native" / "method_fallback.inc"

# ── method descriptors ─────────────────────────────────────────────────
# Each entry: (type_pattern, method_name, assembly, ns_override, argc)
# Optional 6th element: a substring that must appear in the JSON "signature"
# field — needed to pick between overloads with identical parameter counts.
# type_pattern is searched in the JSON "type" field (case-insensitive substring)
# If ns_override is None, it is derived from the type name.
# method_name must match exactly (as stored in JSON "method" field).
DESIRED: list[tuple[Any, ...]] = [
    # SM_destroyThisTimed
    ("SM_destroyThisTimed",     "Update",             "Assembly-CSharp",      None,       0),
    # ItemModule
    ("AutoChess.DataClasses.UserData.ItemModule",
                                "get_Instance",       "Assembly-CSharp",      None,       0),
    ("AutoChess.DataClasses.UserData.ItemModule",
                                "ChangeResource",     "Assembly-CSharp",      None,       3),
    ("AutoChess.DataClasses.UserData.ItemModule",
                                "GetResourceCount",   "Assembly-CSharp",      None,       1),
    # ResourceBit
    ("AutoChess.DataClasses.UserData.UserDataBits.ResourceBit",
                                "ChangeResource",     "Assembly-CSharp",      None,       2),
    ("AutoChess.DataClasses.UserData.UserDataBits.ResourceBit",
                                "SetResource",        "Assembly-CSharp",      None,       2),
    ("AutoChess.DataClasses.UserData.UserDataBits.ResourceBit",
                                "GetResource",        "Assembly-CSharp",      None,       1),
    # UserResourcesUtil
    ("UserResourcesUtil",       "GetStringByResourceType",
                                                      "Assembly-CSharp",      None,       1),
    ("UserResourcesUtil",       "GetResourceTypeByString",
                                                      "Assembly-CSharp",      None,       1),
    ("UserResourcesUtil",       "GetAllResourceTypes",
                                                      "Assembly-CSharp",      None,       0),
    ("UserResourcesUtil",       "GetResourceName",    "Assembly-CSharp",      None,       1),
    # UnityEngine.Time
    ("UnityEngine.Time",        "set_timeScale",      "UnityEngine.CoreModule",
                                                                             None,       1),
    ("UnityEngine.Time",        "get_timeScale",      "UnityEngine.CoreModule",
                                                                             None,       0),
    # Constants
    ("NewAssets.Scripts.DataClasses.UserData.Constants",
                                "get_fightCustomTimescale",
                                                      "Assembly-CSharp",      None,       0),
    # ScalableTimescaleProvider
    ("AutoChess.CoreGameplay.Fight.CustomTimescale.ScalableTimescaleProvider",
                                "get_CustomTimescale",
                                                      "Assembly-CSharp",      None,       0),
    ("AutoChess.CoreGameplay.Fight.CustomTimescale.ScalableTimescaleProvider",
                                ".ctor",              "Assembly-CSharp",      None,       1),
    # IBattleTimescaleProvider (interface, RVA = 0)
    ("AutoChess.CoreGameplay.Fight.CustomTimescale.IBattleTimescaleProvider",
                                "get_CustomTimescale",
                                                      "Assembly-CSharp",      None,       0),
    # LocalMatchTimer
    ("AutoChess.LocalServer.LocalMatchTimer",
                                "SetTimescaleProvider",
                                                      "Assembly-CSharp",      None,       1),
    ("AutoChess.LocalServer.LocalMatchTimer",
                                "CreateNew",          "Assembly-CSharp",      None,       1),
    ("AutoChess.LocalServer.LocalMatchTimer",
                                "FixedUpdate",        "Assembly-CSharp",      None,       0),
    ("AutoChess.LocalServer.LocalMatchTimer",
                                "get_IsPaused",       "Assembly-CSharp",      None,       0),
    ("AutoChess.LocalServer.LocalMatchTimer",
                                "PauseTime",          "Assembly-CSharp",      None,       0),
    ("AutoChess.LocalServer.LocalMatchTimer",
                                "UnpauseTime",        "Assembly-CSharp",      None,       0),
    ("AutoChess.LocalServer.LocalMatchTimer",
                                "Dispose",            "Assembly-CSharp",      None,       0),
    ("AutoChess.LocalServer.LocalMatchTimer",
                                "InvokeAfterTime",    "Assembly-CSharp",      None,       2),
    # Phase gates so speed hacks only apply during the fight, never the shop
    ("AutoChess.LocalServer.LocalPhaseController",
                                "StartBattle",        "Assembly-CSharp",      None,       0),
    ("AutoChess.LocalServer.LocalPhaseController",
                                "FinishBattle",       "Assembly-CSharp",      None,       0),
    ("AutoChess.LocalServer.LocalPhaseController",
                                "FinishBattleEarlier",
                                                      "Assembly-CSharp",      None,       0),
    # Damage / GodMode
    ("AutoChess.CoreGameplay.Fight.Units.BattleUnit",
                                "ApplyDamage",        "Assembly-CSharp",      None,       5),
    ("AutoChess.CoreGameplay.Fight.Units.UnitCore",
                                "get_ArmySide",       "Assembly-CSharp",      None,       0),
    # Currency - the game's "apply locally, don't sync" sentinel
    ("PnkClient.UpdateData",      "get_DontSendMe",     "Assembly-CSharp",      None,       0),
    # Netlog
    ("PnkClient.Core.PnkHandler",
                                "SendRequest",        "Assembly-CSharp",      None,       6,
                                "TRequestType sendElem"),
    ("PnkClient.Core.PnkHandler",
                                "SendRequest",        "Assembly-CSharp",      None,       4),
    ("MessagePack.MessagePackSerializer",
                                "SerializeToJson",    "MessagePack",          None,       3,
                                "System.String MessagePack"),
    # Gacha rates — in-battle shop pool draw (BankSummonLot is the out-of-battle
    # summon banner and has no effect on the in-match unit shop).
    ("AutoChess.CoreGameplay.BattleShop.LocalMonsterPool",
                                "DrawMonstersFromPool",
                                                      "Assembly-CSharp",      None,       2),
    ("AutoChess.CoreGameplay.Pve.LocalServer.PveMonsterPool",
                                "DrawMonstersFromPool",
                                                      "Assembly-CSharp",      None,       2),
    ("AutoChess.CoreGameplay.BattleShop.GeorgianLocalMonsterPool",
                                "DrawMonstersFromPool",
                                                      "Assembly-CSharp",      None,       2),
    ("AutoChess.CoreGameplay.Pve.LocalServer.LabyrinthMonsterPool",
                                "DrawMonstersFromPool",
                                                       "Assembly-CSharp",      None,       2),
    # Shop controller — needed by Gacha feature for in-match shop state
    ("AutoChess.LocalServer.LocalShopController",
                                "RefreshShop",        "Assembly-CSharp",      None,       3),
    ("AutoChess.LocalServer.LocalShopController",
                                "AddMonstersToQueue", "Assembly-CSharp",      None,       2),
    # Cheat Tester — shipped dev cheat UI methods
    ("AutoChess.Prefabs.CheatsWindow.Scripts.Core.Modules.CheatModuleOther",
                                "ShowHideConsole",    "Assembly-CSharp",      None,       0),
    ("AutoChess.CheatsWindow.Core.Modules.CheatModuleGoTo",
                                "FiendGetResource",   "Assembly-CSharp",      None,       2),
    # Monster Dump — read all monster static data (ID + name)
    ("MonsterDataUtils",          "get_MonsterDataHelper",
                                                       "Assembly-CSharp",      None,       0),
    ("NewAssets.Scripts.Data_Helpers.MonsterDataHelper",
                                "get_MonstersList",   "Assembly-CSharp",      None,       0),
    ("NewAssets.Scripts.DataClasses.MonsterStaticData",
                                "GetOwnName",         "Assembly-CSharp",      None,       0),
    ("NewAssets.Scripts.DataClasses.MonsterStaticData",
                                "get_monsterId",      "Assembly-CSharp",      None,       0),
    # BattleShop — shop economy manipulation
    ("AutoChess.LocalServer.LocalShopController",
                                "SetRefreshPrice",    "Assembly-CSharp",      None,       2),
    ("AutoChess.LocalServer.LocalShopController",
                                "UpdateSlotPrice",    "Assembly-CSharp",      None,       2),
    ("AutoChess.LocalServer.LocalShopController",
                                "SellUnit",           "Assembly-CSharp",      None,       3),
    # Cheat Window — hook to enable the hidden dev UI
    ("Assets.Scripts.UI_Scripts.UIElements.TapListener",
                                "CheckShowCheatsWindow",
                                                       "Assembly-CSharp",      None,       0),
    # Sell price multiplier
    ("MonsterDataUtils",          "GetSellingPrice",    "Assembly-CSharp",      None,       1),
    # Slot price — hook to always return 0
    ("AutoChess.LocalServer.LocalShopController",
                                "GetSlotPrice",       "Assembly-CSharp",      None,       1),
    # Debug.Log capture for cheat console output
    ("UnityEngine.Debug",         "Log",                "UnityEngine.CoreModule",None,     1),
    # Energy/Attack Speed — multiply mana gain and reduce turn interval
    ("AutoChess.CoreGameplay.Fight.Units.BattleUnit",
                                "ChangeMana",         "Assembly-CSharp",      None,       1),
    ("AutoChess.CoreGameplay.Fight.Units.BattleUnit",
                                "GetTurnInterval",    "Assembly-CSharp",      None,       0),
]


def split_type_name(full_type: str) -> tuple[str, str]:
    """Return (namespace, class_name) from a (possibly dotted) type name."""
    # Handle nested types like "Outer+Inner" — keep Outer as namespace prefix
    # For simplicity, just use everything before last dot as ns
    if "." in full_type:
        *parts, cls = full_type.split(".")
        ns = ".".join(parts)
        return ns, cls
    return "", full_type


def count_params(signature: str) -> int:
    inner = signature[signature.index("(") + 1 : signature.rindex(")")].strip()
    if not inner:
        return 0
    depth = 0
    count = 1
    for ch in inner:
        if ch in "<[(":
            depth += 1
        elif ch in ">])":
            depth -= 1
        elif ch == "," and depth == 0:
            count += 1
    return count


def main() -> None:
    import argparse

    parser = argparse.ArgumentParser(
        description="Generate method fallback C header from method-pointer-map.json"
    )
    parser.add_argument(
        "--json",
        default=str(DEFAULT_JSON),
        help=f"Path to method-pointer-map.json (default: {DEFAULT_JSON})",
    )
    parser.add_argument(
        "--output",
        default=str(OUTPUT),
        help=f"Output C header path (default: {OUTPUT})",
    )
    args = parser.parse_args()

    json_path = Path(args.json)
    if not json_path.is_file():
        print(f"Error: {json_path} not found", file=sys.stderr)
        sys.exit(1)

    print(f"Reading {json_path} ...")
    with open(json_path, "r", encoding="utf-8") as f:
        entries: list[dict[str, Any]] = json.load(f)

    print(f"Total entries in map: {len(entries)}")

    # Build index: type_method -> list of entry dicts
    index: dict[str, list[dict[str, Any]]] = {}
    for e in entries:
        key = f"{e['type']}::{e['method']}"
        index.setdefault(key, []).append(e)

    results: list[dict[str, Any]] = []
    not_found: list[tuple[str, str]] = []

    for desired in DESIRED:
        type_pat, method_name, assembly, ns_override, argc = desired[:5]
        sig_hint = desired[5] if len(desired) > 5 else None

        candidates: list[dict[str, Any]] = []
        for key, entry_list in index.items():
            if "::" not in key:
                continue
            t, m = key.split("::", 1)
            if m == method_name and type_pat.lower() in t.lower():
                candidates.extend(entry_list)

        exact = [e for e in candidates if e["type"].lower() == type_pat.lower()]
        if exact:
            candidates = exact
        if sig_hint:
            candidates = [e for e in candidates if sig_hint in e["signature"]] or candidates
        by_arity = [e for e in candidates if count_params(e["signature"]) == argc]
        if by_arity:
            candidates = by_arity

        if not candidates:
            available = sorted(
                k for k in index if type_pat.lower() in k.split("::", 1)[0].lower()
            )
            hints = ", ".join(k.split("::", 1)[1] for k in available[:10])
            not_found.append((type_pat, method_name))
            print(
                f"  NOT FOUND: {type_pat}::{method_name}  "
                f"(available: {hints or 'none'})"
            )
            continue

        entry = candidates[0]
        type_name = entry["type"]
        ns, cls = split_type_name(type_name)
        if ns_override is not None:
            ns = ns_override
        rva_str = entry.get("rva", "0x0")
        rva = int(rva_str, 16) if isinstance(rva_str, str) else rva_str

        results.append({
            "assembly": assembly,
            "ns": ns,
            "cls": cls,
            "method": method_name,
            "argc": argc,
            "rva": rva,
        })

    # Sort by RVA for readability
    results.sort(key=lambda r: r["rva"])

    # Generate C header
    lines: list[str] = []
    lines.append("// Auto-generated by tools/gen_method_fallback.py")
    lines.append("// DO NOT EDIT — regenerate when method-pointer-map.json changes.")
    lines.append("#pragma once")
    lines.append("#include <stdint.h>")
    lines.append("#include <stddef.h>")
    lines.append("")
    lines.append("typedef struct {")
    lines.append("    const char* assembly;")
    lines.append("    const char* ns;")
    lines.append("    const char* klass;")
    lines.append("    const char* method;")
    lines.append("    int argc;")
    lines.append("    uintptr_t rva;")
    lines.append("} MethodFallbackEntry;")
    lines.append("")
    lines.append(
        f"static MethodFallbackEntry g_methodFallbackTable[{len(results)}] = {{"
    )

    for r in results:
        rva_hex = f"0x{r['rva']:X}" if r["rva"] != 0 else "0x0"
        lines.append(
            f'    {{"{r["assembly"]}", "{r["ns"]}", "{r["cls"]}", '
            f'"{r["method"]}", {r["argc"]}, {rva_hex}}},'
        )

    lines.append("};")
    lines.append("")
    lines.append(
        "static const size_t g_methodFallbackCount = "
        f"sizeof(g_methodFallbackTable) / sizeof(g_methodFallbackTable[0]);"
    )
    lines.append("")

    content = "\n".join(lines) + "\n"

    out_path = Path(args.output)
    out_path.parent.mkdir(parents=True, exist_ok=True)
    with open(out_path, "w", encoding="utf-8") as f:
        f.write(content)

    print(f"Wrote {out_path}  ({len(results)} entries)")
    if not_found:
        print(f"WARNING: {len(not_found)} method(s) not found in map:")
        for t, m in not_found:
            print(f"  {t}::{m}")
    else:
        print("All methods found.")
    print(f"Entries found: {len(results)}")


if __name__ == "__main__":
    main()
