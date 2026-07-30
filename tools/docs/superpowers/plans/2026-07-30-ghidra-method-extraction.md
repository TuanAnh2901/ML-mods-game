# Ghidra Method Extraction Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Extend the two tools to resolve IL2CPP methods safely, extract selected Ghidra code without synchronous full analysis, and write reports and fallback headers.

**Architecture:** `query_methods.py` owns metadata normalization, target resolution, workspace-safe reports, Ghidra cache and jobs. `gen_method_fallback.py` imports this public API, transforms resolved targets to a C header, and retains `DESIRED` only as a backward-compatible source of targets. A generated Ghidra post-script exchanges request/response JSON with Python.

**Tech Stack:** Python 3.12 standard library, `unittest`, Ghidra `analyzeHeadless`, Markdown.

## Global Constraints

- Write only beneath `D:\VSCode\EL_Native\tools`; never access any backup directory such as `Before` or `Tools Before`.
- Treat game binary and metadata outside the workspace as read-only inputs.
- Reject report/cache/header output outside this workspace and path components named `before` or `tools before`, case-insensitively.
- Metadata mode never launches Ghidra; targeted mode always passes `-noanalysis`; full analysis only runs through a detached `prepare-full` job.
- Preserve the legacy query CLI and legacy `DESIRED` flow.
- Use `python -m unittest discover -s tests -v`; no third-party test framework.
- Commit each finished task using only paths under `tools`.

---

## File Structure

- Modify: `query_methods.py` — shared model, CLI, resolver, report writer, Ghidra runner, jobs.
- Modify: `gen_method_fallback.py` — shared resolver input and C-header rendering.
- Create: `tests/test_query_methods.py` — resolver, CLI, report, Ghidra fake and job tests.
- Create: `tests/test_gen_method_fallback.py` — header integration tests.
- Create: `GHIDRA_METHOD_EXTRACTION_GUIDE.md` — Vietnamese detailed manual workflow.

## Task 1: Establish models, resolver, and output-path guard

**Files:**
- Create: `tests/test_query_methods.py`
- Modify: `query_methods.py:1-71`

**Interfaces produced:**

```python
@dataclass(frozen=True)
class MethodEntry:
    type_name: str; method_name: str; signature: str
    rva: int | None; rva_text: str; source_index: int; raw: dict[str, Any]

@dataclass(frozen=True)
class MethodTarget:
    type_name: str; method_name: str; argc: int | None = None
    signature_contains: str | None = None; assembly: str = "Assembly-CSharp"
    namespace_override: str | None = None

@dataclass(frozen=True)
class Resolution:
    target: MethodTarget; status: str; entry: MethodEntry | None
    candidates: tuple[MethodEntry, ...]; shared_rva_count: int = 0

def load_method_entries(path: Path) -> tuple[list[MethodEntry], list[str]]: ...
def count_signature_params(signature: str) -> int: ...
def parse_target(value: str) -> MethodTarget: ...
def load_targets(path: Path) -> list[MethodTarget]: ...
def resolve_target(target: MethodTarget, entries: Sequence[MethodEntry]) -> Resolution: ...
def workspace_path(path: Path, workspace: Path) -> Path: ...
```

- [ ] **Step 1: Write failing resolver tests**

```python
def test_arity_selects_the_correct_overload(self) -> None:
    entries = [entry("Game.Unit", "Apply", "void Apply(System.Int32)", "0x100"),
               entry("Game.Unit", "Apply", "void Apply(System.String,System.Int32)", "0x200")]
    result = query_methods.resolve_target(query_methods.MethodTarget("Game.Unit", "Apply", argc=2), entries)
    self.assertEqual("resolved", result.status)
    self.assertEqual(0x200, result.entry.rva)

def test_unqualified_overloads_are_ambiguous(self) -> None:
    result = query_methods.resolve_target(query_methods.MethodTarget("Game.Unit", "Apply"), self.two_overloads)
    self.assertEqual("ambiguous", result.status)

def test_resolved_entry_reports_shared_rva_count(self) -> None:
    result = query_methods.resolve_target(query_methods.MethodTarget("Game.Unit", "Apply", argc=1), [
        entry("Game.Unit", "Apply", "void Apply(System.Int32)", "0x100"),
        entry("Game.Other", "M", "void M()", "0x100"),
    ])
    self.assertEqual(2, result.shared_rva_count)

def test_output_path_rejects_backup_and_external_paths(self) -> None:
    with self.assertRaises(ValueError): query_methods.workspace_path(self.root.parent / "Tools Before" / "report", self.root)
    with self.assertRaises(ValueError): query_methods.workspace_path(self.root.parent / "report", self.root)
```

