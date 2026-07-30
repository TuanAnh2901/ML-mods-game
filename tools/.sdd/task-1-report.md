# Task 1 Report — Models, Resolver, and Output-Path Guard

## Changes

- Added immutable `MethodEntry`, `MethodTarget`, and `Resolution` models.
- Added pure helpers for RVA parsing, signature parameter counting, target parsing,
  JSON metadata/target loading, method resolution, workspace guarding, and legacy
  header-output path handling.
- Preserved the existing query CLI while routing its JSON reading through the
  validated method-entry loader.
- Added resolver, loader, target-parser, and path-guard tests.

## TDD evidence

1. Created `tests/test_query_methods.py` before implementing the requested API.
2. Ran `python -m unittest tests.test_query_methods.ResolverTests -v`.
   The initial command exposed a test-package import issue; after adding the
   package marker, it failed as expected with `AttributeError` for the missing
   `query_methods.parse_rva` API.
3. Implemented the minimal models and helper functions, then ran the requested
   resolver test group and the complete test module.

## Verification

Commands run successfully:

```powershell
python -m unittest tests.test_query_methods -v
python -m py_compile query_methods.py
python query_methods.py --help
```

Result: 8 tests passed; compilation and CLI help both exited with code 0.

## Self-review

- Resolver first checks exact type/method matches, then partial type matches;
  signature and arity filters are applied before ambiguity handling.
- Resolved entries report the count of all entries sharing a positive RVA.
- Metadata rows without both names are skipped with index-specific warnings.
- Target metadata accepts root arrays and `{ "targets": [...] }`, and rejects
  invalid or negative arity values.
- `workspace_path` resolves paths, requires workspace containment, and rejects
  `before` and `tools before` path components. `header_output_path` additionally
  admits only the fixed legacy fallback header outside the workspace.

## Commits

- `e3d58aeba3e25cb6b84b56970f4769dd572473df` — `feat: add method target resolver`

## Review fixes

- Restored `DEFAULT_JSON` to the original `Everlusting Life` method-map path and
  added a regression test that directly verifies the default path.
- Validated JSON `signature_contains`: it must be a string or `null`; other
  values now raise a clear `ValueError` during target loading rather than later
  failing in resolver string operations.
- Added both tests first and observed two expected failures before applying the
  minimal fixes.

Fresh verification after the fixes:

```powershell
python -m unittest tests.test_query_methods -v
python -m py_compile query_methods.py
```

Result: 10 tests passed; compilation exited with code 0.
