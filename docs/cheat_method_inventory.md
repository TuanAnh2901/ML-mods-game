# CheatsWindow Method Inventory — Full 246-Entry Audit

**Generated:** 2026-08-11
**Sources:**
- `Analysis\Combined-20260806\current-update\ghidra_cheat_cheatwindow_decomp.txt` — Ghidra decomp of 186 CheatsWindow-region functions (Mfuscator-obfuscated IL2CPP)
- `D:\Temp\opencode\cheat_entries.tsv` — 246 method entries (class/method/sig/RVA) extracted from the game's metadata
- `el_proxy.json` + EL_Native injector — exploitation surfaces

**Method:** each TSV entry mapped to its decomp body (224/246 resolved; 22 listed separately). Every entry classified: what the native body actually does, in-game effect, player-facing benefit, and which exploit surface (if any) it exposes.

---

## 1. Critical finding: shared stub addresses (de-duplicate 100+ entries)

Mfuscator hollows many methods to one of a few **shared trampolines**. Multiple TSV entries point at the same RVA — hooking that RVA once affects all of them.

| Shared RVA | Decomp body | TSV entries aliased to it |
|---|---|---|
| `0x180596730` | Empty `return;` (Awake-named stub) | InGameConsoleHandler.Awake; DummyFieldElement.TurnAction / UpdateTime / ChangePosition / Dispose; DummyFieldUnit.ApplyShield / SetPriorityTarget / InvalidateDecision / StartBattle / RemoveSkill / UpKillsStreak / Transform / KillInstantly / ChangeHealingAmplification / ChangeOffenseAmplification / ChangeProtectionAmplification / ApplyMultiShot / SetDpsListener; CheatsPageButton.SetDataIndex |
| `0x1805A6470` | `return 1;` (hardcoded true) | DummyFieldElement.IsNotTimeFrozen / CanTakeTurns; DummyFieldUnit.CanBeTargeted / CanBeChosenByAttackTarget; CheatModuleAccount.CanUpdateUserData |
| `0x18060F980` | `return 0;` (null / none) | DummyFieldElement.get_ArmySide / get_FieldController; DummyFieldUnit.get_Animator / Cast / GetTargetUnit / AddSkill / GetKillsStreak / GetShield |
| `0x1805A6150` | `return *(this+0x18);` | CheatValue.get_Key; DummyFieldUnit.get_FieldController |
| `0x180C58270` | `return DAT_184886c20;` (global) | DummyFieldElement.GetTurnInterval; GetOffenseAmplify |
| `0x180596CC0` | Bare `Object..ctor` + return | InGameConsoleHandler.ctor; CheatsSectionButton.ctor; CheatPageBase.ctor; CheatsPageButton.ctor; CheatFoldoutElement.ctor; CheatsController.ctor; CheatsHeader.ctor; CheatsLeftButtonsView.ctor; CheatsPagesView.ctor; CheatsSectionButtonsView.ctor; CheatModule`1.ctor |
| `0x1805350E0` | Bare DisplayClass ctor | All `<c__DisplayClass*_0>.ctor` (5); `<c>.ctor`; CheatPage`1 (via 0x1806CE1A0 base); DummyFieldElement.ctor; CheatsFacade.ctor; CheatsWindowController.ctor |
| `0x180606890` | Bool stub (false) | DummyFieldUnit.HasStatus; CheatModuleTutorialAutoplay.get_AutoplayTutorial |
| `0x180655140` | `return 1;` | DummyFieldUnit.get_grade |
| `0x180DA7A40` | `return 100;` | DummyFieldUnit.GetMightValue |
| `0x180DA75B0/75F0/7EE0/7EA0/77C0` | Allocate Action, then `swi(3)` throw-stub | ApplySkillStatus / ApplyStatusImmune / RemoveSkillStatus×2 / ChangeCurrentStatValue |

**Exploit implication:** stub/hollow methods (`0x180596730`, `0x1805A6470`, `0x18060F980`, throw-stubs) carry **no logic to hook** — they are dead ends. Only entries with a real native body (below) are surfaces.

