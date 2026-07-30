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