- [ ] **Step 2: Run the tests and observe RED**

Run: `python -m unittest tests.test_query_methods.ResolverTests -v`  
Expected: import/attribute failures because the models and resolver do not exist.

- [ ] **Step 3: Implement minimal pure functions**

```python
def parse_rva(value: object) -> tuple[int | None, str]:
    text = "" if value is None else str(value).strip()
    try: number = value if isinstance(value, int) else int(text, 0)
    except (TypeError, ValueError): return None, text
    return (number if number > 0 else None), text or f"0x{number:X}"

def resolve_target(target: MethodTarget, entries: Sequence[MethodEntry]) -> Resolution:
    exact = [e for e in entries if e.type_name.casefold() == target.type_name.casefold() and e.method_name.casefold() == target.method_name.casefold()]
    candidates = exact or [e for e in entries if target.type_name.casefold() in e.type_name.casefold() and e.method_name.casefold() == target.method_name.casefold()]
    if target.signature_contains: candidates = [e for e in candidates if target.signature_contains.casefold() in e.signature.casefold()]
    if target.argc is not None: candidates = [e for e in candidates if count_signature_params(e.signature) == target.argc]
    if not candidates: return Resolution(target, "not_found", None, ())
    if len(candidates) != 1: return Resolution(target, "ambiguous", None, tuple(candidates))
    shared = sum(1 for item in entries if item.rva and item.rva == candidates[0].rva)
    return Resolution(target, "resolved" if candidates[0].rva else "non_decompilable", candidates[0], tuple(candidates), shared)
```

Use `Path.resolve()` plus `relative_to(workspace.resolve())` in `workspace_path`; reject `before` and `tools before` components.

- [ ] **Step 4: Run GREEN**

Run: `python -m unittest tests.test_query_methods.ResolverTests -v`  
Expected: PASS.

- [ ] **Step 5: Add RED tests for JSON/CLI target parsing**

```python
def test_parse_target_derives_arity(self) -> None:
    target = query_methods.parse_target("Game.Unit::Apply(System.Int32,System.String)")
    self.assertEqual(("Game.Unit", "Apply", 2), (target.type_name, target.method_name, target.argc))

def test_metadata_loader_skips_nameless_entries_with_warning(self) -> None:
    entries, warnings = query_methods.load_method_entries(self.write_json([{"type":"Game.Unit","method":"M","rva":"0x10"}, {"rva":"0x20"}]))
    self.assertEqual(1, len(entries)); self.assertIn("index 1", warnings[0])
```

- [ ] **Step 6: Run RED, implement parsers, run GREEN**

Run: `python -m unittest tests.test_query_methods.ResolverTests -v`  
Expected before implementation: parser/loader assertion failures.  
Implement `load_targets` for `{"targets":[...]}`, requiring `type`/`method`, validating non-negative `argc`; normalize root-array metadata.  
Run: `python -m unittest tests.test_query_methods -v; python -m py_compile query_methods.py`  
Expected after implementation: PASS and exit code 0.

- [ ] **Step 7: Commit**

```powershell
git add -- query_methods.py tests/test_query_methods.py
git commit -m "feat: add method target resolver"
```

## Task 2: Add reports and backward-compatible CLI routing

**Files:**
- Modify: `query_methods.py`
- Modify: `tests/test_query_methods.py`