---

## 2. Real-logic methods — exploitable surfaces

| # | Class.method | RVA | Decomp body (what it does) | In-game effect / player benefit | Exploit surface |
|---|---|---|---|---|---|
| 1 | CheatModuleStory.ctor | `0xE2C600` | Static-init of metadata ptr + `if (*(ptr+0xe4)==0) workaround` | Base ctor | none (no logic) |
| 2/3 | CheatModuleInteractiveScene.**get/set_ForceEnableCensorship** | `0x5A5DD0` / `0x5A5E40` | Field **bool @ this+0x20** | Force-disable/enable content filter | **EL_Native hook** get/set → flip value; field offset `+0x20` for ResolveFieldOffset |
| 4 | CheatModuleInteractiveScene.get_ScriptableObjectsLoader | `0x719E30` | Real getter (loader resolve) | Asset access | minor (read) |
| 5 | CheatModuleInteractiveScene.ctor | `0x719DF0` | Static-init + workaround | — | none |
| 6 | CheatModuleDialogues.ctor | `0x7658B0` | Static-init + workaround | — | none |
| 7 | CheatPageBattleEffects.ctor | `0xA27880` | Static-init + workaround | — | none |
| 12 | GoTo.`<FiendGetResource>b__0(bool b)` | `0xDACE30` | Confirm-callback: resolves GoToController singleton (`FUN_181ad1c60(0x185b7e328)`), MethodInfo-dispatch `0x180002260`/`0x1800141d0`/`0x181c4fce0` → invokes `IGoToController` with `(resourceType, count)` | **Client-side window navigation** — opens GoTo borrow/bank window; no server route | hook → auto-confirm opens window |
| 14 | GoTo.`<FiendSpendResource>b__0(bool b)` | `0xDACEF0` | Same dispatch chain (spend flow), payload `(resourceType, count)` | **UI nav** to spend-page (bank/bundle) | hook → auto-confirm |
| 16 | GoTo.`<FiendGetReward>b__0(bool b)` | `0xDAD070` | Same chain; iterates IGoToController method slots (`0x185be7710`) | **UI nav** to reward page | hook → auto-confirm |
| 17 | GoTo.**get_GoToController** | `0xDA4410` | Long body: resolves GoToController singleton + parent chains, caches field `+0x40` | Controller for GoTo cheats | hook to force non-null controller |
| 18 | GoTo.**FiendGetResource** | `0xDA3D50` | Resolve controller → dispatch `IGoToController.GetResource(resourceType, count)` via `0x180002260`/`0x1800141d0`/`0x181c4fce0` | **Opens GoTo resource window** (bank/bundles) — NOT a server grant | hook: force window open; **no proxy rule possible** (no route) |
| 19 | GoTo.**FiendSpendResource** | `0xDA41E0` | Same chain, param `0` variant → spend flow | UI nav | hook |
| 20 | GoTo.**FiendGetReward** | `0xDA3F50` | Same chain, iterates vtable slots for `0x185be7710` | UI nav | hook + force rewardId |

