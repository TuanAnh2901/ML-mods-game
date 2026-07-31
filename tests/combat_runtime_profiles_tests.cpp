#include "../el_native/combat_runtime.h"
#include "../el_native/hook_registry.h"
#include "../el_native/profile_store.h"
#include "../el_native/config_registry.h"
#include "../el_native/combat_runtime_adapter.h"
#include "../injector/launcher.h"
#include "../el_native/automation.h"
#include "../el_native/main_thread_dispatcher.h"

#include <cassert>
#include <cmath>
#include <cstdio>
#include <fstream>
#include <string>
#include <direct.h>
#include <cstring>
#include <chrono>

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
    assert(coordinator.CompletedLoops() == 1);
    assert(coordinator.State() == AutomationState::Cooldown);
    coordinator.OnEvent(AutomationEvent::CooldownElapsed);
    assert(coordinator.State() == AutomationState::StartingBattle);
    coordinator.OnEvent(AutomationEvent::BattleStarted);
    coordinator.OnEvent(AutomationEvent::BattleFinished);
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
        assert(unlimited.State() == AutomationState::Cooldown);
        unlimited.OnEvent(AutomationEvent::CooldownElapsed);
    }
    assert(unlimited.CompletedLoops() == 3);
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
    TestIl2CppListAdapter();
    TestHookOwnership();
    TestCombatRuntimeGenerations();
    TestCombatModifiers();
    TestArmySideClassification();
    TestAutomationCoordinator();
    TestMainThreadDispatcher();
    TestProfileCrudAndRecovery();
    TestIniMigrationAndLauncherModes();
    TestTypedConfigRegistry();
    return 0;
}
