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

Map each legacy `DESIRED` tuple into `MethodTarget(type_name=d[0], method_name=d[1], assembly=d[2], namespace_override=d[3], argc=d[4], signature_contains=d[5] if len(d) > 5 else None)`. Use config/CLI targets when supplied; otherwise use this adapter. Only render `resolved` rows, sort numeric RVA, validate output with `header_output_path`, and atomically write the unchanged C structure. Return `4` for missing and `5` for ambiguous targets. `--extract-code targeted` delegates to `extract_targeted`; `full` requires a ready job and never starts one.

- [ ] **Step 4: Run GREEN and commit**

Run: `python -m unittest tests.test_gen_method_fallback -v; python gen_method_fallback.py --help; python -m py_compile gen_method_fallback.py query_methods.py`  
Expected: PASS and all commands exit 0.

```powershell
git add -- gen_method_fallback.py tests/test_gen_method_fallback.py
git commit -m "feat: share resolver with fallback generator"
```

