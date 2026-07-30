#!/usr/bin/env py -3.13
"""Query and resolve Cpp2IL method-map entries."""

import argparse
import ctypes
import ctypes.wintypes
import hashlib
import json
import os
import signal
import time
import re
import subprocess
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


@dataclass(frozen=True)
class GhidraSettings:
    ghidra_home: Path
    game_assembly: Path
    workspace: Path
    cache_dir: Path
    timeout_seconds: int
    decompile_timeout_seconds: int


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


def atomic_write_text(path: Path, content: str) -> None:
    """Atomically write UTF-8 text, creating the destination directory."""
    temp = path.with_suffix(path.suffix + ".tmp")
    temp.parent.mkdir(parents=True, exist_ok=True)
    temp.write_text(content, encoding="utf-8")
    temp.replace(path)


def game_fingerprint(game_assembly: Path) -> str:
    """Return a stable SHA-256 fingerprint without loading a whole binary at once."""
    digest = hashlib.sha256()
    with game_assembly.open("rb") as source:
        while chunk := source.read(1024 * 1024):
            digest.update(chunk)
    return digest.hexdigest()


def _post_script_source(decompile_timeout_seconds: int) -> str:
    """Create the small, targeted Jython script passed to analyzeHeadless."""
    return f'''# Targeted method extraction; deliberately does not enable auto-analysis.
import json
from ghidra.app.decompiler import DecompInterface
from ghidra.program.disassemble import Disassembler

request_path = getScriptArgs()[0]
response_path = getScriptArgs()[1]
request = json.loads(open(request_path, "r").read())
decompiler = DecompInterface()
decompiler.openProgram(currentProgram)
results = []
for item in request["methods"]:
    rva = int(item["rva"])
    result = {{"rva": rva, "status": "error", "pseudocode": "", "disassembly": "", "error": ""}}
    try:
        address = currentProgram.getImageBase().add(rva)
        function = getFunctionContaining(address)
        if function is None:
            Disassembler.getDisassembler(currentProgram, monitor, None).disassemble(address, None)
            function = createFunction(address, None)
        if function is None:
            raise RuntimeError("no function at RVA 0x%X" % rva)
        decompiled = decompiler.decompileFunction(function, {decompile_timeout_seconds}, monitor)
        if not decompiled.decompileCompleted():
            raise RuntimeError(decompiled.getErrorMessage())
        result["pseudocode"] = str(decompiled.getDecompiledFunction().getC())
        listing = currentProgram.getListing()
        instruction = listing.getInstructionAt(address)
        result["disassembly"] = str(instruction) if instruction else ""
        result["status"] = "ok"
    except Exception as error:
        result["error"] = str(error)
    results.append(result)
open(response_path, "w").write(json.dumps({{"results": results}}))
'''


def _reject_backup_path(path: Path) -> Path:
    """Resolve a path only after rejecting backup-tree components."""
    resolved = path.resolve()
    if any(part.casefold() in {"before", "tools before"} for part in resolved.parts):
        raise ValueError("path must not use a backup directory")
    return resolved


def _validated_headless(settings: GhidraSettings) -> Path:
    _reject_backup_path(settings.ghidra_home)
    _reject_backup_path(settings.game_assembly)
    executable = settings.ghidra_home / "support" / "analyzeHeadless.bat"
    if not executable.is_file():
        raise ValueError(f"missing Ghidra headless executable: {executable}")
    if not settings.game_assembly.is_file():
        raise ValueError(f"missing game assembly: {settings.game_assembly}")
    return executable


