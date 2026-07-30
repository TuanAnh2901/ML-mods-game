#!/usr/bin/env py -3.13
"""Search method-pointer-map.json for types/methods matching keywords.

Usage:
    py -3.13 tools/query_methods.py damage takedamage hp
    py -3.13 tools/query_methods.py --type Gacha
    py -3.13 tools/query_methods.py --type ItemModule --show-all
"""

import argparse
import json
import re
import sys
from collections import Counter
from pathlib import Path

DEFAULT_JSON = Path(
    r"D:\SteamLibrary\steamapps\common\Everlusting Life"
    r"\Analysis\Cpp2IL-method-map\method-pointer-map.json"
)


def main() -> None:
    ap = argparse.ArgumentParser()
    ap.add_argument("keywords", nargs="*", help="substrings matched against type::method")
    ap.add_argument("--type", action="append", default=[], help="substring matched against type only")
    ap.add_argument("--method", action="append", default=[], help="substring matched against method only")
    ap.add_argument("--limit", type=int, default=200)
    ap.add_argument("--sig", action="store_true", help="print full signature instead of type::method")
    ap.add_argument("--re", action="append", default=[], help="regex matched against type::method (use \\b for word boundaries)")
    ap.add_argument("--json", default=str(DEFAULT_JSON))
    args = ap.parse_args()

    if not (args.keywords or args.type or args.method or args.re):
        ap.error("need at least one keyword, --type, --method, or --re")

    with open(args.json, "r", encoding="utf-8") as f:
        entries = json.load(f)

    # Cpp2IL folds identical method bodies onto one RVA. An RVA shared by many
    # methods is a dedup'd stub (e.g. `return null`), never a real hook target.
    rva_users = Counter(e.get("rva", "") for e in entries)

    kw = [k.lower() for k in args.keywords]
    tf = [t.lower() for t in args.type]
    mf = [m.lower() for m in args.method]
    rx = [re.compile(r, re.IGNORECASE) for r in args.re]

    hits = []
    for e in entries:
        t = e.get("type", "")
        m = e.get("method", "")
        tl, ml = t.lower(), m.lower()
        combined = f"{tl}::{ml}"
        if kw and not any(k in combined for k in kw):
            continue
        if tf and not any(x in tl for x in tf):
            continue
        if mf and not any(x in ml for x in mf):
            continue
        if rx and not any(r.search(f"{t}::{m}") for r in rx):
            continue
        hits.append(e)

    print(f"{len(hits)} hits (showing {min(len(hits), args.limit)})")
    for e in hits[: args.limit]:
        rva = e.get("rva", "?")
        shared = rva_users.get(rva, 1)
        tag = f" [SHARED x{shared}]" if shared > 1 else ""
        body = e.get("signature", "?") if args.sig else f"{e.get('type','?')}::{e.get('method','?')}"
        print(f"{rva:>12}  {body}{tag}")


if __name__ == "__main__":
    main()
