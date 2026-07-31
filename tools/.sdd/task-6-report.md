# Task 6 report

## Delivered

- Added `GHIDRA_METHOD_EXTRACTION_GUIDE.md`, a Vietnamese operator guide covering metadata-only lookup, targeted `-noanalysis` extraction, manual RVA recovery in Ghidra, overload/thunk/stub/shared-RVA handling, explicit background full analysis, cache handling, cancellation, and troubleshooting.
- Added `DocumentationTests.test_guide_covers_targeted_and_manual_recovery` to preserve the required guide contract.

## Verification

- RED: `python -m unittest tests.test_query_methods.DocumentationTests -v` failed as expected because the guide did not exist.
- GREEN: the documentation test passed after the guide was added.
- `python -m unittest discover -s tests -v` — 32 tests passed.
- `python -m py_compile query_methods.py gen_method_fallback.py` — passed.
- `python query_methods.py --help` — passed.
- `python gen_method_fallback.py --help` — passed.
- `git diff --check` — passed.