def extract_targeted(
    resolutions: Sequence[Resolution], settings: GhidraSettings, report_dir: Path
) -> dict[str, Any]:
    """Extract only resolved RVAs through a no-analysis Ghidra headless run."""
    _reject_backup_path(settings.ghidra_home)
    _reject_backup_path(settings.game_assembly)
    _reject_backup_path(settings.workspace)
    _reject_backup_path(settings.cache_dir)
    _reject_backup_path(report_dir)
    executable = _validated_headless(settings)
    workspace = workspace_path(settings.workspace, settings.workspace)
    report_dir = workspace_path(report_dir, workspace)
    cache_dir = workspace_path(settings.cache_dir, workspace)
    fingerprint = game_fingerprint(settings.game_assembly)
    script_dir = cache_dir / "ghidra-method-tools" / "scripts"
    request_path = script_dir / "targeted-request.json"
    response_path = script_dir / "targeted-response.json"
    script_path = script_dir / "extract_targeted.py"
    requested = [
        resolution for resolution in resolutions
        if resolution.status == "resolved" and resolution.entry is not None and resolution.entry.rva is not None
    ]
    atomic_write_text(script_path, _post_script_source(settings.decompile_timeout_seconds))
    atomic_write_text(
        request_path,
        json.dumps({"methods": [{"rva": item.entry.rva} for item in requested]}) + "\n",
    )
    response_path.unlink(missing_ok=True)
    project_dir = cache_dir / "ghidra-method-tools" / "projects"
    command = [
        str(executable), "-project", str(project_dir), "-projectName", fingerprint,
        "-import", str(settings.game_assembly), "-noanalysis", "-postScript",
        str(script_path), str(request_path), str(response_path),
    ]
    extraction_by_rva: dict[int, dict[str, Any]] = {}
    run_error = ""
    timeout = False
    if requested:
        try:
            completed = subprocess.run(
                command, timeout=settings.timeout_seconds, text=True, capture_output=True, check=False
            )
            if completed.returncode:
                run_error = (completed.stderr or completed.stdout or "analyzeHeadless failed").strip()
            elif response_path.is_file():
                response = json.loads(response_path.read_text(encoding="utf-8"))
                for item in response.get("results", []):
                    if isinstance(item, dict) and isinstance(item.get("rva"), int):
                        extraction_by_rva[item["rva"]] = item
            else:
                run_error = "analyzeHeadless did not produce a response"
        except subprocess.TimeoutExpired:
            timeout = True

    results: list[dict[str, Any]] = []
    for resolution in resolutions:
        metadata = _resolution_metadata(resolution)
        rva = resolution.entry.rva if resolution.entry else None
        extracted = extraction_by_rva.get(rva) if rva is not None else None
        if timeout and resolution in requested:
            metadata.update(extraction_status="timeout", extraction_error="analyzeHeadless timed out")
        elif extracted is not None:
            metadata.update(
                extraction_status=extracted.get("status", "error"),
                extraction_error=extracted.get("error", ""),
            )
            if extracted.get("status") == "ok":
                slug = method_slug(metadata["type"], metadata["method"], metadata["rva"])
                atomic_write_text(report_dir / "methods" / slug / "code.c", str(extracted.get("pseudocode", "")))
                atomic_write_text(report_dir / "methods" / slug / "disassembly.asm", str(extracted.get("disassembly", "")))
        elif resolution in requested:
            metadata.update(extraction_status="error", extraction_error=run_error or "missing extraction result")
        else:
            metadata.update(extraction_status="not_requested", extraction_error="")
        results.append(metadata)
    manifest = {
        "game_fingerprint": fingerprint,
        "command": command,
        "exit_code": 7 if timeout else 0,
        "results": results,
    }
    write_report(report_dir, manifest)
    return manifest


def method_slug(type_name: str, method_name: str, rva_text: str) -> str:
    """Return a stable, filesystem-safe directory name for method metadata."""
    def clean(value: str) -> str:
        return re.sub(r"[^A-Za-z0-9._-]+", "_", value).strip("._") or "unknown"

    return "__".join((clean(type_name), clean(method_name), clean(rva_text)))


def write_report(report_dir: Path, manifest: dict[str, Any]) -> None:
    """Write a manifest, a concise Markdown summary, and per-method metadata."""
    atomic_write_text(
        report_dir / "manifest.json",
        json.dumps(manifest, indent=2, sort_keys=True) + "\n",
    )
    results = manifest.get("results", [])
    lines = ["# Method resolution report", "", "| Status | Type | Method | RVA |", "| --- | --- | --- | --- |"]
    for result in results:
        lines.append(
            "| {status} | {type_name} | {method} | {rva} |".format(
                status=str(result.get("status", "")),
                type_name=str(result.get("type", "")).replace("|", "\\|"),
                method=str(result.get("method", "")).replace("|", "\\|"),
                rva=str(result.get("rva", "")),
            )
        )
        slug = method_slug(
            str(result.get("type", "")),
            str(result.get("method", "")),
            str(result.get("rva", "")),
        )
        atomic_write_text(
            report_dir / "methods" / slug / "metadata.json",
            json.dumps(result, indent=2, sort_keys=True) + "\n",
        )
    atomic_write_text(report_dir / "summary.md", "\n".join(lines) + "\n")


def _add_search_arguments(parser: argparse.ArgumentParser) -> None:
    parser.add_argument("keywords", nargs="*", help="substrings matched against type::method")
    parser.add_argument("--type", action="append", default=[], help="substring matched against type only")
    parser.add_argument("--method", action="append", default=[], help="substring matched against method only")
    parser.add_argument("--limit", type=int, default=200)
    parser.add_argument("--sig", action="store_true", help="print full signature instead of type::method")
    parser.add_argument("--re", action="append", default=[], help="regex matched against type::method")
    parser.add_argument("--json", default=str(DEFAULT_JSON))


