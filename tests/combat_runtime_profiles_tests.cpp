#include "../el_native/combat_runtime.h"
#include "../el_native/hook_registry.h"
#include "../el_native/profile_store.h"
#include "../el_native/config_registry.h"
#include "../el_native/combat_runtime_adapter.h"
#include "../injector/launcher.h"
#include "../el_native/automation.h"
#include "../el_native/main_thread_dispatcher.h"
#include "../el_native/multichest_delegate_guard.h"
#include "../el_native/method_fallback.inc"

#include <windows.h>

#include <cassert>
#include <cmath>
#include <cstdio>
#include <fstream>
#include <string>
#include <direct.h>
#include <cstring>
#include <chrono>
#include <cstdio>

static void StorePointer(void* base, size_t offset, void* value) {
    std::memcpy(static_cast<unsigned char*>(base) + offset, &value, sizeof(value));
}

struct MultichestShowCapture {
    void* provider = nullptr;
    void* controller = nullptr;
    void* adsManager = nullptr;
    int32_t rewardCount = 0;
    void* onClose = nullptr;
    void* beforeHideAction = nullptr;
    void* methodInfo = nullptr;
};

static MultichestShowCapture g_multichestShowCapture;
static void* g_multichestShowReturn = reinterpret_cast<void*>(0x76543210);

static void* __fastcall CaptureMultichestShow(void* provider, void* controller,
    void* adsManager, int32_t rewardCount, void* onClose, void* beforeHideAction,
    void* methodInfo) {
    g_multichestShowCapture = {
        provider, controller, adsManager, rewardCount, onClose, beforeHideAction, methodInfo};
    return g_multichestShowReturn;
}

