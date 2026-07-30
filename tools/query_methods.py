#!/usr/bin/env py -3.13
"""Query and resolve Cpp2IL method-map entries."""

import argparse
import json
import re
import sys
from collections import Counter
from dataclasses import dataclass
from pathlib import Path
from typing import Any, Sequence

DEFAULT_JSON = Path(
    r"D:\SteamLibrary\steamapps\common\Everlusting Life"
    r"\Analysis\Cpp2IL-method-map\method-pointer-map.json"
)


@dataclass(frozen=True)
class MethodEntry:
    type_name: str
    method_name: str
    signature: str
    rva: int | None
    rva_text: str
    source_index: int
    raw: dict[str, Any]


@dataclass(frozen=True)
class MethodTarget:
    type_name: str
    method_name: str
    argc: int | None = None
    signature_contains: str | None = None
    assembly: str = "Assembly-CSharp"
    namespace_override: str | None = None


@dataclass(frozen=True)
class Resolution:
    target: MethodTarget
    status: str
    entry: MethodEntry | None
    candidates: tuple[MethodEntry, ...]
    shared_rva_count: int = 0


def parse_rva(value: object) -> tuple[int | None, str]:
    """Return a positive RVA, if present, together with its original text."""
    text = "" if value is None else str(value).strip()
    try:
        number = value if isinstance(value, int) else int(text, 0)
    except (TypeError, ValueError):
        return None, text
    return (number if number > 0 else None), text or f"0x{number:X}"


def _parameters(value: str) -> list[str] | None:
    """Extract top-level parameters from the first parenthesized parameter list."""
    start = value.find("(")
    if start < 0:
        return None
    depth = 0
    items: list[str] = []
    current: list[str] = []
    for character in value[start + 1 :]:
        if character == "(" or character == "<" or character == "[":
            depth += 1
        elif character == ")":
            if depth == 0:
                items.append("".join(current).strip())
                return [] if items == [""] else items
            depth -= 1
        elif character == ">" or character == "]":
            if depth:
                depth -= 1
        if character == "," and depth == 0:
            items.append("".join(current).strip())
            current = []
        else:
            current.append(character)
    return None


def count_signature_params(signature: str) -> int:
    """Count top-level parameters in a Cpp2IL-style signature."""
    parameters = _parameters(signature)
    return len(parameters) if parameters is not None else 0


def parse_target(value: str) -> MethodTarget:
    """Parse ``Type::Method`` with an optional parenthesized signature."""
    text = value.strip()
    if "::" not in text:
        raise ValueError("target must use Type::Method syntax")
    type_name, method_part = (part.strip() for part in text.split("::", 1))
    if not type_name or not method_part:
        raise ValueError("target requires both type and method")
    start = method_part.find("(")
    if start < 0:
        return MethodTarget(type_name, method_part)
    if not method_part.endswith(")"):
        raise ValueError("target signature must end with a closing parenthesis")
    method_name = method_part[:start].strip()
    if not method_name:
        raise ValueError("target requires a method name")
    return MethodTarget(type_name, method_name, argc=count_signature_params(method_part))


def _read_json_array(path: Path, key: str | None = None) -> list[Any]:
    data = json.loads(path.read_text(encoding="utf-8"))
    if isinstance(data, dict) and key is not None:
        data = data.get(key, data)
    if not isinstance(data, list):
        raise ValueError(f"{path} must contain a JSON array")
    return data


def load_method_entries(path: Path) -> tuple[list[MethodEntry], list[str]]:
    """Load root-array method metadata, retaining malformed rows as warnings."""
    rows = _read_json_array(path)
    entries: list[MethodEntry] = []
    warnings: list[str] = []
    for index, row in enumerate(rows):
        if not isinstance(row, dict):
            warnings.append(f"index {index}: entry is not an object")
            continue
        type_name = str(row.get("type", "")).strip()
        method_name = str(row.get("method", "")).strip()
        if not type_name or not method_name:
            warnings.append(f"index {index}: missing type or method")
            continue
        rva, rva_text = parse_rva(row.get("rva"))
        entries.append(
            MethodEntry(
                type_name,
                method_name,
                str(row.get("signature", "")).strip(),
                rva,
                rva_text,
                index,
                row,
            )
        )
    return entries, warnings


def _target_from_mapping(value: dict[str, Any]) -> MethodTarget:
    type_name = str(value.get("type", "")).strip()
    method_name = str(value.get("method", "")).strip()
    if not type_name or not method_name:
        raise ValueError("target requires type and method")
    argc = value.get("argc")
    if argc is not None and (isinstance(argc, bool) or not isinstance(argc, int) or argc < 0):
        raise ValueError("target argc must be a non-negative integer")
    signature_contains = value.get("signature_contains")
    if signature_contains is not None and not isinstance(signature_contains, str):
        raise ValueError("target signature_contains must be a string or null")
    return MethodTarget(
        type_name,
        method_name,
        argc=argc,
        signature_contains=signature_contains,
        assembly=value.get("assembly", "Assembly-CSharp"),
        namespace_override=value.get("namespace_override"),
    )