def _resolution_metadata(resolution: Resolution) -> dict[str, Any]:
    entry = resolution.entry
    return {
        "status": resolution.status,
        "type": resolution.target.type_name,
        "method": resolution.target.method_name,
        "argc": resolution.target.argc,
        "signature_contains": resolution.target.signature_contains,
        "assembly": resolution.target.assembly,
        "namespace_override": resolution.target.namespace_override,
        "rva": entry.rva_text if entry else "",
        "signature": entry.signature if entry else "",
        "source_index": entry.source_index if entry else None,
        "shared_rva_count": resolution.shared_rva_count,
        "candidates": [
            {"type": candidate.type_name, "method": candidate.method_name,
             "signature": candidate.signature, "rva": candidate.rva_text}
            for candidate in resolution.candidates
        ],
    }


def _run_search(args: argparse.Namespace) -> None:
    if not (args.keywords or args.type or args.method or args.re):
        raise ValueError("need at least one keyword, --type, --method, or --re")
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


def _run_extract(args: argparse.Namespace) -> None:
    if bool(args.target) == bool(args.targets):
        raise ValueError("extract requires exactly one of --target or --targets")
    targets = [parse_target(value) for value in args.target] if args.target else load_targets(Path(args.targets))
    entries, warnings = load_method_entries(Path(args.json))
    manifest = {
        "source": str(Path(args.json).resolve()),
        "warnings": warnings,
        "results": [_resolution_metadata(resolve_target(target, entries)) for target in targets],
    }
    report_candidate = Path(args.report_dir)
    if not report_candidate.is_absolute():
        report_candidate = Path(args.workspace) / report_candidate
    report_dir = workspace_path(report_candidate, Path(args.workspace))
    write_report(report_dir, manifest)
    print(f"wrote report: {report_dir}")


_RUNNING_PROCESSES: list[subprocess.Popen[Any]] = []


class JobConflictError(RuntimeError):
    """Raised when a full-analysis job is already active for an assembly."""


def _job_dir(cache_dir: Path, fingerprint: str) -> Path:
    return cache_dir / "ghidra-method-tools" / "jobs" / fingerprint


def _job_manifest_path(cache_dir: Path, fingerprint: str) -> Path:
    return _job_dir(cache_dir, fingerprint) / "manifest.json"


def _job_lock_path(cache_dir: Path, fingerprint: str) -> Path:
    return _job_dir(cache_dir, fingerprint) / "lock.json"


def _write_job_manifest(path: Path, job: dict[str, Any]) -> None:
    job["updated_at"] = time.time()
    atomic_write_text(path, json.dumps(job, indent=2, sort_keys=True) + "\n")


def _process_start_token(pid: object) -> str | None:
    """Return a process-creation token suitable for detecting PID reuse."""
    if not isinstance(pid, int) or pid <= 0:
        return None
    if sys.platform == "win32":
        kernel32 = ctypes.windll.kernel32
        handle = kernel32.OpenProcess(0x0400, False, pid)
        if not handle:
            return None
        try:
            created = ctypes.wintypes.FILETIME()
            exited = ctypes.wintypes.FILETIME()
            kernel = ctypes.wintypes.FILETIME()
            user = ctypes.wintypes.FILETIME()
            if not kernel32.GetProcessTimes(handle, ctypes.byref(created), ctypes.byref(exited), ctypes.byref(kernel), ctypes.byref(user)):
                return None
            return str((created.dwHighDateTime << 32) | created.dwLowDateTime)
        finally:
            kernel32.CloseHandle(handle)
    try:
        stat = Path(f"/proc/{pid}/stat").read_text(encoding="utf-8")
        return stat.rsplit(")", 1)[1].split()[19]
    except (OSError, IndexError):
        return None


def _matches_process_identity(job: dict[str, Any]) -> bool:
    return _process_start_token(job.get("pid")) == job.get("process_start_token")


def _process_is_live(pid: object) -> bool:
    if not isinstance(pid, int) or pid <= 0:
        return False
    if sys.platform == "win32":
        result = subprocess.run(
            ["tasklist", "/FI", f"PID eq {pid}", "/FO", "CSV", "/NH"],
            capture_output=True, text=True, check=False,
        )
        return str(pid) in result.stdout
    try:
        os.kill(pid, 0)
    except OSError:
        return False
    return True