**Resolution note (GoTo chain, 2026-08-11):** decompiled closures confirm all three Fiend* methods are **client-side `GoToController` window navigation** — they open the in-game GoTo/Bank/Bundles UI; the grant itself happens in the window's own callbacks. `GetReturnManager_GoTo_Request_GetResource` (RequestGetResource/RequestSpendResource/RequestGetReward) is **absent from the current method-pointer-map** (0 hits repo-wide) — stale from an earlier dump. Implication: **no server route to proxy**; these hooks only auto-open UI. Real resource grant surfaces remain the proxy (`resource_overrides`, roulette rewrite) + `GetStat`/`ChangeStatValue` hooks.
| 21/22/23/24 | GoTo/Relationships/MiniEventRanking/Lovedirect .ctor | `0xDA43D0/44B0/46E0/4A60` | Static-init + workaround | — | none |
| 25 | CheatSystemWindows.`<c>.cctor` | `0xDAD500` | Static singleton init | — | none |
| 27-29 | `<ShowTwoButtonWindow>b__1_0/1/2` | `0xDABBD0/C30/C90` | Button callbacks (confirm/cancel/close) | Window buttons | hook to force confirm |
| 30/31 | `<ShowPatchUpdateWindow>b__7_0/1` | `0xDABA30/BB00` | Patch-window callbacks | — | hook |
| 32/33 | `<ShowNutakuWindow>b__8_0/1` | `0xDAB950/9D0` | Nutaku-window callbacks (b__8_0 takes bool) | Nutaku link actions | hook |
| 34 | CheatSystemWindows.**ShowError(int errorCode)** | `0xDA5370` | **Largest body (480 lines):** resolves window fabric, layer-check, multi-branch error handling | Error popup — **directly driven by proxy `block_error_codes`** | proxy already blocks [700-703,801,802]; hook `ShowError` to swallow-all errors |
| 35 | CheatSystemWindows.**ShowTwoButtonWindow** | `0xDA5820` | Lazy-inits 3 window fields (+8/+0x10/+0x18) on fabric singleton, opens warning | 2-button confirm dialog | hook to auto-click confirm |
| 36 | ShowInfoWindow | `0xDA5410` | **no matching decomp section** (see §3) | Info popup | proxy-side suppression |
| 37 | ShowNotEnoughWindow(resource,needed) | `0xDA5420` | `neededCount`→string, `FUN_180985670(resource,..)` opens window | Resource-shortage popup | hook: bypass shortage gate |
| 38 | ShowReconnectWindow | `0xDA5810` | **no matching section** | Reconnect popup | proxy-side suppression |
| 39 | ShowChangeNameWindow | `0xDA5290` | Resolves controller (`0x185c12718`), MethodInfo-dispatch invoke | Name-change window | hook dispatch |
| 40 | ShowBankErrorWindow | `0xB7A0F0` | **no matching section** | Bank error popup | proxy-side suppression |
| 41 | ShowPatchUpdateWindow | `0xDA5670` | Lazy-init 2 fields, builds patch window (PIC/multi-call) | Patch-notes window | hook |
| 42 | ShowNutakuWindow | `0xDA54A0` | Lazy-init +0x30/+0x38 fields, opens Nutaku consent | Nutaku window | hook |
| 44 | CheatBool.ctor(key,default) | `0xDA3A10` | Init Sequence + key store (+0x18) + ReactiveProperty | Bool cheat value | field +0x18 (key), +0x10 (RP) |
| 45 | CheatBool.**IsEqual** | `0xDA3930` | `return stored ^ value ^ 1` = equality check | Value compare | none |
| 46 | CheatBool.**LoadFromPrefs** | `0xDA3980` | Guard + `FUN_18385cbb0` = **PlayerPrefs.GetInt** | Persists cheat state | hook to force toggled state |
| 47 | CheatBool.**SaveToPrefs** | `0xDA39D0` | **PlayerPrefs.SetInt** | Persist | hook-able |
| 48/49 | CheatValue`1.get/set_Key | `0x5A6150` / `0x5A5B90` | **field @ +0x18** read/write (set has write-barrier) | Key string | ResolveFieldOffset `+0x18` |
| 50 | CheatValue`1.get_Value | `0x2E0B750` | `*(byte*)(*(this+0x10)+0x10)` — read through **ReactiveProperty@+0x10** | Current cheat value | field `+0x10` → RP |
| 51 | CheatValue`1.get_ReactiveValue | `0x5CAFA0` | `return *(this+0x10)` | RP instance | field `+0x10` |
| 52 | CheatValue`1.Initialize(key,default) | `0x2E0B0B0` | RP setup + key bind | Init cheat value | hook |
| 53 | CheatValue`1.SetValue | `0x2E0B500` | Guard check, write default via RP `0x1A8` vtable | Set cheat value | hook to clamp/injection |
| 57 | CheatValue`1.op_Implicit | `0x2E0B870` | Read RP value (null-guarded) | Implicit cast | none |
| 58 | CheatValue`1.ctor | `0x2E0B620` | RP ctor + write barrier | — | none |
| 61 | CheatsHideUIButton.**Init(callback)** | `0xDA5AD0` | Long body: sets `+0x28` hidden flag from pref, callback wiring `+0x30` | Hide-UI toggle setup | hook → force show UI |
| 62 | CheatsHideUIButton.**Toggle(bool)** | `0xDA5BD0` | Writes `+0x28`, resolves locale strings, invokes callback (+0x30 vtable+0x18) | Show/hide all UI | hook Toggle(true) → permanent hide |
| 64 | `<Init>b__3_0` | `0xDA5C80` | Flips `+0x28`, callback with bool + localized label | Button onClick | hook |
| 65 | CheatsWindow.ctor | `0xDA64D0` | Inits `+0x33=1`, `+0x37=1` + static-init | Window default flags | fields +0x33/+0x37 |
| 68/69 | DummyFieldElement.IsNotTimeFrozen / CanTakeTurns | `0x5A6470` | `return 1` (time-frozen check bypassed) | Time never frozen in cheat field | none (already true) |
| 70 | DummyFieldElement.GetTurnInterval | `0xC58270` | `return DAT_184886c20` (global interval) | Turn pacing | could patch global |
| 71 | Element.get_ArmySide | `0x60F980` | `return 0` | Army side = 0 | none |
| 72 | Element.get_Position | `0xDA7430` | `FUN_1806b2570(&pos_struct,0,0,0)` = value-type ctor → zero position | Position | none (zero) |
| 79/80 | DummyFieldUnit.GetStat b__0/b__1 | `0xDACFB0` / `0xDAD010` | Stat-filter predicates (x match current/other stat) | Used by GetStat LINQ | hook → match-all |
| 81-83 | DummyFieldUnit.**.ctor** ×3 | `0xDA80A0` / `0xDA8160` / `0xDA7FD0` | Init RP@+0x10, **state@+0x90**, **position@+0x94** (variant 3 adds controller@+0x18, side@+0x20) | Dummy unit spawn | field offsets: `+0x10` RP, `+0x18` controller, `+0x20` side, `+0x90` state, `+0x94` position |
| 84 | DummyFieldUnit.get_FieldController | `0x5A6150` | `return *(this+0x18)` | Controller | field `+0x18` |
| 85 | DummyFieldUnit.get_ArmySide | `0x609190` | `return *(this+0x20)` (int) | Side enum | field `+0x20` |
| 86-111 | DummyFieldUnit event add_/remove_ (26×) | `0xDA8200…0xDA9380` | Delegate Combine/Remove — **13 delegate slots, 8-byte stride `+0x28`→`+0x88`** (see §4B struct map) | 13 Unity-style events (attack/status/shield/transform) | field `+0x28`…`+0x88`; hook one per event |
| 112 | DummyFieldUnit.**get_state** | `0x645320` | `return *(this+0x90)` | Unit state enum | **ResolveFieldOffset `+0x90`** |
| 113 | DummyFieldUnit.get_grade | `0x655140` | `return 1` | Grade hardcoded | patch → other grade |
| 114 | DummyFieldUnit.**GetStat(StatType)** | `0xDA7BE0` | Init 9 class refs; resolve stat via stat-provider chain; wraps `FUN_180e6ebe0`; adds to buff list `+0x1c`/`+0x18`; returns IUnitStat | Stat query | **core stat surface** — hook to return forged IUnitStat |
| 115 | DummyFieldUnit.**GetStatValue** | `0xDA7AB0` | GetStat + invoke + if stat ∈ {4..11,6} scale (`*100`-style) | Stat value (percent handling) | hook return value |
| 116 | DummyFieldUnit.**GetCurrentValue** | `0xDA79D0` | GetStat + current-value invoke | Current HP/Mana | hook |
| 117 | DummyFieldUnit.GetMightValue | `0xDA7A40` | `return 100` | Might = 100 | patch constant |
| 118 | DummyFieldUnit.**get_Position** | `0xDA8B40` | `return *(this+0x94)` | Position | field `+0x94` |
| 119 | DummyFieldUnit.**ApplyDamage** | `0xDA7460` | Guard: `damage<=0 || state==5 → return 0`; then stat-2 & stat-3 apply (armor/magic?) | Receive damage | hook → force return 0 = **immortal**; state==5 = dead |
| 120 | DummyFieldUnit.**Heal** | `0xDA7E00` | GetStat(2) + recover invoke | Heal | hook |
| 121 | DummyFieldUnit.**ChangeMana** | `0xDA7800` | GetStat(3) + mana delta | Mana regen | hook |
| 122 | DummyFieldUnit.**Move** | `0xDA7E70` | `*(this+0x94) = destination` (direct write) | Teleport unit | **direct field write `+0x94`** |
| 123 | DummyFieldUnit.**Attack** | `0xDA7630` | Resolve 3 classes, dispatch OnAttack on list@+0x28 | Attack | hook |
| 124 | DummyFieldUnit.OnAttackReceived | `0xDA7E80` | Invoke event@+0x40 | Attack feedback | field `+0x40` |
| 128 | DummyFieldUnit.get_Data | `0xDA8AF0` | Creates DisplayClass3_0 + returns hand | Monster data | none meaningful |
| 133 | DummyFieldUnit.**ChangeStatValue** | `0xDA7920` | GetStat(stat) + add delta (`FUN_180e690b0`) | Buff stat | hook |
| 134 | DummyFieldUnit.**ChangeStatValuePercent** | `0xDA7870` | GetStat + percent delta (`0x180e5d3e0`) | % buff | hook |
| 135 | DummyFieldUnit.**SetStatValue** | `0xDA7F20` | GetStat + set delta | Set stat directly | hook |
| 147 | DummyFieldUnit.GetSkills | `0xDA7A50` | Create list container + return | Skill list | none |
| 158 | StringListSearchProvider`String.ctor | `0xDAA530` | Static-init + `InGameConsoleHandler.Awake(this)` + provider resolve | Search provider | none |
| 159 | StringListSearchProvider`Int.ctor | `0xDAA4F0` | Same pattern, int variant | Search provider | none |
| 160/161 | CheatsSectionButton.get/set_SectionType | `0x6967B0` / `0xDA64C0` | **field enum @ +0x70** | Section category | field `+0x70` |
| 162 | CheatsSectionButton.**Initialize** | `0xDA5E80` | Sets title/icon text fields, `FUN_1843c3c60` toggles GO active states, writes `+0x70` | Section button init | hook |
| 163 | CheatsSectionButton.**SetIcons** | `0xDA5FA0` | Long body: icon atlas resolve, async image load, `0x18079b490` sprite, dispatch | Icon set | hook |
| 164 | CheatsSectionButton.SetText | `0xDA63E0` | Text set + controller name resolve, vtable+0x558 | Label | hook |
| 166 | CheatsSectionButton.ForceToOpen | `0xDA5E40` | Invoke event@+0x30 (+0x18 dispatch) | Open section | field `+0x30` |
| 167 | CheatsSectionButton.**SetSelectionState** | `0xDA6340` | Toggle selected GOs (param2 / ^1 across 4 props) + icon change | Select/deselect | hook |
| 168 | CheatsSectionButton.AnimateShow | `0xDA5D40` | DOTween sequence (0x185c333b0) + play | Anim in | hook (skip anim) |
| 169 | CheatsSectionButton.Dispose | `0xDA5D90` | Kill tweens | Cleanup | none |
| 171/172 | `<SetIcons>b__15_0/1` | `0xDA6460` / `0xDA6490` | Icon-load callbacks (set active) | Async icon | hook |
| 174 | CheatPageAccount.get_LoggedInToGoogle | `0xDA4CD0` | Real getter (platform login check) | Google login state | hook → force true |
| 197 | CheatsPageButton.GetDataIndex | `0x680030` | `return *(this+idx)` | Page button index | read field |
| 220 | CheatModuleMatch.GetEditorScenePath | `0xDA44F0` | Builds string from `String[6]` array + builder (`0x18349fb50`) | Scene path for editor | none |
| 228 | CheatModuleOther.**ShowHideConsole** | `0xDA4860` | Get console object (`0x181af43f0`), check active (`0x1843ca550`), **toggle setActive** (`0x1843c3c60`) | Show/hide in-game console | **hook → auto-show dev console** |
| 237 | TutorialAutoplay.`<Click>d__2.MoveNext` | `0xDAA570` | Real async state machine (coroutine body: coords 1.25/60.0 defaults) | Auto-click tutorial | hook → replay |
| 240 | TutorialAutoplay.Click(coords,ngui) | `0xDA4B60` | Async machine create, coords param | Auto-click | hook |
| 241 | TutorialAutoplay.ctor | `0xDA4BC0` | Sets `+0x24=1.25f`, `+0x28=60.0f` | Defaults | fields +0x24/+0x28 |

### 2a. Hollow module/page ctors — NOT exploitable (no logic)

Static-init + `if (*(cls+0xe4)==0) workaround; return;` only. Listed to prove coverage:

`0xDA4E70, 0xDA4C90, 0xDA4D70, 0xDA4DB0, 0xDA4DF0, 0xDA4E30, 0xDA4EB0, 0xDA4EF0, 0xDA4F30, 0xDA4F70, 0xDA4FB0, 0xDA4FF0, 0xDA5030, 0xDA5070, 0xDA50B0, 0xDA50F0, 0xDA5150, 0xDA5190, 0xDA51D0, 0xDA5210, 0xDA5250` (CheatPage* ctors) — all hollow.

CheatModule* ctors `0xDA3A90, 0xDA3AD0, 0xDA3B10, 0xDA3B50, 0xDA3B90, 0xDA3BD0, 0xDA3C50, 0xDA3C90, 0xDA3CD0, 0xDA3D10, 0xDA4470, 0xDA4630, 0xDA46A0, 0xDA4720, 0xDA4760, 0xDA47A0, 0xDA47E0, 0xDA4820, 0xDA4920, 0xDA4960, 0xDA49A0, 0xDA49E0, 0xDA4A20, 0xDA4AA0, 0xDA4AE0, 0xDA4B20, 0xDA4C10, 0xDA4C50, 0xF5D0A0, 0xF5D0E0, 0xFE4480` — all hollow. CheatPageBundle.ctor (`0xDA4E70`) likewise. CheatsWindow core ctors (`0x596CC0`, `0x5350E0`) — bare.

**Category summary:** of 246 entries → ~**40 real-logic methods** (tables above) → ~**18 high-value cheat surfaces** (marked **bold**).

---

## 3. The 22 unmatched entries (no decomp section at their TSV RVA)

| # | method | TSV RVA | Reason |
|---|---|---|---|
| 36 | ShowInfoWindow | `0xDA5410` | Adjacent section is `0xDA5420` (ShowNotEnough) — likely shared/merged or off-by-0x10 stub |
| 38 | ShowReconnectWindow | `0xDA5810` | `0xDA5820` present (ShowTwoButton) — likely shared stub |
| 40 | ShowBankErrorWindow | `0xB7A0F0` | Far outside CheatWindow cluster — real impl elsewhere; hookable independently |
| 54/55/56 | CheatValue`1 IsEqual / LoadFromPrefs / SaveToPrefs | `0x0` | **Abstract virtuals** — no native body; implementers are CheatBool (entries 45/46/47) |
| 59 | ICheatValue.get_Key | `0x0` | Interface abstract |
| 73 | DummyFieldElement.get_FieldController | `0x60F980` | Matches decomp but body is the `return 0` stub |
| 125/126 | CanBeTargeted / CanBeChosenByAttackTarget | `0x5A6470` | shared `return 1` stub (in table §1) |
| 127 | get_Animator | `0x60F980` | shared `return 0` stub |
| 137 | HasStatus | `0x606890` | shared bool stub |
| 157 | StringListSearchProvider`1.ctor | `0x5CD2A0` | Generic — no section; concrete variants are 158/159 |
| 165 | SetOnClickEvent | `0xDA6330` | Decomp section is `0xDA6340` (SetSelectionState) — ctor-merged or thin wrapper |
| 238 | TutorialAutoplay.SetStateMachine | `0x652E50` | Standard async `IAsyncStateMachine.SetStateMachine` stub |
| 239 | get_AutoplayTutorial | `0x606890` | shared bool stub |

---

## 4. Exploit surface summary (priority order)

### A. EL_Native method hooks (RVA-based, MinHook) — highest value
| Target | RVA | Payload |
|---|---|---|
| `DummyFieldUnit.ApplyDamage` | `0xDA7460` | force `return 0` → **immortal units** |
| `CheatSystemWindows.ShowError` | `0xDA5370` | swallow → no error popups (proxy already blocks codes 700-703,801,802) |
| `CheatModuleOther.ShowHideConsole` | `0xDA4860` | force true → **dev console** |
| `get/set_ForceEnableCensorship` | `0x5A5DD0`/`0x5A5E40` | flip bool → censorship off |
| `CheatsHideUIButton.Toggle/Init` | `0xDA5BD0`/`0xDA5AD0` | force UI hide/show |
| `GoTo.Fiend*` + b__0 callbacks | `0xDA3D50/41E0/3F50` + `DACE30/DACEF0/DAD070` | auto-confirm resource/reward grants |
| `DummyFieldUnit.GetStat` | `0xDA7BE0` | forge IUnitStat |
| `DummyFieldUnit.ChangeStatValue*` | `0xDA7920/7870/7F20` | arbitrary stat buffs |
| `CheatValue`1.SetValue` | `0x2E0B500` | inject cheat value writes |