def load_targets(path: Path) -> list[MethodTarget]:
    """Load target specifications from a root array or ``{\"targets\": [...]}``."""
    rows = _read_json_array(path, "targets")
    targets: list[MethodTarget] = []
    for index, row in enumerate(rows):
        if isinstance(row, str):
            targets.append(parse_target(row))
        elif isinstance(row, dict):
            try:
                targets.append(_target_from_mapping(row))
            except ValueError as error:
                raise ValueError(f"target index {index}: {error}") from error
        else:
            raise ValueError(f"target index {index}: target must be a string or object")
    return targets


def resolve_target(target: MethodTarget, entries: Sequence[MethodEntry]) -> Resolution:
    """Resolve a target to exactly one method entry, if possible."""
    exact = [
        entry
        for entry in entries
        if entry.type_name.casefold() == target.type_name.casefold()
        and entry.method_name.casefold() == target.method_name.casefold()
    ]
    candidates = exact or [
        entry
        for entry in entries
        if target.type_name.casefold() in entry.type_name.casefold()
        and entry.method_name.casefold() == target.method_name.casefold()
    ]
    if target.signature_contains:
        candidates = [
            entry
            for entry in candidates
            if target.signature_contains.casefold() in entry.signature.casefold()
        ]
    if target.argc is not None:
        candidates = [
            entry for entry in candidates if count_signature_params(entry.signature) == target.argc
        ]
    if not candidates:
        return Resolution(target, "not_found", None, ())
    if len(candidates) != 1:
        return Resolution(target, "ambiguous", None, tuple(candidates))
    selected = candidates[0]
    shared = sum(1 for item in entries if item.rva and item.rva == selected.rva)
    return Resolution(
        target,
        "resolved" if selected.rva else "non_decompilable",
        selected,
        tuple(candidates),
        shared,
    )


def workspace_path(path: Path, workspace: Path) -> Path:
    """Resolve a workspace-local path while excluding backup-tree components."""
    resolved_workspace = workspace.resolve()
    resolved_path = path.resolve()
    try:
        relative = resolved_path.relative_to(resolved_workspace)
    except ValueError as error:
        raise ValueError("path must be inside the workspace") from error
    forbidden = {"before", "tools before"}
    if any(part.casefold() in forbidden for part in relative.parts):
        raise ValueError("path must not use a backup directory")
    return resolved_path


def header_output_path(path: Path, workspace: Path) -> Path:
    """Accept workspace output or the one legacy fallback header destination."""
    resolved_path = path.resolve()
    legacy = (workspace.resolve().parent / "el_native" / "method_fallback.inc").resolve()
    if resolved_path == legacy:
        return resolved_path
    return workspace_path(resolved_path, workspace)


def main() -> None:
    ap = argparse.ArgumentParser()
    ap.add_argument("keywords", nargs="*", help="substrings matched against type::method")
    ap.add_argument("--type", action="append", default=[], help="substring matched against type only")
    ap.add_argument("--method", action="append", default=[], help="substring matched against method only")
    ap.add_argument("--limit", type=int, default=200)
    ap.add_argument("--sig", action="store_true", help="print full signature instead of type::method")
    ap.add_argument("--re", action="append", default=[], help="regex matched against type::method")
    ap.add_argument("--json", default=str(DEFAULT_JSON))
    args = ap.parse_args()
    if not (args.keywords or args.type or args.method or args.re):
        ap.error("need at least one keyword, --type, --method, or --re")

    entries, warnings = load_method_entries(Path(args.json))
    for warning in warnings:
        print(f"warning: {warning}", file=sys.stderr)
    rva_users = Counter(entry.rva for entry in entries)
    keywords = [keyword.casefold() for keyword in args.keywords]
    type_filters = [value.casefold() for value in args.type]
    method_filters = [value.casefold() for value in args.method]
    expressions = [re.compile(value, re.IGNORECASE) for value in args.re]
    hits = [
        entry
        for entry in entries
        if (not keywords or any(word in f"{entry.type_name}::{entry.method_name}".casefold() for word in keywords))
        and (not type_filters or any(word in entry.type_name.casefold() for word in type_filters))
        and (not method_filters or any(word in entry.method_name.casefold() for word in method_filters))
        and (not expressions or any(expression.search(f"{entry.type_name}::{entry.method_name}") for expression in expressions))
    ]
    print(f"{len(hits)} hits (showing {min(len(hits), args.limit)})")
    for entry in hits[: args.limit]:
        shared = rva_users[entry.rva]
        tag = f" [SHARED x{shared}]" if shared > 1 else ""
        body = entry.signature if args.sig else f"{entry.type_name}::{entry.method_name}"
        print(f"{entry.rva_text or '?':>12}  {body}{tag}")


if __name__ == "__main__":
    main()