static void TestMultichestShowForwarding() {
    void* provider = reinterpret_cast<void*>(0x11111110);
    void* controller = reinterpret_cast<void*>(0x22222220);
    void* adsManager = reinterpret_cast<void*>(0x33333330);
    void* onClose = reinterpret_cast<void*>(0x44444440);
    void* methodInfo = reinterpret_cast<void*>(0x55555550);
    MultichestDelegateGuardResult guardResult = MultichestDelegateGuardResult::InvalidInput;
    void* observedInvoke = reinterpret_cast<void*>(1);

    void* promise = ForwardMultichestShowWithGuard(&CaptureMultichestShow,
        0x100000, 0x200000, provider, controller, adsManager, 6, onClose,
        reinterpret_cast<void*>(~uintptr_t{0}), methodInfo, &guardResult, &observedInvoke);
    assert(promise == g_multichestShowReturn);
    assert(g_multichestShowCapture.provider == provider);
    assert(g_multichestShowCapture.controller == controller);
    assert(g_multichestShowCapture.adsManager == adsManager);
    assert(g_multichestShowCapture.rewardCount == 6);
    assert(g_multichestShowCapture.onClose == onClose);
    assert(g_multichestShowCapture.beforeHideAction == nullptr);
    assert(g_multichestShowCapture.methodInfo == methodInfo);
    assert(guardResult == MultichestDelegateGuardResult::ClearedInvalidDelegate);
    assert(observedInvoke == nullptr);

    auto* delegate = static_cast<unsigned char*>(
        VirtualAlloc(nullptr, 0x1000, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
    auto* executable = static_cast<unsigned char*>(
        VirtualAlloc(nullptr, 0x1000, MEM_RESERVE | MEM_COMMIT, PAGE_EXECUTE_READ));
    assert(delegate && executable);
    StorePointer(delegate, 0x18, executable);
    promise = ForwardMultichestShowWithGuard(&CaptureMultichestShow,
        reinterpret_cast<uintptr_t>(executable), reinterpret_cast<uintptr_t>(executable) + 0x1000,
        provider, controller, adsManager, 7, onClose, delegate, methodInfo,
        &guardResult, &observedInvoke);
    assert(promise == g_multichestShowReturn);
    assert(g_multichestShowCapture.rewardCount == 7);
    assert(g_multichestShowCapture.beforeHideAction == delegate);
    assert(guardResult == MultichestDelegateGuardResult::ValidDelegate);
    assert(observedInvoke == executable);

    VirtualFree(executable, 0, MEM_RELEASE);
    VirtualFree(delegate, 0, MEM_RELEASE);
    std::puts("MULTICHEST_SHOW_ABI=FORWARDED_6_ARGS");
    std::puts("MULTICHEST_SHOW_INVALID_CALLBACK=CLEARED_BEFORE_STORE");
}

static void TestMultichestDelegateGuard() {
    constexpr int32_t fieldOffset = 0x130;
    constexpr size_t invokeImplOffset = 0x18;
    auto* window = static_cast<unsigned char*>(
        VirtualAlloc(nullptr, 0x1000, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
    auto* delegate = static_cast<unsigned char*>(
        VirtualAlloc(nullptr, 0x1000, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
    auto* executable = static_cast<unsigned char*>(
        VirtualAlloc(nullptr, 0x1000, MEM_RESERVE | MEM_COMMIT, PAGE_EXECUTE_READ));
    auto* unreadable = static_cast<unsigned char*>(
        VirtualAlloc(nullptr, 0x1000, MEM_RESERVE | MEM_COMMIT, PAGE_NOACCESS));
    assert(window && delegate && executable && unreadable);

    const uintptr_t moduleBegin = reinterpret_cast<uintptr_t>(executable);
    const uintptr_t moduleEnd = moduleBegin + 0x1000;
    void* observedDelegate = reinterpret_cast<void*>(1);
    void* observedInvoke = reinterpret_cast<void*>(1);

    assert(SanitizeMultichestBeforeHideAction(window, fieldOffset, moduleBegin, moduleEnd,
        &observedDelegate, &observedInvoke) == MultichestDelegateGuardResult::NoDelegate);
    assert(observedDelegate == nullptr && observedInvoke == nullptr);

    StorePointer(window, fieldOffset, delegate + 1);
    assert(SanitizeMultichestBeforeHideAction(window, fieldOffset, moduleBegin, moduleEnd) ==
        MultichestDelegateGuardResult::ClearedInvalidDelegate);
    void* field = reinterpret_cast<void*>(1);
    std::memcpy(&field, window + fieldOffset, sizeof(field));
    assert(field == nullptr);

    StorePointer(window, fieldOffset, reinterpret_cast<void*>(~uintptr_t{0}));
    assert(SanitizeMultichestBeforeHideAction(window, fieldOffset, moduleBegin, moduleEnd) ==
        MultichestDelegateGuardResult::ClearedInvalidDelegate);

    StorePointer(window, fieldOffset, unreadable);
    assert(SanitizeMultichestBeforeHideAction(window, fieldOffset, moduleBegin, moduleEnd) ==
        MultichestDelegateGuardResult::ClearedInvalidDelegate);

    StorePointer(delegate, invokeImplOffset, reinterpret_cast<void*>(~uintptr_t{0}));
    StorePointer(window, fieldOffset, delegate);
    assert(SanitizeMultichestBeforeHideAction(window, fieldOffset, moduleBegin, moduleEnd) ==
        MultichestDelegateGuardResult::ClearedInvalidDelegate);

    StorePointer(delegate, invokeImplOffset, window);
    StorePointer(window, fieldOffset, delegate);
    assert(SanitizeMultichestBeforeHideAction(window, fieldOffset, moduleBegin, moduleEnd) ==
        MultichestDelegateGuardResult::ClearedInvalidDelegate);

    StorePointer(delegate, invokeImplOffset, executable);
    StorePointer(window, fieldOffset, delegate);
    assert(SanitizeMultichestBeforeHideAction(window, fieldOffset, moduleBegin, moduleEnd,
        &observedDelegate, &observedInvoke) == MultichestDelegateGuardResult::ValidDelegate);
    assert(observedDelegate == delegate && observedInvoke == executable);

    DWORD oldProtect = 0;
    assert(VirtualProtect(window, 0x1000, PAGE_READONLY, &oldProtect));
    StorePointer(delegate, invokeImplOffset, reinterpret_cast<void*>(~uintptr_t{0}));
    assert(SanitizeMultichestBeforeHideAction(window, fieldOffset, moduleBegin, moduleEnd) ==
        MultichestDelegateGuardResult::ClearFailed);
    DWORD ignored = 0;
    assert(VirtualProtect(window, 0x1000, oldProtect, &ignored));

    assert(SanitizeMultichestBeforeHideAction(nullptr, fieldOffset, moduleBegin, moduleEnd) ==
        MultichestDelegateGuardResult::InvalidInput);

    std::puts("GUARD_INVALID_INVOKE=CLEARED");
    std::puts("GUARD_VALID_INVOKE=RETAINED");

    VirtualFree(unreadable, 0, MEM_RELEASE);
    VirtualFree(executable, 0, MEM_RELEASE);
    VirtualFree(delegate, 0, MEM_RELEASE);
    VirtualFree(window, 0, MEM_RELEASE);
}

static void TestIl2CppListAdapter() {
    unsigned char list[0x30] = {};
    unsigned char array[0x38] = {};
    void* first = reinterpret_cast<void*>(0x1111);
    void* second = reinterpret_cast<void*>(0x2222);
    std::memcpy(list + 0x10, &array, sizeof(array));
    // The list stores the array pointer, not the array bytes.
    void* arrayPtr = array;
    std::memcpy(list + 0x10, &arrayPtr, sizeof(arrayPtr));
    const int size = 2;
    std::memcpy(list + 0x18, &size, sizeof(size));
    std::memcpy(array + 0x20, &first, sizeof(first));
    std::memcpy(array + 0x28, &second, sizeof(second));
    const auto values = ExtractIl2CppListPointers(list, 8);
    assert(values.size() == 2);
    assert(values[0] == first && values[1] == second);
}

static void TestHookOwnership() {
    HookRegistry registry;
    const uintptr_t address = 0x1234;
    assert(registry.Claim(address, "rapid-fire"));
    assert(!registry.Claim(address, "multi-hit"));
    assert(registry.Status(address) == HookStatus::Conflict);
    assert(registry.Owner(address) == "rapid-fire");
    assert(registry.MarkResolved(address));
    assert(registry.MarkHooked(address));
    assert(registry.Status(address) == HookStatus::Hooked);
}

static void TestCombatRuntimeGenerations() {
    CombatRuntime runtime;
    runtime.BeginSession(7);
    UnitSnapshot first;
    first.pointer = 0x1000;
    first.monsterUid = 42;
    first.name = "ROLE_A";
    first.grade = 3;
    first.side = ArmySide::Player;
    first.hp = 100.0f;
    first.mana = 20.0f;
    first.alive = true;
    runtime.ObserveUnit(first);
    assert(runtime.Snapshot().size() == 1);
    assert(runtime.GenerationFor(first.pointer) == 1);

    first.hp = 80.0f;
    runtime.ObserveUnit(first);
    assert(runtime.Snapshot().front().hp == 80.0f);
    assert(runtime.Snapshot().front().callCount == 1);
    assert(runtime.GenerationFor(first.pointer) == 1);

    first.monsterUid = 43;
    runtime.ObserveUnit(first);
    assert(runtime.GenerationFor(first.pointer) == 2);
    runtime.DisposeUnit(first.pointer);
    assert(runtime.Snapshot().empty());
    first.monsterUid = 44;
    runtime.ObserveUnit(first);
    assert(runtime.GenerationFor(first.pointer) == 3);
}

static void TestCombatModifiers() {
    CombatConfig config;
    config.attackSpeedMultiplier = 2.0f;
    config.multiHit = 7;
    config.freezeEnemies = true;
    config.dumbEnemies = true;
    assert(std::fabs(ApplyTurnInterval(1.0f, ArmySide::Player, config) - 0.5f) < 0.0001f);
    assert(std::fabs(ApplyTurnInterval(1.0f, ArmySide::Enemy, config) - 1.0f) < 0.0001f);
    assert(MultiHitCount(ArmySide::Player, config) == 5);
    assert(MultiHitCount(ArmySide::Enemy, config) == 1);
    assert(IsTimeFrozen(ArmySide::Enemy, false, config));
    assert(!IsTimeFrozen(ArmySide::Player, false, config));
    assert(!AllowAttackActivation(ArmySide::Enemy, true, config));
    assert(AllowAttackActivation(ArmySide::Player, true, config));
    int calls = 0;
    config.multiHit = 3;
    assert(ExecuteMultiHit(ArmySide::Player, config, [](void* context) { ++*static_cast<int*>(context); }, &calls) == 3);
    assert(calls == 3);
}

static void TestArmySideClassification() {
    assert(ArmySideFromRaw(0, 1) == ArmySide::Unknown);
    assert(ArmySideFromRaw(1, 1) == ArmySide::Player);
    assert(ArmySideFromRaw(2, 1) == ArmySide::Enemy);
    assert(ArmySideFromRaw(2, 2) == ArmySide::Player);
    assert(ArmySideFromRaw(1, 2) == ArmySide::Enemy);
    assert(ArmySideFromRaw(-1, 1) == ArmySide::Unknown);
}

static void TestAutomationCoordinator() {
    AutomationCoordinator coordinator;
    coordinator.Configure(AutomationMode::AutoBattle, 2);
    assert(coordinator.Start());
    assert(coordinator.State() == AutomationState::StartingBattle);
    coordinator.OnEvent(AutomationEvent::BattleStarted);
    coordinator.OnEvent(AutomationEvent::BattleFinished);
    coordinator.OnEvent(AutomationEvent::RewardCollected);
    coordinator.OnEvent(AutomationEvent::RewardCollected);
    assert(coordinator.CompletedLoops() == 1);
    assert(coordinator.State() == AutomationState::Cooldown);
    coordinator.OnEvent(AutomationEvent::CooldownElapsed);
    assert(coordinator.State() == AutomationState::StartingBattle);
    coordinator.OnEvent(AutomationEvent::BattleStarted);
    coordinator.OnEvent(AutomationEvent::BattleFinished);
    coordinator.OnEvent(AutomationEvent::RewardCollected);
    coordinator.OnEvent(AutomationEvent::RewardCollected);
    assert(coordinator.CompletedLoops() == 2);
    assert(coordinator.State() == AutomationState::Done);
    assert(!coordinator.Start());

    AutomationCoordinator unlimited;
    unlimited.Configure(AutomationMode::Derank, 0);
    assert(unlimited.Start());
    for (int i = 0; i < 3; ++i) {
        unlimited.OnEvent(AutomationEvent::BattleStarted);
        unlimited.OnEvent(AutomationEvent::BattleFinished);
        unlimited.OnEvent(AutomationEvent::RewardCollected);
        unlimited.OnEvent(AutomationEvent::RewardCollected);
        assert(unlimited.State() == AutomationState::Cooldown);
        unlimited.OnEvent(AutomationEvent::CooldownElapsed);
    }
    assert(unlimited.CompletedLoops() == 3);
}

static void TestAutoBattleTriggerDecision() {
    assert(DecideAutoBattleTrigger(false, false, false, false) ==
        AutoBattleTriggerDecision::WaitForController);
    assert(DecideAutoBattleTrigger(true, false, false, false) ==
        AutoBattleTriggerDecision::WaitForInitialization);
    assert(DecideAutoBattleTrigger(true, true, false, false) ==
        AutoBattleTriggerDecision::WaitForInitialization);
    assert(DecideAutoBattleTrigger(true, true, true, false) ==
        AutoBattleTriggerDecision::Invoke);
    assert(DecideAutoBattleTrigger(true, true, true, true) ==
        AutoBattleTriggerDecision::Complete);
    std::puts("AUTOBATTLE_PREINIT=WAIT");
    std::puts("AUTOBATTLE_READY_INACTIVE=INVOKE");
    std::puts("AUTOBATTLE_ACTIVE=COMPLETE");
}

static void TestAutoBattleReadinessWithoutStartObserver() {
    assert(IsAutoBattleRuntimeReady(true, true, true, true));
    assert(!IsAutoBattleRuntimeReady(false, true, true, true));
    assert(!IsAutoBattleRuntimeReady(true, false, true, true));
    assert(!IsAutoBattleRuntimeReady(true, true, false, true));
    assert(!IsAutoBattleRuntimeReady(true, true, true, false));

    assert(!HasAutoBattleStartBarrier(true, false, true));
    assert(HasAutoBattleStartBarrier(true, true, false));
    assert(!HasAutoBattleStartBarrier(false, false, false));
    assert(HasAutoBattleStartBarrier(false, false, true));
    std::puts("AUTOBATTLE_START_OBSERVER=OPTIONAL");
    std::puts("AUTOBATTLE_BATTLEFIELD_START_FALLBACK=OK");
}

static uintptr_t FindMethodFallbackRva(const char* klass, const char* method, int argc) {
    for (size_t index = 0; index < g_methodFallbackCount; ++index) {
        const MethodFallbackEntry& entry = g_methodFallbackTable[index];
        if (std::strcmp(entry.klass, klass) == 0 &&
            std::strcmp(entry.method, method) == 0 &&
            entry.argc == argc) {
            return entry.rva;
        }
    }
    return 0;
}

static void TestAutoBattleLifecycleFallbacks() {
    assert(FindMethodFallbackRva("BattlefieldWindow", "get_AutoBattleController", 0) == 0xBFC510);
    assert(FindMethodFallbackRva("AutoBattleController", "OnAutoBattleClicked", 0) == 0xEE4B10);
    assert(FindMethodFallbackRva("AutoBattleController", "IsAutoBattleActive", 0) == 0xEE4AC0);
    assert(FindMethodFallbackRva("AutoBattleController", "Start", 0) == 0xEE5090);
    assert(FindMethodFallbackRva("AutoBattleController", "SetAutoBattleInactive", 0) == 0xEE5030);
    std::puts("AUTOBATTLE_LIFECYCLE_FALLBACKS=OK");
}

static void TestAutomationMethodFallbackCoverage() {
    assert(FindMethodFallbackRva("ItemModule", "IsEnoughResource", 2) == 0xDF3240);
    assert(FindMethodFallbackRva("ExternalJourneyFighter", "ClaimRewardsOnWin", 0) == 0xD4EC10);
    assert(FindMethodFallbackRva("MultichestWindow", "ShowMultichestWindow", 6) == 0x9808A0);
    assert(FindMethodFallbackRva("IdleChestPresenter", "ShowWindow", 1) == 0x6C9BE0);
    assert(FindMethodFallbackRva("IdleChestPresenter", "OnPreclaimRewards", 0) == 0x6C9380);
    std::puts("AUTOMATION_METHOD_FALLBACK_COVERAGE=OK");
}

static void TestRewardSettlementDecision() {
    assert(DecideRewardSettlement(false, false, false, false, false) ==
        RewardSettlementDecision::Wait);
    assert(DecideRewardSettlement(false, false, false, false, true) ==
        RewardSettlementDecision::ManualClaimRequired);
    assert(DecideRewardSettlement(false, false, false, true, false) ==
        RewardSettlementDecision::CompleteAfterConfirmedClaim);
    assert(DecideRewardSettlement(true, false, false, true, true) ==
        RewardSettlementDecision::Wait);
}

static void TestPlayInvokeWatchdogDecision() {
    assert(DecidePlayInvoke(true, true, true, true) == PlayInvokeDecision::InvokeViaWatchdog);
    assert(DecidePlayInvoke(false, true, true, true) == PlayInvokeDecision::WaitForHook);
    assert(DecidePlayInvoke(true, false, true, true) == PlayInvokeDecision::WaitForHook);
    assert(DecidePlayInvoke(true, true, false, true) == PlayInvokeDecision::WaitForHook);
    assert(DecidePlayInvoke(true, true, true, false) == PlayInvokeDecision::WaitForHook);
}

static void TestMultichestAutomationStateMachine() {
    MultichestRuntimeState runtime;
    assert(runtime.Phase() == MultichestPhase::Hidden);

    runtime.OnShown(reinterpret_cast<void*>(0x1000));
    runtime.OnShown(reinterpret_cast<void*>(0x1000));
    assert(runtime.Phase() == MultichestPhase::WaitingForInitialization);
    assert(runtime.Generation() == 1);

    MultichestSnapshot snapshot{};
    snapshot.rewardCount = 6;
    snapshot.actualCount = 6;
    snapshot.cardsAppearAnimDone = true;
    snapshot.openAllLock = false;
    snapshot.openCardsButtonPresent = true;
    snapshot.openAllActive = true;
    assert(runtime.Decide(snapshot) == MultichestAction::InvokeOpenAll);

    const MultichestSnapshot beforeOpenAll = snapshot;
    runtime.OnOpenAllReturned(beforeOpenAll, snapshot);
    assert(runtime.Phase() == MultichestPhase::WaitingForOpenAll);
    assert(!runtime.OpenAllInvoked());

    snapshot.pressedOpenAll = true;
    snapshot.openAllActive = false;
    runtime.OnOpenAllReturned(beforeOpenAll, snapshot);
    assert(runtime.Phase() == MultichestPhase::WaitingForRewardSettlement);
    assert(runtime.OpenAllInvoked());

    snapshot.currentCount = 5;
    snapshot.pressedOpenAll = false;
    assert(runtime.Decide(snapshot) == MultichestAction::Wait);
    snapshot.currentCount = 6;
    assert(runtime.Decide(snapshot) == MultichestAction::RequestClose);

    runtime.OnCloseRequested();
    assert(runtime.Phase() == MultichestPhase::CloseRequested);
    assert(runtime.OnHidden());
    assert(!runtime.OnHidden());
    assert(runtime.Phase() == MultichestPhase::Hidden);

    MultichestRuntimeState hiddenOpenAll;
    hiddenOpenAll.OnShown(reinterpret_cast<void*>(0x2000));
    snapshot = {};
    snapshot.rewardCount = 6;
    snapshot.actualCount = 6;
    snapshot.cardsAppearAnimDone = true;
    snapshot.openCardsButtonPresent = true;
    snapshot.buttonsActive = false;
    snapshot.openAllActive = false;
    assert(hiddenOpenAll.Decide(snapshot) == MultichestAction::Wait);
    std::puts("MULTICHEST_DUPLICATE_SHOW=COALESCED");
    std::puts("MULTICHEST_SETTLEMENT=COUNT_CONFIRMED");
    std::puts("MULTICHEST_HIDDEN_EVENT=ONCE");
}

static void TestMultichestCloseLockGating() {
    MultichestRuntimeState runtime;
    runtime.OnShown(reinterpret_cast<void*>(0x3000));

    MultichestSnapshot snapshot{};
    snapshot.rewardCount = 6;
    snapshot.actualCount = 6;
    snapshot.currentCount = 6; // already settled
    snapshot.cardsAppearAnimDone = true;
    snapshot.openCardsButtonPresent = true;
    snapshot.openAllActive = true;
    snapshot.pressedOpenAll = true;
    snapshot.openAllLock = false;

    MultichestSnapshot beforeOpenAll = snapshot;
    beforeOpenAll.currentCount = 0;
    beforeOpenAll.pressedOpenAll = false;
    runtime.OnOpenAllReturned(beforeOpenAll, snapshot);
    assert(runtime.Phase() == MultichestPhase::WaitingForRewardSettlement);
    assert(runtime.OpenAllInvoked());

    // Counts settled but animation lock still held -> must NOT close.
    snapshot.openAllLock = true;
    assert(runtime.Decide(snapshot) == MultichestAction::Wait);
    // Lock cleared -> close is safe.
    snapshot.openAllLock = false;
    assert(runtime.Decide(snapshot) == MultichestAction::RequestClose);
    std::puts("MULTICHEST_CLOSE_LOCK=GATED");
}

static void TestMultichestForcedCloseDecision() {
    assert(ShouldForceCloseMultichest(true, true, false));
    assert(!ShouldForceCloseMultichest(true, true, true));
    assert(!ShouldForceCloseMultichest(true, false, false));
    assert(!ShouldForceCloseMultichest(false, true, false));
    assert(!ShouldForceCloseMultichest(false, false, false));
    std::puts("MULTICHEST_FORCED_CLOSE=LOCK_AWARE");
}

static void TestAutomationEventOrdering() {
    assert(CanArmAutoBattleAtBattlefieldStart(AutomationState::StartingBattle));
    assert(CanArmAutoBattleAtBattlefieldStart(AutomationState::InBattle));
    assert(!CanArmAutoBattleAtBattlefieldStart(AutomationState::Idle));
    assert(!CanArmAutoBattleAtBattlefieldStart(AutomationState::CollectingReward));
    std::puts("AUTOBATTLE_STARTING_STATE=ARMED");
}

static void TestMainThreadDispatcher() {
    MainThreadDispatcher dispatcher;
    int calls = 0;
    assert(dispatcher.PostAfter(std::chrono::milliseconds(0), [&] { ++calls; }) != 0);
    assert(dispatcher.Tick() == 1);
    assert(calls == 1);
    assert(dispatcher.Tick() == 0);
    dispatcher.PostAfter(std::chrono::hours(1), [&] { ++calls; });
    dispatcher.Clear();
    assert(dispatcher.Tick() == 0);
}

static void TestProfileCrudAndRecovery() {
    const std::string path = "D:\\Temp\\opencode\\el_native_profiles_test.json";
    ProfileStore store(path);
    ProfileDocument document;
    document.schemaVersion = 1;
    document.currentProfile = "default";
    document.profiles["default"].name = "default";
    document.profiles["default"].enabled["rapid_fire"] = true;
    document.profiles["default"].settings["multi_hit"] = "2";
    assert(store.Save(document));

    ProfileDocument loaded;
    assert(store.Load(loaded));
    assert(loaded.currentProfile == "default");
    assert(loaded.profiles["default"].enabled["rapid_fire"]);
    assert(loaded.profiles["default"].settings["multi_hit"] == "2");

    assert(store.Clone(loaded, "default", "arena"));
    assert(store.Rename(loaded, "arena", "raid"));
    assert(store.SetCurrent(loaded, "raid"));
    assert(store.Remove(loaded, "default"));
    assert(loaded.currentProfile == "raid");
    assert(store.Save(loaded));

    std::ofstream corrupt(path.c_str(), std::ios::trunc);
    corrupt << "{broken";
    corrupt.close();
    ProfileDocument recovered;
    assert(!store.Load(recovered));
    assert(store.RecoverBackup(recovered));
    assert(recovered.currentProfile == "raid");
}

static void TestIniMigrationAndLauncherModes() {
    const std::string ini = "D:\\Temp\\opencode\\el_native_config_test.ini";
    std::ofstream out(ini.c_str(), std::ios::trunc);
    out << "rapid_fire=1\n";
    out << "multi_hit=3\n";
    out.close();
    ProfileStore store("D:\\Temp\\opencode\\el_native_profiles_migration.json");
    ProfileDocument document;
    assert(store.MigrateIni(ini, document));
    assert(document.profiles["default"].enabled["rapid_fire"]);
    assert(document.profiles["default"].settings["multi_hit"] == "3");

    assert(ResolveLaunchMode(false, false, false) == LaunchMode::Configure);
    assert(ResolveLaunchMode(true, true, false) == LaunchMode::Configure);
    assert(ResolveLaunchMode(true, false, false) == LaunchMode::Launch);
    assert(ResolveLaunchMode(true, false, true) == LaunchMode::Configure);

    const std::string folder = "D:\\Temp\\opencode\\launcher_fixture";
    _mkdir(folder.c_str());
    std::ofstream(folder + "\\Everlusting Life.exe").close();
    std::ofstream(folder + "\\GameAssembly.dll").close();
    std::ofstream(folder + "\\UnityPlayer.dll").close();
    std::string error;
    assert(ValidateGameFolder(folder, "Everlusting Life.exe", error));
    std::remove((folder + "\\Everlusting Life.exe").c_str());
    std::remove((folder + "\\GameAssembly.dll").c_str());
    std::remove((folder + "\\UnityPlayer.dll").c_str());
    _rmdir(folder.c_str());

    const std::string steamRoot = "D:\\Temp\\opencode\\steam_fixture";
    _mkdir(steamRoot.c_str());
    _mkdir((steamRoot + "\\steamapps").c_str());
    _mkdir((steamRoot + "\\steamapps\\common").c_str());
    _mkdir((steamRoot + "\\steamapps\\common\\ROLE_A").c_str());
    std::ofstream appid(steamRoot + "\\steamapps\\common\\ROLE_A\\steam_appid.txt", std::ios::trunc);
    appid << "3218710\n";
    appid.close();
    assert(DetectSteamAppId(steamRoot + "\\steamapps\\common\\ROLE_A") == "3218710");
    std::remove((steamRoot + "\\steamapps\\common\\ROLE_A\\steam_appid.txt").c_str());
    _rmdir((steamRoot + "\\steamapps\\common\\ROLE_A").c_str());
    _rmdir((steamRoot + "\\steamapps\\common").c_str());
    _rmdir((steamRoot + "\\steamapps").c_str());
    _rmdir(steamRoot.c_str());
}

static void TestTypedConfigRegistry() {
    ConfigRegistry registry;
    bool enabled = false;
    int hits = 1;
    float multiplier = 1.0f;
    std::string hotkey = "F6";
    registry.RegisterBool("enabled", &enabled);
    registry.RegisterInteger("hits", &hits);
    registry.RegisterFloat("multiplier", &multiplier);
    registry.RegisterHotkey("hotkey", &hotkey);
    assert(registry.Set("enabled", "true"));
    assert(registry.Set("hits", "4"));
    assert(registry.Set("multiplier", "2.5"));
    assert(registry.Set("hotkey", "F7"));
    assert(enabled && hits == 4 && std::fabs(multiplier - 2.5f) < 0.0001f && hotkey == "F7");
}

int main() {
    TestMultichestShowForwarding();
    TestMultichestDelegateGuard();
    TestIl2CppListAdapter();
    TestHookOwnership();
    TestCombatRuntimeGenerations();
    TestCombatModifiers();
    TestArmySideClassification();
    TestAutomationCoordinator();
    TestAutoBattleTriggerDecision();
    TestAutoBattleReadinessWithoutStartObserver();
    TestAutoBattleLifecycleFallbacks();
    TestAutomationMethodFallbackCoverage();
    TestRewardSettlementDecision();
    TestPlayInvokeWatchdogDecision();
    TestMultichestAutomationStateMachine();
    TestMultichestCloseLockGating();
    TestMultichestForcedCloseDecision();
    TestAutomationEventOrdering();
    TestMainThreadDispatcher();
    TestProfileCrudAndRecovery();
    TestIniMigrationAndLauncherModes();
    TestTypedConfigRegistry();
    return 0;
}
