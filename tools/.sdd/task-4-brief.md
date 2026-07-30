## Task 4: Add detached full-analysis jobs

**Files:**
- Modify: `query_methods.py`
- Modify: `tests/test_query_methods.py`

**Interfaces produced:** `start_full_analysis(settings)`, `read_job(cache_dir, fingerprint)`, `refresh_job_status(job)`, `cancel_job(cache_dir, fingerprint)` and CLI `prepare-full/status/cancel`.

- [ ] **Step 1: Write RED lifecycle tests**

```python
def test_prepare_returns_immediately_and_records_running_job(self) -> None:
    began = time.monotonic(); manifest = query_methods.start_full_analysis(self.settings(self.sleeping_fake()))
    self.assertLess(time.monotonic() - began, 2)
    self.assertEqual("running", json.loads(manifest.read_text())["status"])

def test_cancel_marks_job_cancelled(self) -> None:
    manifest = query_methods.start_full_analysis(self.settings(self.sleeping_fake()))
    job = json.loads(manifest.read_text())
    self.assertEqual("cancelled", query_methods.cancel_job(self.cache_dir, job["fingerprint"])["status"])
```

- [ ] **Step 2: Run RED**

Run: `python -m unittest tests.test_query_methods.JobTests -v`  
Expected: missing job API failure.

- [ ] **Step 3: Implement job lifecycle**

Create `.cache/ghidra-method-tools/jobs/<fingerprint>/lock.json` atomically using `os.O_EXCL`. Launch `Popen` with stdout/stderr both redirected to `analysis.log` and `CREATE_NO_WINDOW`; write manifest with PID, command, fingerprint, timestamps, status, exit code, and log path before returning. Reject a live lock using `JobConflictError` and exit `8`. Refresh dead PID into `ready` if exit zero, otherwise `failed`; mark unchanged-live logs `stale` after 15 minutes without killing them. `cancel` runs `taskkill /PID <pid> /T /F`, marks `cancelled`, then removes lock.

- [ ] **Step 4: Add conflict test, run GREEN, and commit**

```python
def test_live_lock_prevents_second_full_analysis(self) -> None:
    query_methods.start_full_analysis(self.settings(self.sleeping_fake()))
    with self.assertRaises(query_methods.JobConflictError): query_methods.start_full_analysis(self.settings(self.sleeping_fake()))
```

Run: `python -m unittest tests.test_query_methods.JobTests -v; python query_methods.py prepare-full --help; python query_methods.py status --help`  
Expected: PASS and help exits 0 without spawning Ghidra.

```powershell
git add -- query_methods.py tests/test_query_methods.py
git commit -m "feat: manage background Ghidra analysis"
```

