# Card Roulette Security QA Audit

## Scope and Evidence

Static review used recovered client sources under the approved development build:

- `retoolkit-output/cpp2il-cs/DiffableCs/Assembly-CSharp/Autochess/CardRouletteRequestHandler.cs`
- `retoolkit-output/cpp2il-cs/DiffableCs/Assembly-CSharp/CardRoulette/Elements/CardRouletteDeckController.cs`
- `retoolkit-output/cpp2il-cs/DiffableCs/Assembly-CSharp/CardRoulette/CardRouletteModel.cs`
- `retoolkit-output/cpp2il-cs/DiffableCs/Assembly-CSharp/CardRoulette/Elements/CardDeckModel.cs`

Runtime evidence is recorded in `artifacts/bunny_royale_trace/frida.jsonl`.

## Final Conclusion

No client-side per-card BlackMark location was found. The client sends a spin
request for a roulette step, not for one of four visual cards.
`CardRouletteSpinDataRequest` contains only `roulette_id` and `step_id`. It
has no card index, card ID, deck order, seed, or BlackMark location.

The spin response has `result.rewards` and one `result.is_black_mark` boolean.
It does not expose a four-card result map. `CardRouletteDeckController` uses
`OnCardClick(int index)` and `OnRewardOpened(..., int index)` only in the
local presentation path. `BlackMarkState` is a lifecycle enum: `No`,
`Generated`, `Drawn`, `PaidOff`; it is not a card index.

The evidence supports a server-authoritative step outcome. There is no static
evidence that the client receives enough data to identify BlackMark among the
three unchosen visual cards before a spin response arrives.

## QA-TB-CR-001: Card Index Is Outside the Spin RPC Boundary

**Condition:** A roulette step shows four visual cards.

**Minimum reproduction:**

1. Start a staging roulette step.
2. Perform four reset runs, selecting each card visual once.
3. Capture and compare the serialized spin requests.

**Expected:** If visual card choice is authoritative, the request contains an
authenticated card token or card index that the server validates against the
current server-side deck.

**Observed:** `OnCardClick(int index)` is local. The request boundary is
`LoadCardRouletteSpinInfo(int rouletteId, int stepId, ...)`, and its request
model has only `roulette_id` and `step_id`.

**Impact:** If card choice is intended to affect the outcome, the UI can imply
a meaningful choice while the server resolves only a step. No evidence shows
that an unchosen-card BlackMark position is transmitted to the client.

**Evidence:**

- `CardRouletteRequestHandler.CardRouletteSpinDataRequest`: `roulette_id`,
  `step_id`.
- `CardRouletteRequestHandler.ServerSpinInfo`: `rewards`, `is_black_mark`.
- `CardRouletteDeckController.OnCardClick(int index)`.
- `CardRouletteDeckController.SendApplySpinReward(int rouletteId, int stepId,
  IReward reward = null)`.

**Remediation:** Decide the product contract. For visual-only cards, make the
server-resolved step explicit in the UI. For authoritative selection, send an
opaque server-issued card token; validate its ownership, event, step, expiry,
and single-use state server-side in the same atomic outcome transaction.

**Regression tests:**

- Wire-contract test requiring one valid card token if selection is intended
  to matter.
- Server tests for foreign, expired, duplicate, and out-of-range selections.
- UI test proving the returned outcome applies only to the selected token.

## QA-TB-CR-002: Spin Response Is the Outcome Boundary

**Condition:** The client receives a successful spin response.

**Minimum reproduction:**

1. Capture a valid staging response.
2. Confirm `OnSpinResponseReceived` occurs before reward parsing and display.
3. Confirm `UpdateBlackMarkState` and visual transitions follow the response.

**Expected:** The server validates ownership and current step, computes one
outcome atomically, and returns a response correlated to the initiating request.

**Observed:** `CardRouletteModel.OnSpinResponseReceived` consumes
`CardRouletteSpinInfoResponse`; `ServerSpinInfo` supplies rewards plus
`is_black_mark`. `CardDeckModel` holds a chance and lifecycle state, but not a
per-card location map.

**Impact:** The current client protocol does not disclose a future BlackMark
location. Remaining risk is server-side validation of session, event, step,
idempotency, and reward persistence.

**Remediation:** Enforce authenticated ownership, event/step state checks,
idempotency keys, and one atomic state transition. Bind response correlation
to the active request and persist outcome before delivery.

**Regression tests:**

- Valid request produces one reward/progress transition.
- Stale, foreign-session, and replayed requests mutate no state.
- A test transport fixture with mismatched correlation data is discarded by
  the client rather than applied to the active UI state.

## Staging Test Matrix

| Test | Input | Expected result |
| --- | --- | --- |
| Valid step | Active event and current step | One persisted outcome and one UI transition |
| Stale step | Previously completed step | Domain error and no mutation |
| Invalid step | Negative or greater than configured maximum | Validation error and no mutation |
| Replay | Repeat the same request correlation | Idempotent result or rejection; no duplicate reward |
| Session isolation | Step ID from another staging account | Ownership error and no disclosure |
| Card-token validation | Invalid or expired token after selection becomes authoritative | Validation error and no mutation |

## Controlled Response-Integrity Harness

`tools/bunny_roulette_staging_integrity.py` is a loopback-only mitmproxy addon
for the approved staging service. It records original and modified SHA-256
values to `artifacts/bunny_royale_trace/staging_integrity.jsonl`. The runner
rejects a non-loopback `-StagingHost` when a mutation profile is selected.

Use a baseline control first:

```powershell
powershell -ExecutionPolicy Bypass -File D:\VSCode\EL_Native\tools\run_bunny_royale_trace.ps1 -Seconds 180 -StagingHost localhost -StagingIntegrityProfile observe
```

Then run one isolated staging test profile per reset session:

```powershell
powershell -ExecutionPolicy Bypass -File D:\VSCode\EL_Native\tools\run_bunny_royale_trace.ps1 -Seconds 180 -StagingHost localhost -StagingIntegrityProfile flip_black_mark
powershell -ExecutionPolicy Bypass -File D:\VSCode\EL_Native\tools\run_bunny_royale_trace.ps1 -Seconds 180 -StagingHost localhost -StagingIntegrityProfile increment_first_reward
powershell -ExecutionPolicy Bypass -File D:\VSCode\EL_Native\tools\run_bunny_royale_trace.ps1 -Seconds 180 -StagingHost localhost -StagingIntegrityProfile malformed_result
```

For each run, retain the trace report, client result, and server audit record.
The relevant pass condition is that an altered client response does not produce
a mismatched persisted reward or progress state after server reconciliation.