**Interfaces produced:** `atomic_write_text(path, content)`, `method_slug(type_name, method_name, rva_text)`, `write_report(report_dir, manifest)`, `search`, and metadata-only `extract` subcommands.

- [ ] **Step 1: Write failing report and legacy CLI tests**

```python
def test_report_contains_summary_manifest_and_per_method_metadata(self) -> None:
    query_methods.write_report(self.root / "report", {"results":[{"status":"resolved","type":"Game.Unit","method":"Apply","rva":"0x100"}]})
    self.assertTrue((self.root / "report" / "summary.md").is_file())
    self.assertTrue((self.root / "report" / "methods" / "Game.Unit__Apply__0x100" / "metadata.json").is_file())

def test_old_keyword_cli_still_succeeds(self) -> None:
    result = self.run_cli("Apply", "--json", str(self.map_path))
    self.assertEqual(0, result.returncode); self.assertIn("1 hits", result.stdout)
```

- [ ] **Step 2: Run RED**

Run: `python -m unittest tests.test_query_methods.ReportTests -v`  
Expected: absent report writer or routing failure.

- [ ] **Step 3: Implement reports and routing**

```python
def atomic_write_text(path: Path, content: str) -> None:
    temp = path.with_suffix(path.suffix + ".tmp")
    temp.parent.mkdir(parents=True, exist_ok=True)
    temp.write_text(content, encoding="utf-8")
    temp.replace(path)
```

Write `manifest.json`, a Markdown summary table, and each `metadata.json` after `workspace_path` validation. Prefix non-subcommand argv with `search`; keep every old search argument unchanged. `extract` requires `--target` or `--targets`, resolves metadata and writes a report.

- [ ] **Step 4: Run GREEN and commit**

Run: `python -m unittest tests.test_query_methods -v; python query_methods.py --help; python query_methods.py extract --help`  
Expected: PASS, then both help commands exit 0.

```powershell
git add -- query_methods.py tests/test_query_methods.py
git commit -m "feat: add config-driven method reports"
```

## Task 3: Add bounded targeted Ghidra extraction

**Files:**
- Modify: `query_methods.py`
- Modify: `tests/test_query_methods.py`

**Interfaces produced:**

```python
@dataclass(frozen=True)
class GhidraSettings:
    ghidra_home: Path; game_assembly: Path; workspace: Path; cache_dir: Path
    timeout_seconds: int; decompile_timeout_seconds: int

def game_fingerprint(game_assembly: Path) -> str: ...
def extract_targeted(resolutions: Sequence[Resolution], settings: GhidraSettings, report_dir: Path) -> dict[str, Any]: ...
```

- [ ] **Step 1: Write the fake-headless RED test**

```python
def test_targeted_extraction_uses_noanalysis_and_writes_code(self) -> None:
    fake = self.make_fake_analyze_headless("write_response('int Apply() { return 1; }', '00000100 RET')")
    manifest = query_methods.extract_targeted([self.resolved_rva_100()], self.settings(fake), self.root / "report")
    self.assertIn("-noanalysis", manifest["command"])
    self.assertEqual("int Apply() { return 1; }", self.code_file().read_text())
```

- [ ] **Step 2: Run RED**

Run: `python -m unittest tests.test_query_methods.GhidraTests.test_targeted_extraction_uses_noanalysis_and_writes_code -v`  
Expected: missing settings/extractor failure.

- [ ] **Step 3: Implement cache, post-script, and command**

Hash the game in 1 MiB SHA-256 chunks. Validate `ghidra_home/support/analyzeHeadless.bat` and read-only binary existence. Generate a Jython post-script under `.cache/ghidra-method-tools/scripts` that uses `imageBase.add(rva)`, `getFunctionContaining`, targeted disassembly/function creation, `DecompInterface`, and response JSON fields `rva`, `status`, `pseudocode`, `disassembly`, `error`.