def refresh_job_status(job: dict[str, Any]) -> dict[str, Any]:
    """Update a job manifest in place from its PID, exit marker, and log age."""
    status = str(job.get("status", ""))
    if status in {"ready", "failed", "cancelled"}:
        return job
    manifest_path = Path(str(job["manifest_path"]))
    log_path = Path(str(job["log_path"]))
    live = _process_is_live(job.get("pid"))
    if not live:
        exit_path = Path(str(job.get("exit_code_path", "")))
        try:
            exit_code = int(exit_path.read_text(encoding="utf-8").strip())
        except (OSError, ValueError):
            exit_code = 1
        job["exit_code"] = exit_code
        job["status"] = "ready" if exit_code == 0 else "failed"
        _write_job_manifest(manifest_path, job)
        _job_lock_path(Path(str(job["cache_dir"])), str(job["fingerprint"])).unlink(missing_ok=True)
    elif status == "running" and time.time() - log_path.stat().st_mtime > 15 * 60:
        job["status"] = "stale"
        _write_job_manifest(manifest_path, job)
    return job


def read_job(cache_dir: Path, fingerprint: str) -> dict[str, Any]:
    """Read and refresh the persisted full-analysis job manifest."""
    cache_dir = _reject_backup_path(cache_dir)
    manifest_path = _job_manifest_path(cache_dir, fingerprint)
    job = json.loads(manifest_path.read_text(encoding="utf-8"))
    return refresh_job_status(job)


def start_full_analysis(settings: GhidraSettings) -> Path:
    """Launch a detached, cached full Ghidra analysis and return its manifest path."""
    _reject_backup_path(settings.ghidra_home)
    _reject_backup_path(settings.game_assembly)
    _reject_backup_path(settings.workspace)
    _reject_backup_path(settings.cache_dir)
    executable = _validated_headless(settings)
    workspace = workspace_path(settings.workspace, settings.workspace)
    cache_dir = workspace_path(settings.cache_dir, workspace)
    fingerprint = game_fingerprint(settings.game_assembly)
    job_dir = _job_dir(cache_dir, fingerprint)
    manifest_path = _job_manifest_path(cache_dir, fingerprint)
    lock_path = _job_lock_path(cache_dir, fingerprint)
    job_dir.mkdir(parents=True, exist_ok=True)
    if lock_path.exists():
        try:
            existing = read_job(cache_dir, fingerprint)
        except (OSError, json.JSONDecodeError, KeyError):
            existing = {"status": "running"}
        if existing.get("status") in {"running", "stale"}:
            raise JobConflictError(f"full analysis already active for {fingerprint}")
        lock_path.unlink(missing_ok=True)
    try:
        descriptor = os.open(lock_path, os.O_WRONLY | os.O_CREAT | os.O_EXCL)
    except FileExistsError as error:
        raise JobConflictError(f"full analysis already active for {fingerprint}") from error
    with os.fdopen(descriptor, "w", encoding="utf-8") as lock_file:
        json.dump({"fingerprint": fingerprint, "created_at": time.time()}, lock_file)
    project_dir = cache_dir / "ghidra-method-tools" / "projects"
    command = [
        str(executable), "-project", str(project_dir), "-projectName", fingerprint,
        "-import", str(settings.game_assembly),
    ]
    log_path = job_dir / "analysis.log"
    exit_code_path = job_dir / "exit-code.txt"
    exit_code_path.unlink(missing_ok=True)
    shell_command = f"{subprocess.list2cmdline(command)} & echo !ERRORLEVEL! > {subprocess.list2cmdline([str(exit_code_path)])}"
    try:
        with log_path.open("ab") as log_file:
            process = subprocess.Popen(
                ["cmd.exe", "/d", "/v:on", "/s", "/c", shell_command], stdout=log_file, stderr=log_file,
                creationflags=getattr(subprocess, "CREATE_NO_WINDOW", 0),
            )
    except Exception:
        lock_path.unlink(missing_ok=True)
        raise
    _RUNNING_PROCESSES.append(process)
    now = time.time()
    job = {
        "fingerprint": fingerprint, "pid": process.pid, "command": command,
        "launch_command": ["cmd.exe", "/d", "/v:on", "/s", "/c", shell_command],
        "process_start_token": _process_start_token(process.pid),
        "created_at": now, "started_at": now, "updated_at": now,
        "status": "running", "exit_code": None, "log_path": str(log_path),
        "exit_code_path": str(exit_code_path), "manifest_path": str(manifest_path),
        "cache_dir": str(cache_dir),
    }
    _write_job_manifest(manifest_path, job)
    return manifest_path


