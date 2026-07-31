# Battle Cheat Extraction Review

Reviewed source: D:\Temp\opencode\battle_cheat.exe_extracted (static inspection only).

## What was retained

- `il2cpp_resolve.js` provided a metadata-backed field lookup pattern: enumerate fields and walk base classes. `el_native/il2cpp_resolve` now exposes `ResolveFieldOffset` with the same parent-class traversal and a `-1` unavailable result.
- `battle_cheat.js` confirmed that combat identity belongs to `BattleUnit::Data` / `HandMonster`; Combat Runtime now resolves `get_Data`, then reads `HandMonster::get_monsterId` and `GetName` instead of relying on `BattleUnit::ToString`.
- `reference/battle_cheat_monster_ids.json` is a byte-for-byte external reference catalog. SHA-256: A1F620D861A5F278C14CCC93DBAC5E872F0C8510B155D04E561970688CBA0C91. It is documentation-only; live identity still comes from game metadata.

## Reviewed but not integrated

- Direct request-state changes, device/header changes, detector hooks, and UI automation.
- Hard-coded side offset fallback (`144`); this project resolves the `side` field by metadata only when the `get_ArmySide` accessor is unavailable.
- Direct reward, battle-stat, or inventory mutations.

The extracted `mascot_unlock.js` and `auto_battle.js` flows were re-checked
against the current method map. Their safe, session-local parts are now
integrated in the native build:

- `CheatsWindow` inherited `WindowScriptCore<T>` methods have concrete alias
  fallbacks for `.cctor`, `LoadWindow`, `get_instance`, and `Show`.
- Mascot unlock uses `ItemModule.get_Instance` + `get_MascotCollection`,
  expands the managed string list only while `MascotWindowPresenter` is open,
  and restores the original list from `MascotWindow.OnHidden`. When metadata
  exposes the extracted `PnkClient.blockRequests` and
  `ServerUpdateHandler._hasRequestSent` fields, the native feature also clears
  those transient 409 guards while the session toggle is active so selecting a
  session-only mascot does not stall later profile/quest requests.
- Automation now resolves `MainWindowPlayButton`, result-window advance paths,
  and the extracted three-phase Derank button dispatcher. Derank remains
  gated until metadata field offsets (`settingsButton`, `surrenderButton`,
  `leftButton`) resolve; result advancement also uses the extracted
  `MatchCompletedWindow._onButtonAction` delegate when available. Field lookup
  mirrors the companion's class-name search if a namespace changed; no stale
  numeric offsets were copied.
- Force-win coverage now includes the Tournament/session path: local winner
  selection through `LocalServerEmulator.BattleResult`, outgoing
  `ChessSocketsController` BattleResult/BattleCalculateResult requests, and
  the local `ServerParticipant.UpdateStreaks` result. `UserData.NameModule`
  supplies the live profile ID instead of relying only on the phase-controller
  constructor.

The session-local mascot path exposes the added IDs to the normal mascot UI so
the user can select and use them during that window session. It does not write
account inventory or claim server-side ownership.

## Result

Entity Manager has a concrete metadata path for board units: `BattleUnit -> Data -> HandMonster -> monsterId/name`. This removes the previous generic `BattleUnit` display identity when the game exposes the normal data provider.