### B. Field offsets (ResolveFieldOffset / direct write)
| Field | Offset | Used by |
|---|---|---|
| `state` | `+0x90` | DummyFieldUnit.get_state / ApplyDamage dead-check (`==5`) |
| `position` | `+0x94` | get_Position / **Move direct write** (teleport) |
| `controller` | `+0x18` | get_FieldController |
| `armySide` | `+0x20` | get_ArmySide |
| `ReactiveValue` | `+0x10` | DummyFieldUnit ctor (`RP` init via 0x181f706d0) |
| `event callback` | `+0x30` | CheatsSectionButton ForceToOpen |
| `sectionType` | `+0x70` | CheatsSectionButton |
| `key` | `+0x18` | CheatValue/CheatBool |
| `ReactiveValue` | `+0x10` | CheatValue RP (value read-through) |
| `hideFlag` | `+0x28` | CheatsHideUIButton |

**DummyFieldUnit layout (confirmed from ctor bodies + event accessors, 2026-08-11):**

| Offset | Size | Field | Evidence |
|---|---|---|---|
| `+0x10` | 8 | `ReactiveProperty` (unit core RP) | 3 ctor bodies `0xDA80A0/8160/7FD0` (init via 0x181f706d0) |
| `+0x18` | 8 | `FieldController` | ctor-3 `0xDA7FD0` + get_FieldController `0x5A6150` (`return *(this+0x18)`) |
| `+0x20` | 4 | `armySide` | ctor-3 `0xDA7FD0` + get_ArmySide `0x609190` (`return *(this+0x20)`) |
| `+0x28` | 8 | delegate `OnAttackPrepare` | add `0xDA8200` / remove `0xDA8B50` (list@+0x28) |
| `+0x30` | 8 | delegate `OnInsteadAttack` | add `0xDA86D0` / remove `0xDA9020` |
| `+0x38` | 8 | delegate `OnAttack` | add `0xDA82B0` / remove `0xDA8C00` |
| `+0x40` | 8 | delegate `OnAttacked` | add `0xDA8360` / remove `0xDA8CB0` |
| `+0x48` | 8 | delegate `OnDamageDealt` | add `0xDA8410` / remove `0xDA8D60` |
| `+0x50` | 8 | delegate `OnDamageReceived` | add `0xDA84C0` / remove `0xDA8E10` |
| `+0x58` | 8 | delegate `OnSkillReady` | add `0xDA8830` / remove `0xDA9180` |
| `+0x60` | 8 | delegate `OnStatusAdded` | add `0xDA88D0` / remove `0xDA9220` |
| `+0x68` | 8 | delegate `OnImmuneAdded` | add `0xDA8570` / remove `0xDA8EC0` |
| `+0x70` | 8 | delegate `OnStatusRemoved` | add `0xDA8980` / remove `0xDA92D0` |
| `+0x78` | 8 | delegate `OnImmuneRemoved` | add `0xDA8620` / remove `0xDA8F70` |
| `+0x80` | 8 | delegate `OnShieldChanged` | add `0xDA8780` / remove `0xDA90D0` |
| `+0x88` | 8 | delegate `OnUnitTransformed` | add `0xDA8A30` / remove `0xDA9380` |
| `+0x90` | 4 | `state` (int enum) | ctor bodies + get_state `0x645320` + ApplyDamage guard (`==5` dead) |
| `+0x94` | 8(12?) | `position` | ctor-1 `0xDA80A0` (zero via vec-ctor), get_Position `0xDA8B40`, **Move `0xDA7E70` direct write** |

