# Task 3 report

## Scope
Implemented bounded, targeted Ghidra extraction in `query_methods.py` without invoking a real Ghidra installation or reading backup directories.

## TDD evidence
1. Added `GhidraTests.test_targeted_extraction_uses_noanalysis_and_writes_code` using a generated fake `analyzeHeadless.bat`.
2. RED command:
   `python -m unittest tests.test_query_methods.GhidraTests.test_targeted_extraction_uses_noanalysis_and_writes_code -v`
   failed as expected with `AttributeError: module 'query_methods' has no attribute 'extract_targeted'`.
3. Implemented the settings API, 1 MiB chunked SHA-256 fingerprinting, bounded no-analysis headless invocation, generated Jython post-script, atomic artifacts, and timeout metadata retention.
4. GREEN commands:
   - `python -m unittest tests.test_query_methods.GhidraTests.test_targeted_extraction_uses_noanalysis_and_writes_code -v` — 1 passed.
   - `python -m unittest tests.test_query_methods.GhidraTests.test_timeout_preserves_metadata -v` — 1 passed in 1.037s.
   - `python -m unittest tests.test_query_methods -v` — 15 passed in 1.573s.
   - `python -m py_compile query_methods.py` — passed.
   - `git diff --check` — passed.

## Self-review
- Headless command is a list, includes `-import`, `-noanalysis`, and `-postScript`; it uses `subprocess.run` with captured text output and the configured timeout.
- Only resolved, RVA-bearing entries are sent to the script. Resolution metadata remains present for errors and timeouts; timeout yields manifest exit code 7.
- Script and report writes use `atomic_write_text`; a stale response file is removed before each run.
- Fake tests cover extraction command/artifact output and timeout metadata, and do not start real Ghidra.

## Commit
`0e5239a feat: add targeted Ghidra extraction`.

## Concerns
The generated Jython post-script was validated structurally through fake-headless tests only, as required; it was not executed against a real Ghidra instance.
## Follow-up: backup-path validation
- Added preflight rejection for `before` and `tools before` path components in `ghidra_home`, `game_assembly`, `workspace`, `cache_dir`, and `report_dir`, before executable/binary validation, hashing, or subprocess invocation.
- RED: backup-game and workspace tests initially failed with the old missing-file / workspace-containment errors instead of the backup-directory error.
- GREEN: `python -m unittest tests.test_query_methods.GhidraTests -v` — 5 passed; full `tests.test_query_methods` — 18 passed; `py_compile` and `git diff --check` passed.
- Added fake-headless tests for backup game input, backup Ghidra home, and a workspace named `Before`.