def cancel_job(cache_dir: Path, fingerprint: str) -> dict[str, Any]:
    """Terminate a full-analysis process and preserve its cancelled manifest."""
    job = read_job(cache_dir, fingerprint)
    if job.get("status") not in {"ready", "failed", "cancelled"} and _matches_process_identity(job):
        if sys.platform == "win32":
            subprocess.run(["taskkill", "/PID", str(job["pid"]), "/T", "/F"], capture_output=True, check=False)
        else:
            os.kill(job["pid"], signal.SIGTERM)
    for process in list(_RUNNING_PROCESSES):
        if process.pid == job.get("pid"):
            try:
                process.wait(timeout=2)
            except subprocess.TimeoutExpired:
                pass
            _RUNNING_PROCESSES.remove(process)
    job["status"] = "cancelled"
    _write_job_manifest(_job_manifest_path(cache_dir, fingerprint), job)
    _job_lock_path(cache_dir, fingerprint).unlink(missing_ok=True)
    return job


def _add_full_analysis_arguments(parser: argparse.ArgumentParser) -> None:
    parser.add_argument("--ghidra-home", required=True)
    parser.add_argument("--game-assembly", required=True)
    parser.add_argument("--workspace", default=str(Path.cwd()))
    parser.add_argument("--cache-dir", default=".cache")
    parser.add_argument("--timeout-seconds", type=int, default=3600)
    parser.add_argument("--decompile-timeout-seconds", type=int, default=60)


def _settings_from_args(args: argparse.Namespace) -> GhidraSettings:
    workspace = Path(args.workspace)
    cache_dir = Path(args.cache_dir)
    if not cache_dir.is_absolute():
        cache_dir = workspace / cache_dir
    return GhidraSettings(
        ghidra_home=Path(args.ghidra_home), game_assembly=Path(args.game_assembly),
        workspace=workspace, cache_dir=cache_dir, timeout_seconds=args.timeout_seconds,
        decompile_timeout_seconds=args.decompile_timeout_seconds,
    )


def _run_prepare_full(args: argparse.Namespace) -> None:
    print(start_full_analysis(_settings_from_args(args)))


def _run_job_status(args: argparse.Namespace) -> None:
    print(json.dumps(read_job(Path(args.cache_dir), args.fingerprint), indent=2, sort_keys=True))


def _run_cancel_job(args: argparse.Namespace) -> None:
    print(json.dumps(cancel_job(Path(args.cache_dir), args.fingerprint), indent=2, sort_keys=True))

def _normalize_argv(argv: Sequence[str]) -> list[str]:
    if not argv:
        return ["search"]
    if argv[0] in {"search", "extract", "prepare-full", "status", "cancel", "-h", "--help"}:
        return list(argv)
    return ["search", *argv]


def main(argv: Sequence[str] | None = None) -> None:
    ap = argparse.ArgumentParser()
    subparsers = ap.add_subparsers(dest="command")
    search_parser = subparsers.add_parser("search", help="search method metadata")
    _add_search_arguments(search_parser)
    extract_parser = subparsers.add_parser("extract", help="resolve targets and write metadata reports")
    extract_parser.add_argument("--target", action="append", default=[])
    extract_parser.add_argument("--targets")
    extract_parser.add_argument("--json", default=str(DEFAULT_JSON))
    extract_parser.add_argument("--report-dir", default="reports")
    extract_parser.add_argument("--workspace", default=str(Path.cwd()))
    prepare_parser = subparsers.add_parser("prepare-full", help="launch detached full Ghidra analysis")
    _add_full_analysis_arguments(prepare_parser)
    status_parser = subparsers.add_parser("status", help="read a detached full-analysis job")
    status_parser.add_argument("fingerprint")
    status_parser.add_argument("--cache-dir", default=".cache")
    cancel_parser = subparsers.add_parser("cancel", help="cancel a detached full-analysis job")
    cancel_parser.add_argument("fingerprint")
    cancel_parser.add_argument("--cache-dir", default=".cache")
    args = ap.parse_args(_normalize_argv(list(sys.argv[1:] if argv is None else argv)))
    try:
        if args.command == "search":
            _run_search(args)
        elif args.command == "extract":
            _run_extract(args)
        elif args.command == "prepare-full":
            _run_prepare_full(args)
        elif args.command == "status":
            _run_job_status(args)
        elif args.command == "cancel":
            _run_cancel_job(args)
        else:
            ap.print_help()
    except JobConflictError as error:
        print(str(error), file=sys.stderr)
        raise SystemExit(8) from error
    except ValueError as error:
        ap.error(str(error))


if __name__ == "__main__":
    main()
