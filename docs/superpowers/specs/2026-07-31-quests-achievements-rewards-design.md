# Quests, Achievements, and Rewards Design

## Goal

Extend the existing quest feature into a single `Quests & Achievements` surface
with independently controlled Daily, Weekly, Achievement, and reward-audit
controllers. Remove the currently inert reward multiplier UI state.

## Architecture

`quest.cpp` remains the feature entry point and owns a small set of controllers:

- `QuestController` resolves `QuestSaveData`, `QuestRefreshTimer`,
  `QuestRefreshHandler`, and `QuestChestsController`; it gates Daily and Weekly
  behavior by `QuestMetatype`.
- `AchievementController` resolves and observes achievement data through
  `IQuestModule`, `PlayerInfoModule`, notification UI, and Steam achievement
  helper targets.
- `RewardController` owns display and local-state multipliers independently.
- `RewardAudit` is a bounded ring buffer of base/display/local reward values,
  request/result events, and response codes.

Controllers share method resolution and availability reporting but never share
enable flags. A missing target disables only the dependent controller.

## Behavior

- Daily and Weekly sections show their own availability, toggles, refresh time,
  progress, chest state, and shared-RVA warnings.
- Achievement section reports chosen achievements, total count, progress, UI
  notification events, and Steam-state update events.
- Display multiplier changes only presentation/audit values.
- Local-state multiplier changes only the local reward value represented by the
  feature after a collect event. The original request/result path remains
  observable through the audit record.
- A response code outside the controller's configured success set records the
  event and disables only the reward controller that produced it.
- The current multiplier slider is disabled until its required reward target is
  resolved; it is never presented as active while inert.

## Safety and validation

- Every target uses type, method, arity, and signature metadata.
- Shared RVA targets are surfaced in UI/audit and are not enabled until their
  type-specific resolver result is available.
- `ObscuredBool` and `ObscuredInt` packing helpers are centralized and covered
  by tests before use.
- Audit storage is a fixed-capacity ring buffer; exporting is explicit and does
  not mutate game state.
- Target resolution, dependency availability, and controller enablement are
  recorded in diagnostic logs.

## Tests

- Encode/decode tests for `ObscuredBool` and `ObscuredInt`.
- Quest-metatype classification tests for Daily and Weekly.
- Display/local multiplier calculation tests, including `1.0x` and bounds.
- Audit ring-buffer overflow and response-code disable tests.
- Missing/shared target tests proving that dependent controllers remain off.
- Build and existing project tests after implementation.

## Acceptance criteria

- Daily, Weekly, Achievement, and reward sections are separately visible and
  controllable.
- No UI control has no implementation path.
- Reward audit exposes base/display/local values and result code per event.
- Missing or shared targets fail closed at controller scope, not feature scope.
- Existing QuestSaveData behavior remains available through the new controller.
