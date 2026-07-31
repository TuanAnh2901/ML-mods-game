#include "combat_runtime_feature.h"

#include "../combat_runtime_adapter.h"
#include "../framework.h"
#include "../config_registry.h"
#include "../hook_registry.h"
#include "../il2cpp_resolve.h"
#include "../../minhook/include/MinHook.h"
#include "imgui.h"

#include <chrono>
#include <string>
#include <fstream>
#include <vector>
#include <cwchar>

namespace {
typedef void* (__fastcall* StartBattle_t)(void* self, void* fightData, void* participants, void* mi);
typedef void* (__fastcall* GetAllUnits_t)(void* self, void* mi);
typedef void (__fastcall* VoidUnit_t)(void* self, void* mi);
typedef bool (__fastcall* BoolUnit_t)(void* self, void* mi);
typedef int32_t (__fastcall* IntUnit_t)(void* self, void* mi);
typedef void* (__fastcall* TargetUnit_t)(void* self, void* mi);
typedef void* (__fastcall* ToString_t)(void* self, void* mi);
typedef void* (__fastcall* ObjectUnit_t)(void* self, void* mi);
typedef void (__fastcall* AttackActivation_t)(void* self, void* targets, void* mi);
typedef void (__fastcall* HandleAttack_t)(void* self, void* target, int32_t index, void* mi);

StartBattle_t Original_StartBattle = nullptr;
GetAllUnits_t Original_GetAllUnits = nullptr;
VoidUnit_t Original_BattleEnded = nullptr;
VoidUnit_t Original_Dispose = nullptr;
BoolUnit_t Original_IsNotTimeFrozen = nullptr;
AttackActivation_t Original_AttackActivation = nullptr;
HandleAttack_t Original_HandleAttackInternal = nullptr;
IntUnit_t Resolved_GetArmySide = nullptr;
IntUnit_t Resolved_GetGrade = nullptr;
TargetUnit_t Resolved_GetTargetUnit = nullptr;
ToString_t Resolved_ToString = nullptr;
ObjectUnit_t Resolved_GetData = nullptr;
IntUnit_t Resolved_GetMonsterId = nullptr;
ToString_t Resolved_GetMonsterName = nullptr;
int32_t Resolved_ArmySideField = -1;

CombatConfig s_config;
bool s_active = false;
int s_localArmySide = 1;
char s_search[64] = {};

int CopyIl2CppString(void* value, wchar_t* output, int capacity) {
    if (!value || !output || capacity <= 1) return 0;
    __try {
        const auto* base = reinterpret_cast<const unsigned char*>(value);
        const int32_t length = *reinterpret_cast<const int32_t*>(base + 0x10);
        if (length <= 0 || length >= capacity || length > 512) return 0;
        const auto* chars = reinterpret_cast<const wchar_t*>(base + 0x14);
        std::wmemcpy(output, chars, static_cast<std::size_t>(length));
        output[length] = L'\0';
        return length;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return 0;
    }
}

std::string ReadIl2CppString(void* value) {
    wchar_t buffer[513] = {};
    const int length = CopyIl2CppString(value, buffer, 513);
    if (length <= 0) return {};
    const int bytes = WideCharToMultiByte(CP_UTF8, 0, buffer, length, nullptr, 0, nullptr, nullptr);
    if (bytes <= 0) return {};
    std::string result(static_cast<std::size_t>(bytes), '\0');
    WideCharToMultiByte(CP_UTF8, 0, buffer, length, &result[0], bytes, nullptr, nullptr);
    return result;
}

ArmySide SideFor(void* self) {
    if (!self) return ArmySide::Unknown;
    int raw = -1;
    if (Resolved_GetArmySide) raw = Resolved_GetArmySide(self, nullptr);
    else if (Resolved_ArmySideField >= 0) {
        __try { raw = *reinterpret_cast<const int32_t*>(static_cast<const unsigned char*>(self) + Resolved_ArmySideField); }
        __except (EXCEPTION_EXECUTE_HANDLER) { raw = -1; }
    }
    return ArmySideFromRaw(raw, s_localArmySide);
}

void ObserveBattleUnit(void* unit) {
    if (!unit || !GlobalCombatRuntime().Active()) return;
    UnitSnapshot snapshot;
    snapshot.pointer = reinterpret_cast<std::uintptr_t>(unit);
    snapshot.side = SideFor(unit);
    snapshot.grade = Resolved_GetGrade ? Resolved_GetGrade(unit, nullptr) : 0;
    snapshot.target = Resolved_GetTargetUnit ? reinterpret_cast<std::uintptr_t>(Resolved_GetTargetUnit(unit, nullptr)) : 0;
    void* data = Resolved_GetData ? Resolved_GetData(unit, nullptr) : nullptr;
    snapshot.monsterUid = data && Resolved_GetMonsterId ? Resolved_GetMonsterId(data, nullptr) : 0;
    snapshot.name = data && Resolved_GetMonsterName ? ReadIl2CppString(Resolved_GetMonsterName(data, nullptr)) : std::string();
    if (snapshot.name.empty()) snapshot.name = Resolved_ToString ? ReadIl2CppString(Resolved_ToString(unit, nullptr)) : std::string();
    if (snapshot.name.empty()) snapshot.name = "BattleUnit";
    snapshot.alive = true;
    GlobalCombatRuntime().ObserveUnit(snapshot);
}

void* __fastcall StartBattleHook(void* self, void* fightData, void* participants, void* mi) {
    GlobalCombatRuntime().BeginSession(static_cast<std::uint64_t>(
        std::chrono::steady_clock::now().time_since_epoch().count()));
    LOG("[RUNTIME] FieldController.StartBattle session=%llu",
        static_cast<unsigned long long>(GlobalCombatRuntime().SessionId()));
    void* promise = Original_StartBattle ? Original_StartBattle(self, fightData, participants, mi) : nullptr;
    return promise;
}

void* __fastcall GetAllUnitsHook(void* self, void* mi) {
    void* list = Original_GetAllUnits ? Original_GetAllUnits(self, mi) : nullptr;
    if (GlobalCombatRuntime().Active() && list) {
        const std::vector<void*> units = ExtractIl2CppListPointers(list);
        for (void* unit : units) ObserveBattleUnit(unit);
        if (!units.empty()) LOG("[RUNTIME] snapshot refreshed units=%zu", units.size());
    }
    return list;
}

void __fastcall BattleEndedHook(void* self, void* mi) {
    if (Original_BattleEnded) Original_BattleEnded(self, mi);
    GlobalCombatRuntime().EndSession();
    LOG("[RUNTIME] FieldController.BattleEnded snapshot cleared");
}

void __fastcall DisposeHook(void* self, void* mi) {
    GlobalCombatRuntime().DisposeUnit(reinterpret_cast<std::uintptr_t>(self));
    if (Original_Dispose) Original_Dispose(self, mi);
}

bool __fastcall IsNotTimeFrozenHook(void* self, void* mi) {
    const bool original = Original_IsNotTimeFrozen ? Original_IsNotTimeFrozen(self, mi) : true;
    if (s_active && SideFor(self) == ArmySide::Enemy && s_config.freezeEnemies) return false;
    return original;
}

void __fastcall AttackActivationHook(void* self, void* targets, void* mi) {
    if (s_active && SideFor(self) == ArmySide::Enemy && s_config.dumbEnemies) return;
    if (Original_AttackActivation) Original_AttackActivation(self, targets, mi);
}

void __fastcall HandleAttackInternalHook(void* self, void* target, int32_t index, void* mi) {
    if (!Original_HandleAttackInternal) return;
    const int hits = s_active ? MultiHitCount(SideFor(self), s_config) : 1;
    for (int i = 0; i < hits; ++i) Original_HandleAttackInternal(self, target, index, mi);
}

template <typename T>
void Install(const char* ns, const char* klass, const char* method, int argc,
             const char* owner, T detour, LPVOID* original) {
    void* target = ResolveMethodOrFallback("Assembly-CSharp", ns, klass, method, argc);
    if (!target) { LOG("[RUNTIME] %s unavailable", method); return; }
    const auto address = reinterpret_cast<std::uintptr_t>(target);
    if (!GlobalHookRegistry().Claim(address, owner)) {
        LOG("[RUNTIME] %s conflict owner=%s", method, owner);
        return;
    }
    GlobalHookRegistry().MarkResolved(address);
    MH_STATUS status = MH_CreateHook(target, detour, original);
    if (status == MH_OK) status = MH_EnableHook(target);
    if (status == MH_OK) {
        GlobalHookRegistry().MarkHooked(address);
        LOG("[RUNTIME] %s hooked @ %p", method, target);
    } else {
        GlobalHookRegistry().MarkUnavailable(address);
        LOG("[RUNTIME] %s hook failed status=%d", method, status);
    }
}
}