Invoke `analyzeHeadless.bat` via an argument list with `-import`, input binary, `-noanalysis`, `-postScript`, request JSON and response JSON. Use `subprocess.run(..., timeout=settings.timeout_seconds, text=True, capture_output=True)`. Write code artifacts atomically; preserve metadata on per-method errors.

- [ ] **Step 4: Write timeout RED test, implement, and run GREEN**

```python
def test_timeout_preserves_metadata(self) -> None:
    manifest = query_methods.extract_targeted([self.resolved_rva_100()], replace(self.settings(self.sleeping_fake()), timeout_seconds=1), self.root / "report")
    self.assertEqual("timeout", manifest["results"][0]["extraction_status"])
    self.assertTrue(self.metadata_file().is_file())
```

Run before implementation: `python -m unittest tests.test_query_methods.GhidraTests.test_timeout_preserves_metadata -v` (expected FAIL).  
Catch `TimeoutExpired`, create a timeout result, write report, return CLI exit code `7`, and never retry or trigger full analysis.  
Run after implementation: `python -m unittest tests.test_query_methods.GhidraTests -v; python -m py_compile query_methods.py` (expected PASS).

- [ ] **Step 5: Commit**

```powershell
git add -- query_methods.py tests/test_query_methods.py
git commit -m "feat: add targeted Ghidra extraction"
```

## Task 4: Add detached full-analysis jobs

**Files:**
- Modify: `query_methods.py`
- Modify: `tests/test_query_methods.py`

**Interfaces produced:** `start_full_analysis(settings)`, `read_job(cache_dir, fingerprint)`, `refresh_job_status(job)`, `cancel_job(cache_dir, fingerprint)` and CLI `prepare-full/status/cancel`.

- [ ] **Step 1: Write RED lifecycle tests**

```python
def test_prepare_returns_immediately_and_records_running_job(self) -> None:
    began = time.monotonic(); manifest = query_methods.start_full_analysis(self.settings(self.sleeping_fake()))
    self.assertLess(time.monotonic() - began, 2)
    self.assertEqual("running", json.loads(manifest.read_text())["status"])

def test_cancel_marks_job_cancelled(self) -> None:
    manifest = query_methods.start_full_analysis(self.settings(self.sleeping_fake()))
    job = json.loads(manifest.read_text())
    self.assertEqual("cancelled", query_methods.cancel_job(self.cache_dir, job["fingerprint"])["status"])
```

- [ ] **Step 2: Run RED**

Run: `python -m unittest tests.test_query_methods.JobTests -v`  
Expected: missing job API failure.

- [ ] **Step 3: Implement job lifecycle**

Create `.cache/ghidra-method-tools/jobs/<fingerprint>/lock.json` atomically using `os.O_EXCL`. Launch `Popen` with stdout/stderr both redirected to `analysis.log` and `CREATE_NO_WINDOW`; write manifest with PID, command, fingerprint, timestamps, status, exit code, and log path before returning. Reject a live lock using `JobConflictError` and exit `8`. Refresh dead PID into `ready` if exit zero, otherwise `failed`; mark unchanged-live logs `stale` after 15 minutes without killing them. `cancel` runs `taskkill /PID <pid> /T /F`, marks `cancelled`, then removes lock.

- [ ] **Step 4: Add conflict test, run GREEN, and commit**

```python
def test_live_lock_prevents_second_full_analysis(self) -> None:
    query_methods.start_full_analysis(self.settings(self.sleeping_fake()))
    with self.assertRaises(query_methods.JobConflictError): query_methods.start_full_analysis(self.settings(self.sleeping_fake()))
```

Run: `python -m unittest tests.test_query_methods.JobTests -v; python query_methods.py prepare-full --help; python query_methods.py status --help`  
Expected: PASS and help exits 0 without spawning Ghidra.

```powershell
git add -- query_methods.py tests/test_query_methods.py
git commit -m "feat: manage background Ghidra analysis"
```

## Task 5: Move header generation to the shared resolver

**Files:**
- Modify: `gen_method_fallback.py:1-346`
- Create: `tests/test_gen_method_fallback.py`

