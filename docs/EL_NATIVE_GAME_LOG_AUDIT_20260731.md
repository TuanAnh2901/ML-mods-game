# Game-folder log audit (2026-07-31)

Scope: root-level `*.log`, `*.txt`, `*.json`, `*.jsonl`, and `*.ini` in
`D:\SteamLibrary\steamapps\common\Everlusting Life` (14 files; no recursive
analysis/source folders).

## Confirmed runtime state

- `el_native.log` records `CombatRuntime` snapshots of 10 → 9 → 8 → 7 → 6
  units, followed by `FieldController.BattleEnded snapshot cleared`.
- `el_native_stat_compare.txt` shows player turn intervals changing while
  enemy intervals remain unchanged.
- `el_native_combat_compare.txt` shows player offense/heal deltas and enemy
  baseline values preserved.
- No `700`, `802`, disconnect, or new endpoint/payload marker was found in the
  scanned root logs.
- `el_native_entity_snapshot.txt` and `el_native_monsters.txt` are empty in
  this capture because the export buttons were not used during that run; this
  does not indicate an empty board (the runtime snapshot lines contain units).

## Issues corrected from the logs

- Older runs resolved Automation with empty `MatchCompletedWindow` namespace
  and therefore logged `show=0/update=0`. The current fallback map uses the
  verified namespace and RVAs and installs original-preserving observers.
- Older runs logged `ItemModule.get_MascotCollection` unresolved. The current
  map contains the verified `0xDF43B0` RVA and the UI performs a guarded,
  read-only preview through `get_Instance`.
- `SpeedHackProofTime.OnSpeedHackDetected` was a speculative compatibility
  target that retried noisily without a verified method entry; it was removed
  from the target list.
- Resolver initialization now retries an incomplete IL2CPP export chain instead
  of permanently caching an early null export set. This is important for
  `CheatsWindow` inherited methods and metadata field offsets.
- Resolve Method output is now isolated in the **Debug** tab. Relationships
  have their own **Relationships** tab, and profile controls are rendered only
  in **Profiles**.

## Current extracted-feature port

- `CheatsWindow` aliases are present for the four inherited generic methods.
- Mascot session unlock hooks `InitMascotButtons` and `OnHidden`; the managed
  collection is expanded from the mascot dictionary and restored on close.
  It also attempts metadata-based clearing of the two transient request guards
  identified in `mascot_unlock.js`; fresh runs report these as
  `[MASCOT] request guards ... offsets=...`.
- Automation has real main-thread Play/result dispatch. Derank's three-phase
  settings/surrender/confirmation path is gated by live metadata field offsets
  and uses the extracted `_onButtonAction` result delegate before the fallback
  result methods. Field resolution now retries by class name when namespace
  metadata differs from the companion.
- Force-win now logs separate converter, local-emulator, and tournament-streak
  hook paths. Tournament loss reports from older captures predate this
  `ServerParticipant.UpdateStreaks` coverage.

## Event trace notes

Roulette observer entries in prior logs use HTTP 200 responses and resolve both
CardRoulette and ChooseReward paths. The trace remains read-only: a bad card is
marked only when the server payload contains an explicit bad token; a bare
black-mark boolean remains `unknown`.