CombatRuntimeFeature::CombatRuntimeFeature() { name = "CombatRuntime"; enabled = false; }

void CombatRuntimeFeature::Init() {
    GlobalConfigRegistry().RegisterFloat("combat.turn_interval_multiplier", &m_attackSpeedMult);
    GlobalConfigRegistry().RegisterInteger("combat.multi_hit", &m_multiHit);
    GlobalConfigRegistry().RegisterBool("combat.freeze_enemies", &m_freezeEnemies);
    GlobalConfigRegistry().RegisterBool("combat.dumb_enemies", &m_dumbEnemies);
    GlobalConfigRegistry().RegisterBool("entity_manager.enabled", &m_entityManager);
    GlobalConfigRegistry().RegisterInteger("combat.local_side", &m_playerSide);

    Resolved_GetArmySide = reinterpret_cast<IntUnit_t>(ResolveMethodOrFallback(
        "Assembly-CSharp", "AutoChess.CoreGameplay.Fight.Units", "UnitCore", "get_ArmySide", 0));
    Resolved_GetGrade = reinterpret_cast<IntUnit_t>(ResolveMethodOrFallback(
        "Assembly-CSharp", "AutoChess.CoreGameplay.Fight.Units", "UnitCore", "get_grade", 0));
    Resolved_GetTargetUnit = reinterpret_cast<TargetUnit_t>(ResolveMethodOrFallback(
        "Assembly-CSharp", "AutoChess.CoreGameplay.Fight.Units", "BattleUnit", "GetTargetUnit", 0));
    Resolved_ToString = reinterpret_cast<ToString_t>(ResolveMethodOrFallback(
        "Assembly-CSharp", "AutoChess.CoreGameplay.Fight.Units", "BattleUnit", "ToString", 0));
    Resolved_GetData = reinterpret_cast<ObjectUnit_t>(ResolveMethodOrFallback(
        "Assembly-CSharp", "AutoChess.CoreGameplay.Fight.Units", "BattleUnit", "get_Data", 0));
    Resolved_GetMonsterId = reinterpret_cast<IntUnit_t>(ResolveMethodOrFallback(
        "Assembly-CSharp", "AutoChess.CoreGameplay.Participant", "HandMonster", "get_monsterId", 0));
    Resolved_GetMonsterName = reinterpret_cast<ToString_t>(ResolveMethodOrFallback(
        "Assembly-CSharp", "AutoChess.CoreGameplay.Participant", "HandMonster", "GetName", 0));
    if (!Resolved_GetArmySide)
        Resolved_ArmySideField = ResolveFieldOffset("Assembly-CSharp",
            "AutoChess.CoreGameplay.Fight.Units", "BattleUnit", "side");
    LOG("[RUNTIME] identity data=%p monsterId=%p name=%p sideField=%d",
        Resolved_GetData, Resolved_GetMonsterId, Resolved_GetMonsterName, Resolved_ArmySideField);

    Install("AutoChess.CoreGameplay.Fight", "FieldController", "StartBattle", 2,
        "combat-runtime.lifecycle", &StartBattleHook, reinterpret_cast<LPVOID*>(&Original_StartBattle));
    Install("AutoChess.CoreGameplay.Fight", "FieldController", "GetAllUnits", 0,
        "combat-runtime.snapshot", &GetAllUnitsHook, reinterpret_cast<LPVOID*>(&Original_GetAllUnits));
    Install("AutoChess.CoreGameplay.Fight", "FieldController", "BattleEnded", 0,
        "combat-runtime.lifecycle", &BattleEndedHook, reinterpret_cast<LPVOID*>(&Original_BattleEnded));
    Install("AutoChess.CoreGameplay.Fight.Units", "BattleUnit", "Dispose", 0,
        "combat-runtime.dispose", &DisposeHook, reinterpret_cast<LPVOID*>(&Original_Dispose));
    Install("AutoChess.CoreGameplay.Fight.Units", "BattleUnit", "IsNotTimeFrozen", 0,
        "freeze-enemy", &IsNotTimeFrozenHook, reinterpret_cast<LPVOID*>(&Original_IsNotTimeFrozen));
    Install("AutoChess.CoreGameplay.Fight.Units", "BattleUnit", "AttackActivation", 1,
        "dumb-enemy", &AttackActivationHook, reinterpret_cast<LPVOID*>(&Original_AttackActivation));
    Install("AutoChess.CoreGameplay.Fight.Units", "BattleUnit", "HandleAttackInternal", 2,
        "multi-hit", &HandleAttackInternalHook, reinterpret_cast<LPVOID*>(&Original_HandleAttackInternal));
}