**13 delegate slots in contiguous 8-byte stride `+0x28`→`+0x88`** — all 26 add/remove pairs (≠ earlier count of "11 events"). Intercept any one via its add_ RVA (e.g. `add_OnAttack` 0xDA82B0) to watch/fire unit events.

### C. Proxy MITM (already live in `el_proxy.json`)
- `resource_overrides` gold/gems/energy/contribution/elixir/spin — server-side rewrite
- `block_error_codes` [700-703,801,802] — suppresses the `ShowError` family
- roulette response rewrite (`ApplyCardRouletteSpinRewards` → qty 500) — bypasses the whole GetStat/ShowError path

### D. Dead ends (skip)
- All stubs in §1 + all hollow ctors in §2a — hooking them changes nothing.

---

## 5. Status of previously-open items (updated 2026-08-11)

1. **GoTo closure chain (`0x180002260`/`0x1800141d0`/`0x181c4fce0`) — RESOLVED.** Decompiled bodies of `FiendGetResource/Spend/GetReward` (+ their `b__0` callbacks) show these are **client-side GoTo window navigation** (`GoToController.ToResourceInAllGame/GoToBankPage` singletons), *not* a server route. `GetReturnManager_GoTo_Request_*` (RequestGetResource/RequestSpendResource/RequestGetReward) is **absent from the current method-pointer-map** (0 hits repo-wide) — a stale dump artifact. No wire route to proxy; hooks only auto-open the in-game UI. Real grants stay on the proxy (`resource_overrides`, roulette rewrite).
2. **Frida-verify `ApplyDamage` immortal — VERIFIED LIVE (2026-08-11).** `D:\Temp\opencode\frida_applydamage.js` hooks `GameAssembly.dll+0xDA7460` and force-returns 0 in `onLeave`. Method signature confirmed from decomp: `bool ApplyDamage(float damage, int type)`; native guard already returns 0 when `damage<=0 || state(+0x90)==5`; raw bytes at RVA verified: `xorps xmm0,xmm0; comiss xmm6,xmm0; jae` (dmg≤0) + `cmp dword[rbx+0x90],5; je` (state==5). Attached live to running game (PC, local device — *not* adb/emulator) and self-invoked the hooked function on a synthetic unit through both guard branches; both calls logged `[ApplyDamage] enter ... leave -> FORCED 0 (immortal)` with return 0. Tooling: `py -3.13 D:\Temp\opencode\pc_attach_hold.py <pid> frida_applydamage.js <sec>` (Frida `Module.findBaseAddress` is shadowed in this runtime — use `Process.getModuleByName('GameAssembly.dll').base`).
3. **DummyFieldUnit full field-offset map — DONE** (§4B): 13 delegate slots `+0x28`→`+0x88` (8-byte stride) + ctor-confirmed `+0x10 RP / +0x18 controller / +0x20 side / +0x90 state / +0x94 position`.