**Interfaces produced:** `render_header(rows)` and `--target`, `--targets`, `--extract-code` generator CLI options.

- [ ] **Step 1: Write RED tests**

```python
def test_targets_file_writes_resolved_rva(self) -> None:
    completed = self.run_generator("--json", str(self.map_path), "--targets", str(self.targets_path), "--output", str(self.output))
    self.assertEqual(0, completed.returncode); self.assertIn("0x100", self.output.read_text())

def test_ambiguous_target_has_no_arbitrary_rva(self) -> None:
    completed = self.run_generator("--json", str(self.map_path), "--target", "Game.Unit::Apply", "--output", str(self.output))
    self.assertEqual(5, completed.returncode); self.assertNotIn("0x100", self.output.read_text())
```

- [ ] **Step 2: Run RED**

Run: `python -m unittest tests.test_gen_method_fallback -v`  
Expected: unknown options or arbitrary first candidate behavior.

- [ ] **Step 3: Implement target adapter and safe rendering**

Map each legacy `DESIRED` tuple into `MethodTarget(type_name=d[0], method_name=d[1], assembly=d[2], namespace_override=d[3], argc=d[4], signature_contains=d[5] if len(d) > 5 else None)`. Use config/CLI targets when supplied; otherwise use this adapter. Only render `resolved` rows, sort numeric RVA, validate output with `workspace_path`, and atomically write the unchanged C structure. Return `4` for missing and `5` for ambiguous targets. `--extract-code targeted` delegates to `extract_targeted`; `full` requires a ready job and never starts one.

- [ ] **Step 4: Run GREEN and commit**

Run: `python -m unittest tests.test_gen_method_fallback -v; python gen_method_fallback.py --help; python -m py_compile gen_method_fallback.py query_methods.py`  
Expected: PASS and all commands exit 0.

```powershell
git add -- gen_method_fallback.py tests/test_gen_method_fallback.py
git commit -m "feat: share resolver with fallback generator"
```

## Task 6: Add manual guide and final verification

**Files:**
- Create: `GHIDRA_METHOD_EXTRACTION_GUIDE.md`
- Modify: `tests/test_query_methods.py`

- [ ] **Step 1: Write documentation RED test**

```python
def test_guide_covers_targeted_and_manual_recovery(self) -> None:
    guide = (ROOT / "GHIDRA_METHOD_EXTRACTION_GUIDE.md").read_text(encoding="utf-8")
    for text in ["image base", "RVA", "-noanalysis", "Decompiler", "shared RVA", "prepare-full", "status --follow"]:
        self.assertIn(text, guide)
```

- [ ] **Step 2: Run RED**

Run: `python -m unittest tests.test_query_methods.DocumentationTests -v`  
Expected: file-not-found failure.

- [ ] **Step 3: Write Vietnamese operator guide**

Include sections `Chuẩn bị`, `metadata-only`, `Targeted extraction nhanh (-noanalysis)`, `Mở thủ công target RVA`, `Create Function và Decompiler`, `overload/thunk/stub/shared RVA`, `Full analysis nền`, `status --follow và cancel`, `cache`, and `Chẩn đoán lỗi`. Give exact PowerShell invocations plus detailed GUI actions: calculate `image base + RVA`, press `G`, use Disassemble, Create Function, open Decompiler, correct signature, and export pseudocode/disassembly. State that full analysis is explicit and may take a long time.

- [ ] **Step 4: Run GREEN and the entire suite**

Run:

```powershell
python -m unittest discover -s tests -v
python -m py_compile query_methods.py gen_method_fallback.py
python query_methods.py --help
python gen_method_fallback.py --help
git diff --check
git status --short -- .
```

Expected: tests, compilation, help, and whitespace checks succeed; status contains only intended `tools` changes.

- [ ] **Step 5: Commit**

```powershell
git add -- GHIDRA_METHOD_EXTRACTION_GUIDE.md tests/test_query_methods.py tests/test_gen_method_fallback.py
git commit -m "docs: add Ghidra extraction guide"
```
