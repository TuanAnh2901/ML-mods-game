#!/usr/bin/env py -3.13
"""Compact RVA fallback table generator.

Reads method-pointer-map.json, outputs el_native/method_fallback.inc
with critical methods only. No embedding 200k-entry map in the DLL.

Usage:
    py -3.13 tools/gen_method_fallback.py
    py -3.13 tools/gen_method_fallback.py --json path/to/method-pointer-map.json
"""

import argparse
import sys
from pathlib import Path
from typing import Any, Sequence

from query_methods import (
    GhidraSettings,
    MethodTarget,
    atomic_write_text,
    extract_targeted,
    game_fingerprint,
    header_output_path,
    load_method_entries,
    load_targets,
    parse_target,
    read_job,
    resolve_target,
)

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
    # Combat runtime lifecycle (RVA is supplied by query_methods when present)
    ("AutoChess.CoreGameplay.Fight.FieldController", "StartBattle", "Assembly-CSharp", None, 2),
    ("AutoChess.CoreGameplay.Fight.FieldController", "BattleEnded", "Assembly-CSharp", None, 0),
    ("AutoChess.CoreGameplay.Fight.FieldController", "GetAllUnits", "Assembly-CSharp", None, 0),
    ("AutoChess.CoreGameplay.Fight.Units.BattleUnit", "Dispose", "Assembly-CSharp", None, 0),
    ("AutoChess.CoreGameplay.Fight.Units.BattleUnit", "IsNotTimeFrozen", "Assembly-CSharp", None, 0),
    ("AutoChess.CoreGameplay.Fight.Units.BattleUnit", "AttackActivation", "Assembly-CSharp", None, 1),
    ("AutoChess.CoreGameplay.Fight.Units.BattleUnit", "HandleAttackInternal", "Assembly-CSharp", None, 2),
    ("AutoChess.CoreGameplay.Fight.Units.BattleUnit", "get_Data", "Assembly-CSharp", None, 0),
    ("AutoChess.CoreGameplay.Participant.HandMonster", "get_monsterId", "Assembly-CSharp", None, 0),
    ("AutoChess.CoreGameplay.Participant.HandMonster", "GetName", "Assembly-CSharp", None, 0),
    ("AutoChess.CoreGameplay.Fight.Units.BattleUnit", "GetTargetUnit", "Assembly-CSharp", None, 0),
    ("AutoChess.CoreGameplay.Fight.Units.BattleUnit", "ToString", "Assembly-CSharp", None, 0),
    ("AutoChess.CoreGameplay.Fight.Units.UnitCore", "get_grade", "Assembly-CSharp", None, 0),
    ("AutoChess.MonsterInfoNew.MonsterInfoTabs.Relationships.RelationshipsModel", "SendPoints", "Assembly-CSharp", None, 2),
    ("AutoChess.MonsterInfoNew.MonsterInfoTabs.Relationships.RelationshipsModel", "IsNotEnoughGifts", "Assembly-CSharp", None, 1),
    ("AutoChess.MonsterInfoNew.MonsterInfoTabs.Relationships.RelationshipsModel", "GetCostResources", "Assembly-CSharp", None, 0),
    # Bunny/Rabbit roulette observers (all hooks preserve original behavior)
    ("CardRoulette.CardRouletteModel", "ParseRewards", "Assembly-CSharp", None, 1),
    ("CardRoulette.CardRouletteModel", "OnSpinResponseReceived", "Assembly-CSharp", None, 2),
    ("CardRoulette.CardRouletteModel", "OnProgressResponseReceived", "Assembly-CSharp", None, 2),
    ("AutoChess.MiniEvents.MiniEventModules.ChooseRewardEvent.ChooseRewardRouletteEventModule", "GetRelevantRouletteLotDataForCategory", "Assembly-CSharp", None, 1),
    ("AutoChess.MiniEvents.MiniEventModules.ChooseRewardEvent.ChooseRewardRouletteEventModule", "ChooseReward", "Assembly-CSharp", None, 2),
    ("AutoChess.MiniEvents.MiniEventModules.ChooseRewardEvent.ChooseRewardRouletteEventModule", "CreateDeliveryDataAndSave", "Assembly-CSharp", None, 0),
    ("AutoChess.CoreGameplay.Participant.ServerParticipant", "get_Coins", "Assembly-CSharp", None, 0),
    # ItemModule
    ("AutoChess.DataClasses.UserData.ItemModule",
                                "get_Instance",       "Assembly-CSharp",      None,       0),
    # Mascot collection observer.  This is a read-only getter used by the
    # Mascot session-unlock panel and collection restore hooks.
    ("AutoChess.DataClasses.UserData.ItemModule",
                                "get_MascotCollection", "Assembly-CSharp",      None,       0),
    ("AutoChess.MascotModule.MascotWindowPresenter",
                                "InitMascotButtons", "Assembly-CSharp", None, 0),
    ("AutoChess.MascotModule.MascotWindowPresenter",
                                "OnMascotButtonClicked", "Assembly-CSharp", None, 1),
    ("AutoChess.MascotModule.MascotWindow",
                                "OnHidden", "Assembly-CSharp", None, 0),
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
    ("MonsterDataUtils",          "GetSellingPrice",    "Assembly-CSharp",      None,       2,
                                 "(System.Int32 monsterId"),
    ("MonsterDataUtils",          "GetSellingPrice",    "Assembly-CSharp",      None,       5,
                                 "(System.Int32 grade, NewAssets.Scripts.DataClasses"),
    # Slot price — hook to always return 0
    ("AutoChess.LocalServer.LocalShopController",
                                "GetSlotPrice",       "Assembly-CSharp",      None,       1),
    # Debug.Log capture for cheat console output
    ("UnityEngine.Debug",         "Log",                "UnityEngine.CoreModule",None,     1),
    # Preserve the existing runtime fallback coverage when regenerating.
    ("AutoChess.CoreGameplay.Fight.CustomTimescale.IBattleTimescaleProvider",
                                "get_CustomTimescale", "Assembly-CSharp",    None,       0),
    ("NewAssets.Scripts.UtilScripts.MyUtil", "ConvertWinningSideToBattleResult",
                                                     "Assembly-CSharp",      None,       2),
    ("CodeStage.AntiCheat.Detectors.DetectorListenerWithNetworkWindow",
                                "OnCheatingDetected", "Assembly-CSharp",    None,       0),
    ("CodeStage.AntiCheat.Detectors.ObscuredCheatingDetectorListener",
                                "TryShowError",       "Assembly-CSharp",    None,       0),
    ("AutoChess.LocalServer.LocalMatchData", "ApplyBattleResult",
                                                     "Assembly-CSharp",      None,       3),
    ("AutoChess.LocalServer.LocalServerEmulator", "BattleResult",
                                                     "Assembly-CSharp",      None,       6),
    ("AutoChess.ChessSockets.ChessSocketsController",
        "ChessSockets.IChessSocketsController.BattleResult",
                                                     "Assembly-CSharp",      None,       6),
    ("AutoChess.ChessSockets.ChessSocketsController",
        "ChessSockets.IChessSocketsController.BattleCalculateResult",
                                                     "Assembly-CSharp",      None,       4),
    ("AutoChess.CoreGameplay.Participant.ServerParticipant", "UpdateStreaks",
                                                     "Assembly-CSharp",      None,       1),
    ("AutoChess.CoreGameplay.Participant.ServerParticipant", "get_IsLocal",
                                                     "Assembly-CSharp",      None,       0),
    ("AutoChess.CoreGameplay.Participant.ServerParticipant", "get_Uid",
                                                     "Assembly-CSharp",      None,       0),
    ("UserData.NameModule", "get_instance",          "Assembly-CSharp",      None,       0),
    ("UserData.NameModule", "get_MyProfileId",       "Assembly-CSharp",      None,       0),
    # Automation result-window observers.  These methods are only used to
    # advance the coordinator after a real result window is shown.
    ("AutoChess.UIScripts.WindowScripts.MatchCompleted.MatchCompletedWindow",
                                "ShowWindow", "Assembly-CSharp", None, 4),
    ("AutoChess.UIScripts.WindowScripts.MatchCompleted.MatchCompletedWindow",
                                "Update", "Assembly-CSharp", None, 0),
    ("AutoChess.UIScripts.WindowScripts.MatchCompleted.MatchCompletedWindow",
                                "HandlePlayNextButton", "Assembly-CSharp", None, 0),
    ("AutoChess.UIScripts.WindowScripts.MatchCompleted.MatchCompletedWindow",
                                "HideWindow", "Assembly-CSharp", None, 0),
    ("AutoChess.UIScripts.WindowScripts.MatchCompleted.MatchCompletedWindow",
                                "ContinueShow", "Assembly-CSharp", None, 0),
    ("AutoChess.UIScripts.WindowScripts.MainWindowPlayButton",
                                "OnPlayClick", "Assembly-CSharp", None, 0),
    ("AutoChess.UIScripts.WindowScripts.MainWindowPlayButton",
                                "Update", "Assembly-CSharp", None, 0),
    ("AutoChess.UIScripts.WindowScripts.BattlefieldWindow.BattlefieldWindow",
                                "Start", "Assembly-CSharp", None, 0),
    ("AutoChess.UIScripts.WindowScripts.BattleSettingsWindow",
                                "Update", "Assembly-CSharp", None, 0),
    ("AutoChess.UIScripts.WindowScripts.TechnicalWindows.TwoButtonWindow",
                                "OnShown", "Assembly-CSharp", None, 0),
    ("UGUIVisual.UGUIButtonListener", "HandleClick", "Assembly-CSharp", None, 0),
    ("UGUIVisual.UGUIButtonListener", "Update", "Assembly-CSharp", None, 0),
    # LeagueBar reward overlay used by both AutoBattle and Derank.  The
    # presenter is captured on OnShown and closed from a regular Unity Update
    # tick after UnlockButtons has completed.
    ("AutoChess.LeagueFlow.Views.LeagueBarWindowPresenter",
                                "OnShown", "Assembly-CSharp", None, 0),
    ("AutoChess.LeagueFlow.Views.LeagueBarWindowPresenter",
                                "OnClose", "Assembly-CSharp", None, 0),
    ("AutoChess.LeagueFlow.Views.LeagueBarWindow",
                                "UnlockButtons", "Assembly-CSharp", None, 0),
    ("AutoChess.LeagueFlow.Views.BarViews.LeagueBarScroll",
                                "Update", "Assembly-CSharp", None, 0),
    ("AutoChess.UIScripts.WindowScripts.MultichestWindow.CounterElement",
                                "Update", "Assembly-CSharp", None, 0),
    ("UI_Scripts.WindowManager.MultichestWindow",
                                "OnEnable", "Assembly-CSharp", None, 0),
    ("UI_Scripts.WindowManager.MultichestWindow",
                                "Start", "Assembly-CSharp", None, 0),
    ("UI_Scripts.WindowManager.MultichestWindow",
                                "OnOpenAll", "Assembly-CSharp", None, 0),
    ("UI_Scripts.WindowManager.MultichestWindow",
                                "OnCloseAction", "Assembly-CSharp", None, 0),
    ("UI_Scripts.WindowManager.MultichestWindow",
                                "WindowHidden", "Assembly-CSharp", None, 0),
    ("AutoChess.UIScripts.WindowScripts.Bundles.BundleForceShowWindow",
                                "OnFocus", "Assembly-CSharp", None, 1),
    ("AutoChess.UIScripts.WindowScripts.Bundles.BundleForceShowWindow",
                                "OnClose", "Assembly-CSharp", None, 0),
    ("AutoChess.LocalServer.LocalPhaseController", ".ctor",
                                                     "Assembly-CSharp",      None,       7),
    ("AutoChess.LocalServer.LocalServerEmulator", "InvokeOnSlotsPriceUpdate",
                                                     "Assembly-CSharp",      None,       3),
    ("AutoChess.CoreGameplay.Participant.ServerParticipant", "AddSlot",
                                                     "Assembly-CSharp",      None,       0),
    ("AutoChess.CoreGameplay.Participant.ServerPlayerParticipantController",
                                "AddCoinsAfterBattle", "Assembly-CSharp",    None,       2),
    ("AutoChess.CoreGameplay.Fight.Units.BattleUnit", "CheckDeath",
                                                     "Assembly-CSharp",      None,       2),
    ("AutoChess.CoreGameplay.Fight.Units.BattleUnit", "GetCurrentValue",
                                                     "Assembly-CSharp",      None,       1),
    ("AutoChess.CoreGameplay.Fight.Units.BattleUnit", "GetOffenseAmplify",
                                                     "Assembly-CSharp",      None,       1),
    ("AutoChess.CoreGameplay.Fight.Units.BattleUnit", "GetProtectionAmplify",
                                                     "Assembly-CSharp",      None,       1),
    ("AutoChess.CoreGameplay.Fight.Units.BattleUnit", "GetStatValue",
                                                     "Assembly-CSharp",      None,       1),
    ("AutoChess.CoreGameplay.Fight.Units.BattleUnit", "Heal",
                                                     "Assembly-CSharp",      None,       2),
    # Energy/Attack Speed — multiply mana gain and reduce turn interval
    ("AutoChess.CoreGameplay.Fight.Units.BattleUnit",
                                "ChangeMana",         "Assembly-CSharp",      None,       1),
    ("AutoChess.CoreGameplay.Fight.Units.BattleUnit",
                                "GetTurnInterval",    "Assembly-CSharp",      None,       0),
]

