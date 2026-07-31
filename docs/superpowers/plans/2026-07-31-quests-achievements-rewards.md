# Quests, Achievements, and Rewards Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add independently controlled Daily, Weekly, Achievement, and reward-audit controllers.

**Architecture:** `quest.cpp` remains the feature entry point; `quest.h` holds testable packed-value, audit, and controller-state helpers. Each runtime controller owns its dependency gate and audit records.

**Tech Stack:** C++17, MinHook, ImGui, existing IL2CPP resolver.

## Global Constraints

- Resolve targets by type, method, arity, and signature metadata.
- Shared-RVA targets stay disabled until type-specific resolution is confirmed.
- Daily, Weekly, Achievement, display-reward, and local-reward toggles are independent.
- A non-success result disables only its reward controller and appends an audit event.
- Every new behavior begins with a failing test.

---

## Task 1: Packed values and reward audit

**Files:** `el_native/features/quest.h`, `el_native/features/quest_tests.cpp`

- [ ] **Step 1: Write RED tests**

```cpp
TEST(ObscuredBoolTests, EncodesAndDecodesTrue) { EXPECT_TRUE(ObscuredBool::from(true).value()); }
TEST(RewardAuditTests, RetainsNewestEvents) { RewardAuditBuffer a(2); a.push({.questId=1}); a.push({.questId=2}); a.push({.questId=3}); EXPECT_EQ(a.events().front().questId, 2); }
```

- [ ] **Step 2: Verify RED**

Run the project test target filtered to these two tests. Expected: missing helper symbols.

- [ ] **Step 3: Implement helpers**

```cpp
struct ObscuredBool { uint8_t cryptoKey, hiddenValue, fakeValue, inited; bool value() const; static ObscuredBool from(bool); };
struct RewardAuditEvent { int32_t questId; float baseValue, displayValue, localValue; int32_t responseCode; };
class RewardAuditBuffer { public: void push(RewardAuditEvent); std::span<const RewardAuditEvent> events() const; };
```

- [ ] **Step 4: Verify GREEN and commit**

Run filtered tests and build; commit `quest.h` and `quest_tests.cpp` with `feat: add quest audit primitives`.

## Task 2: Daily and Weekly controllers

**Files:** `el_native/features/quest.cpp`, `el_native/features/quest.h`, `el_native/features/quest_tests.cpp`

- [ ] **Step 1: Write RED test**

```cpp
TEST(QuestControllerTests, MissingWeeklyDependencyDoesNotDisableDaily) { EXPECT_TRUE(CanEnableQuestController({true, true, QuestScope::Daily})); EXPECT_FALSE(CanEnableQuestController({true, false, QuestScope::Weekly})); }
```

- [ ] **Step 2: Verify RED, implement, verify GREEN**

Add `QuestScope`, `QuestControllerState`, and per-scope dependency gates. Resolve `QuestRefreshTimer`, `QuestRefreshHandler`, and `QuestChestsController`; add Daily/Weekly UI sections with availability and shared-RVA warnings. Run filtered tests and build.

- [ ] **Step 3: Commit**

Commit Quest source/header/tests with `feat: split daily and weekly quest controllers`.

## Task 3: Achievement controller

**Files:** `el_native/features/quest.cpp`, `el_native/features/quest.h`, `el_native/features/quest_tests.cpp`

- [ ] **Step 1: Write RED test**

```cpp
TEST(AchievementControllerTests, RequiresObservationDependencies) { EXPECT_FALSE(CanEnableAchievementController({false,0,0,0.0f,false})); EXPECT_TRUE(CanEnableAchievementController({true,10,2,0.5f,true})); }
```

- [ ] **Step 2: Verify RED, implement, verify GREEN**

Resolve `IQuestModule`, `PlayerInfoModule`, achievement notification, and Steam-update targets. Add independent Achievement UI showing total/chosen, percentage, notification count, Steam state, and audit records. Run tests/build.

- [ ] **Step 3: Commit**

Commit with `feat: add achievement observation controller`.

## Task 4: Reward controllers and response audit

**Files:** `el_native/features/quest.cpp`, `el_native/features/quest.h`, `el_native/features/quest_tests.cpp`

- [ ] **Step 1: Write RED tests**

```cpp
TEST(RewardControllerTests, AppliesMultiplier) { EXPECT_FLOAT_EQ(ApplyMultiplier(10, 1.0f),10); EXPECT_FLOAT_EQ(ApplyMultiplier(10,2.5f),25); }
TEST(RewardControllerTests, FailureDisablesAffectedMode) { EXPECT_TRUE(ShouldDisableRewardController(700)); EXPECT_FALSE(ShouldDisableRewardController(0)); }
```

- [ ] **Step 2: Verify RED, implement, verify GREEN**

Resolve reward target, add independent display/local toggles, append base/display/local/result records, disable only failed controller, and disable unresolved UI controls. Run filtered/full tests and build.

- [ ] **Step 3: Final commit**

Run `git diff --check`, then commit Quest files with `feat: add quest reward audit controllers`.