void CombatRuntimeFeature::OnUpdate() {
    s_active = enabled;
    s_config.attackSpeedMultiplier = m_attackSpeedMult;
    s_config.multiHit = m_multiHit;
    s_config.freezeEnemies = enabled && m_freezeEnemies;
    s_config.dumbEnemies = enabled && m_dumbEnemies;
    s_localArmySide = (m_playerSide == 2) ? 2 : 1;
    SetRuntimeLocalArmySide(s_localArmySide);
}

void CombatRuntimeFeature::OnMenu() {
    if (!enabled) return;
    ImGui::SliderFloat("Turn interval multiplier (player)", &m_attackSpeedMult, 0.1f, 20.0f, "%.1fx");
    ImGui::SliderInt("Multi-hit (player)", &m_multiHit, 1, 5);
    ImGui::Checkbox("Freeze enemy time", &m_freezeEnemies);
    ImGui::Checkbox("Dumb enemy attacks", &m_dumbEnemies);
    ImGui::Checkbox("Entity Manager", &m_entityManager);
    const auto units = GlobalCombatRuntime().Snapshot();
    ImGui::Text("Runtime units: %zu", units.size());
    ImGui::Text("Session: %llu", static_cast<unsigned long long>(GlobalCombatRuntime().SessionId()));
    if (!m_entityManager) return;
    ImGui::InputText("Entity search", s_search, sizeof(s_search));
    if (ImGui::Button("Export entity snapshot")) {
        std::ofstream file("el_native_entity_snapshot.txt", std::ios::trunc);
        for (const auto& unit : units) {
            file << unit.pointer << '|' << unit.generation << '|' << unit.monsterUid << '|'
                << unit.name << '|' << unit.grade << '|' << static_cast<int>(unit.side) << '|'
                << unit.hp << '|' << unit.mana << '|' << unit.target << '|'
                << (unit.alive ? 1 : 0) << '|' << unit.callCount << '\n';
        }
    }
    ImGui::SeparatorText("Entity Manager");
    ImGui::Columns(8, "entity_columns", false);
    ImGui::Text("Character"); ImGui::NextColumn();
    ImGui::Text("UID"); ImGui::NextColumn();
    ImGui::Text("Side"); ImGui::NextColumn();
    ImGui::Text("Grade"); ImGui::NextColumn();
    ImGui::Text("HP/Mana"); ImGui::NextColumn();
    ImGui::Text("Target"); ImGui::NextColumn();
    ImGui::Text("Alive"); ImGui::NextColumn();
    ImGui::Text("Calls"); ImGui::NextColumn();
    ImGui::Separator();
    for (const auto& unit : units) {
        if (s_search[0] && unit.name.find(s_search) == std::string::npos) continue;
        ImGui::Text("%s", unit.name.c_str()); ImGui::NextColumn();
        ImGui::Text("%d", unit.monsterUid); ImGui::NextColumn();
        ImGui::Text("%s", unit.side == ArmySide::Player ? "player" : unit.side == ArmySide::Enemy ? "enemy" : "unknown"); ImGui::NextColumn();
        ImGui::Text("%d", unit.grade); ImGui::NextColumn();
        ImGui::Text("%.1f / %.1f", unit.hp, unit.mana); ImGui::NextColumn();
        ImGui::Text("0x%llX", static_cast<unsigned long long>(unit.target)); ImGui::NextColumn();
        ImGui::Text("%s", unit.alive ? "yes" : "no"); ImGui::NextColumn();
        ImGui::Text("%u", unit.callCount); ImGui::NextColumn();
    }
    ImGui::Columns(1);
}

static CombatRuntimeFeature g_combatRuntime;
static int g_combatRuntimeRegistered = (RegisterFeature(&g_combatRuntime), 0);