# Inherited generic methods are looked up by CheatsWindow's concrete class in
# the native resolver.  Keep aliases for the verified WindowScriptCore RVAs so
# the fallback path works even when metadata traversal is unavailable.
MANUAL_ALIAS_ROWS: list[dict[str, Any]] = [
    {"assembly": "Assembly-CSharp", "ns": "UI_Scripts.WindowManager",
     "cls": "MultichestWindow", "method": "ShowMultichestWindow", "argc": 0, "rva": 0x9808A0},
    {"assembly": "Assembly-CSharp", "ns": "AutoChess.Prefabs.CheatsWindow.Scripts",
     "cls": "CheatsWindow", "method": ".cctor", "argc": 0, "rva": 0x2BE3CA0},
    {"assembly": "Assembly-CSharp", "ns": "AutoChess.Prefabs.CheatsWindow.Scripts",
     "cls": "CheatsWindow", "method": "LoadWindow", "argc": 0, "rva": 0x2BE3400},
    {"assembly": "Assembly-CSharp", "ns": "AutoChess.Prefabs.CheatsWindow.Scripts",
     "cls": "CheatsWindow", "method": "get_instance", "argc": 0, "rva": 0x2BE3F80},
    {"assembly": "Assembly-CSharp", "ns": "AutoChess.Prefabs.CheatsWindow.Scripts",
     "cls": "CheatsWindow", "method": "Show", "argc": 2, "rva": 0x2BE3860},
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



def _legacy_targets() -> list[MethodTarget]:
    return [MethodTarget(d[0], d[1], argc=d[4], signature_contains=d[5] if len(d) > 5 else None, assembly=d[2], namespace_override=d[3]) for d in DESIRED]


def render_header(rows: Sequence[dict[str, Any]]) -> str:
    """Render the legacy fallback-table C header from resolved rows only."""
    lines = [
        "// Auto-generated by tools/gen_method_fallback.py",
        "// DO NOT EDIT — regenerate when method-pointer-map.json changes.",
        "#pragma once",
        "#include <stdint.h>",
        "#include <stddef.h>",
        "",
        "typedef struct {",
        "    const char* assembly;",
        "    const char* ns;",
        "    const char* klass;",
        "    const char* method;",
        "    int argc;",
        "    uintptr_t rva;",
        "} MethodFallbackEntry;",
        "",
        f"static MethodFallbackEntry g_methodFallbackTable[{len(rows)}] = {{",
    ]
    for row in sorted(rows, key=lambda row: int(row["rva"])):
        rva = int(row["rva"])
        rva_text = f"0x{rva:X}" if rva else "0x0"
        lines.append(
            f'    {{"{row["assembly"]}", "{row["ns"]}", "{row["cls"]}", '
            f'"{row["method"]}", {row["argc"]}, {rva_text}}},'
        )
    lines.extend([
        "};",
        "",
        "static const size_t g_methodFallbackCount = "
        "sizeof(g_methodFallbackTable) / sizeof(g_methodFallbackTable[0]);",
        "",
    ])
    return "\n".join(lines)


def _settings_from_args(args: argparse.Namespace) -> GhidraSettings:
    """Build extraction settings only when extraction was explicitly requested."""
    if not args.ghidra_home or not args.game_assembly:
        raise ValueError("--extract-code requires --ghidra-home and --game-assembly")
    workspace = Path(args.workspace)
    cache_dir = Path(args.cache_dir)
    if not cache_dir.is_absolute():
        cache_dir = workspace / cache_dir
    return GhidraSettings(
        ghidra_home=Path(args.ghidra_home),
        game_assembly=Path(args.game_assembly),
        workspace=workspace,
        cache_dir=cache_dir,
        timeout_seconds=args.timeout_seconds,
        decompile_timeout_seconds=args.decompile_timeout_seconds,
    )


def _run_extraction(args: argparse.Namespace, resolutions: Sequence[Any]) -> None:
    """Run only the explicitly requested extraction mode; full mode never starts jobs."""
    if args.extract_code == "none":
        return
    settings = _settings_from_args(args)
    if args.extract_code == "targeted":
        report_dir = Path(args.report_dir)
        if not report_dir.is_absolute():
            report_dir = settings.workspace / report_dir
        extract_targeted(resolutions, settings, report_dir)
        return
    job = read_job(settings.cache_dir, game_fingerprint(settings.game_assembly))
    if job.get("status") != "ready":
        raise ValueError("--extract-code full requires an existing ready full-analysis job")


def main(argv: Sequence[str] | None = None) -> None:
    parser = argparse.ArgumentParser(description="Generate method fallback C header")
    parser.add_argument("--json", default=str(DEFAULT_JSON))
    parser.add_argument("--output", default=str(OUTPUT))
    parser.add_argument("--target", action="append", default=[])
    parser.add_argument("--targets")
    parser.add_argument("--extract-code", choices=("none", "targeted", "full"), default="none")
    parser.add_argument("--ghidra-home")
    parser.add_argument("--game-assembly")
    parser.add_argument("--workspace", default=str(Path(__file__).resolve().parent))
    parser.add_argument("--cache-dir", default=".cache")
    parser.add_argument("--report-dir", default="reports")
    parser.add_argument("--timeout-seconds", type=int, default=3600)
    parser.add_argument("--decompile-timeout-seconds", type=int, default=60)
    args = parser.parse_args(argv)

    entries, warnings = load_method_entries(Path(args.json))
    targets = [parse_target(value) for value in args.target]
    if args.targets:
        targets.extend(load_targets(Path(args.targets)))
    if not targets:
        targets = _legacy_targets()

    rows: list[dict[str, Any]] = []
    resolutions = []
    missing = ambiguous = False
    for target in targets:
        result = resolve_target(target, entries)
        resolutions.append(result)
        if result.status == "not_found":
            missing = True
            print(f"NOT FOUND: {target.type_name}::{target.method_name}", file=sys.stderr)
            continue
        if result.status != "resolved":
            ambiguous = True
            print(f"AMBIGUOUS/NON-DECOMPILABLE: {target.type_name}::{target.method_name}", file=sys.stderr)
            continue
        entry = result.entry
        assert entry is not None and entry.rva is not None
        ns, cls = split_type_name(entry.type_name)
        rows.append({
            "assembly": target.assembly,
            "ns": target.namespace_override if target.namespace_override is not None else ns,
            "cls": cls,
            "method": target.method_name,
            "argc": target.argc if target.argc is not None else 0,
            "rva": entry.rva,
        })

    out = header_output_path(Path(args.output), Path(__file__).resolve().parent)
    rows.extend(MANUAL_ALIAS_ROWS)
    atomic_write_text(out, render_header(rows))
    _run_extraction(args, resolutions)
    for warning in warnings:
        print(f"WARNING: {warning}", file=sys.stderr)
    print(f"Wrote {out} ({len(rows)} entries)")
    if ambiguous:
        raise SystemExit(5)
    if missing:
        raise SystemExit(4)


if __name__ == "__main__":
    main()
