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

Use `Path.resolve()` plus `relative_to(workspace.resolve())` in `workspace_path`; reject `before` and `tools before` components. Add `header_output_path` which accepts either a workspace-local path or the fixed legacy default `workspace.parent / "el_native" / "method_fallback.inc"` and rejects every other external path.

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

