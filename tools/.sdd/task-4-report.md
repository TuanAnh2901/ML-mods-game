# Task 4 report

## Status
Implemented detached full-analysis lifecycle in `query_methods.py`.

## Delivered
- `start_full_analysis`, `read_job`, `refresh_job_status`, `cancel_job`, and `JobConflictError`.
- Atomic per-fingerprint `lock.json`, persisted manifest/log/exit-code paths, PID tracking, stale status, conflict handling, and forced cancellation.
- CLI subcommands: `prepare-full`, `status`, and `cancel`.
- Lifecycle tests using the fake sleeping headless process.

## Verification
- `python -m unittest tests.test_query_methods -v` — 21 passed.
- `python query_methods.py prepare-full --help` — exit 0.
- `python query_methods.py status --help` — exit 0.
- `git diff --check` — clean.

## Concern
Full analysis was not run against a real Ghidra installation, as required by the task boundary; behavior is covered with the fake headless process.