# Task 2 Report — Reports and CLI Routing

## Changes

- Added atomic UTF-8 report writes, stable per-method path slugs, report manifest output, Markdown summaries, and individual `metadata.json` files.
- Added `search` and metadata-only `extract` subcommands. Legacy argument invocations are normalized to `search`, preserving existing search flags and output behavior.
- `extract` accepts exactly one source of targets (`--target` or `--targets`), resolves them with the existing public resolver APIs, validates its report destination using `workspace_path`, and writes the report.
- Added report layout and legacy keyword CLI regression tests.

## TDD Evidence

1. RED: `python -m unittest tests.test_query_methods.ReportTests -v`
   - Initially failed as expected with `AttributeError: module 'query_methods' has no attribute 'write_report'`.
2. GREEN: reran the focused tests after the implementation; both tests passed.

## Verification Commands and Results

- `python -m unittest tests.test_query_methods -v` — 12 tests passed.
- `python query_methods.py --help` — exited 0 and listed `search` and `extract`.
- `python query_methods.py extract --help` — exited 0 and listed extract options.
- Extract smoke test using a temporary map/workspace — passed; verified `manifest.json`, `summary.md`, and `methods/Game.Unit__Apply__0x100/metadata.json`.
- `git diff --check` — no whitespace errors.

## Self-review

- Kept the confirmed resolver and search matching behavior intact; search logic was moved into a helper without changing filters or output formatting.
- Relative report paths are resolved under the explicitly selected workspace before validation, preventing external or backup-tree report writes.
- Report writes use a sibling temporary file and replacement, and all report files use UTF-8.
- Scope is limited to Task 2 implementation, tests, and this report.

## Commit

`feat: add config-driven method reports`

## P1 Follow-up: Empty Legacy CLI

- Added a regression test proving that an empty legacy invocation exits with code 2 and retains the selector error text.
- RED evidence: the new test initially failed because the root CLI printed help and exited 0.
- Updated argv normalization so an empty argument vector routes to `search`; `search` then emits its established selector error.
- Verification: `python -m unittest tests.test_query_methods.ReportTests -v` passed (3 tests); `python -m unittest tests.test_query_methods -v` passed (13 tests); `git diff --check` passed.
