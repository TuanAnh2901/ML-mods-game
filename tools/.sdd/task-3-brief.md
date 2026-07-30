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

