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
