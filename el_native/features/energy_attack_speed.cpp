#include "energy_attack_speed.h"
#include "../framework.h"
#include "../il2cpp_resolve.h"
#include "../combat_runtime.h"
#include "../../minhook/include/MinHook.h"
#include "imgui.h"
#include <cstdint>
#include <cstdio>

// Energy/AttackSpeed feature — side-locked multipliers.
//
// Attack Speed: hooks BattleUnit::GetTurnInterval() and scales only player units.
//
// Energy (Mana): hooks BattleUnit::ChangeMana(float) at 0xE4C130.
//   Multiply delta for player side only.

typedef void(__fastcall* ChangeMana_t)(void* self, float delta, void* mi);
typedef float(__fastcall* GetTurnInterval_t)(void* self, void* mi);
typedef int32_t(__fastcall* GetArmySide_t)(void* self, void* mi);

static ChangeMana_t Original_ChangeMana = nullptr;
static GetTurnInterval_t Original_GetTurnInterval = nullptr;
static GetArmySide_t Resolved_GetArmySide = nullptr;

static bool s_active = false;
static float s_energyMult = 1.0f;
static float s_attackSpeedMult = 1.0f;
static int s_playerSide = 1;
static int s_statAttackSpeed = 9;  // StatType from Tracer dump
static int s_manaCallCount = 0;
static int s_speedCallCount = 0;
static bool s_trackStats = false;
static int s_compareFrameCounter = 0;

struct StatCompareEntry {
    void* unit;
    int side;
    int32_t stat;
    float baseline;
    float applied;
    int calls;
};
static StatCompareEntry s_statCompare[256] = {};
static int s_statCompareCount = 0;

static int RuntimeSide(void* unit) {
    for (const auto& snapshot : GlobalCombatRuntime().Snapshot()) {
        if (snapshot.pointer == reinterpret_cast<std::uintptr_t>(unit))
            return snapshot.side == ArmySide::Player ? s_playerSide : snapshot.side == ArmySide::Enemy ? 0 : -1;
    }
    if (!Resolved_GetArmySide) return -1;
    const ArmySide side = ArmySideFromRaw(Resolved_GetArmySide(unit, nullptr), RuntimeLocalArmySide());
    return side == ArmySide::Player ? s_playerSide : side == ArmySide::Enemy ? 0 : -1;
}

static void RecordStatCompare(void* unit, int side, int32_t stat, float baseline, float applied) {
    if (!s_trackStats) return;
    for (int i = 0; i < s_statCompareCount; ++i) {
        if (s_statCompare[i].unit == unit && s_statCompare[i].stat == stat) {
            s_statCompare[i].side = side;
            s_statCompare[i].baseline = baseline;
            s_statCompare[i].applied = applied;
            ++s_statCompare[i].calls;
            return;
        }
    }
    if (s_statCompareCount < 256)
        s_statCompare[s_statCompareCount++] = {unit, side, stat, baseline, applied, 1};
}

static void SaveStatCompare() {
    FILE* file = fopen("el_native_stat_compare.txt", "w");
    if (!file) return;
    fprintf(file, "# BattleUnit stat comparison (last observed value)\n");
    fprintf(file, "# Unit | Side | Stat | Calls | Baseline | Applied | Delta\n");
    for (int i = 0; i < s_statCompareCount; ++i) {
        const auto& entry = s_statCompare[i];
        fprintf(file, "%p | %d | %d | %d | %.4f | %.4f | %+.4f\n", entry.unit,
            entry.side, entry.stat, entry.calls, entry.baseline, entry.applied,
            entry.applied - entry.baseline);
    }
    fclose(file);
}

static void __fastcall ChangeManaHook(void* self, float delta, void* mi) {
    if (!Original_ChangeMana) return;
    if (s_manaCallCount == 0) {
        LOG("[FEATURE] EnergyAttackSpeed: ChangeMana first call self=%p delta=%.2f", self, delta);
        s_manaCallCount = 1;
    }
    if (!s_active || !Resolved_GetArmySide) {
        Original_ChangeMana(self, delta, mi);
        return;
    }
    int side = RuntimeSide(self);
    if (side == s_playerSide)
        delta *= s_energyMult;
    Original_ChangeMana(self, delta, mi);
}

static float __fastcall GetTurnIntervalHook(void* self, void* mi) {
    float val = Original_GetTurnInterval(self, mi);
    int side = RuntimeSide(self);
    float applied = val;
    if (s_active && side == s_playerSide) {
        applied = ApplyTurnInterval(val, ArmySide::Player, CombatConfig{s_attackSpeedMult, 1, false, false});
        if (s_speedCallCount == 0) {
            LOG("[FEATURE] EnergyAttackSpeed: TurnInterval baseline=%.2f applied=%.2f mult=%.1f",
                val, applied, s_attackSpeedMult);
            s_speedCallCount = 1;
        }
    }
    RecordStatCompare(self, side, 9, val, applied);
    return applied;
}

EnergyAttackSpeedFeature::EnergyAttackSpeedFeature() {
    name = "EnergyAttackSpeed";
    enabled = false;
}

void EnergyAttackSpeedFeature::Init() {
    Resolved_GetArmySide = (GetArmySide_t)ResolveMethodOrFallback(
        "Assembly-CSharp", "AutoChess.CoreGameplay.Fight.Units",
        "UnitCore", "get_ArmySide", 0);

    void* changeMana = ResolveMethodOrFallback(
        "Assembly-CSharp", "AutoChess.CoreGameplay.Fight.Units",
        "BattleUnit", "ChangeMana", 1);

    void* getTurnInterval = ResolveMethodOrFallback(
        "Assembly-CSharp", "AutoChess.CoreGameplay.Fight.Units",
        "BattleUnit", "GetTurnInterval", 0);

    LOG("[FEATURE] EnergyAttackSpeed: ChangeMana=%p GetTurnInterval=%p get_ArmySide=%p",
        changeMana, getTurnInterval, Resolved_GetArmySide);

    if (changeMana && MH_CreateHook(changeMana, &ChangeManaHook,
        (LPVOID*)&Original_ChangeMana) == MH_OK && MH_EnableHook(changeMana) == MH_OK)
        LOG("[FEATURE] EnergyAttackSpeed: ChangeMana hooked @ %p", changeMana);
    else LOG("[FEATURE] EnergyAttackSpeed: ChangeMana fail");

    if (getTurnInterval && MH_CreateHook(getTurnInterval, &GetTurnIntervalHook,
        (LPVOID*)&Original_GetTurnInterval) == MH_OK && MH_EnableHook(getTurnInterval) == MH_OK)
        LOG("[FEATURE] EnergyAttackSpeed: GetTurnInterval hooked @ %p", getTurnInterval);
    else LOG("[FEATURE] EnergyAttackSpeed: GetTurnInterval fail");
}

void EnergyAttackSpeedFeature::OnUpdate() {
    s_active = enabled && Original_GetTurnInterval != nullptr;
    s_energyMult = m_energyMult;
    s_attackSpeedMult = m_attackSpeedMult;
    s_playerSide = RuntimeLocalArmySide();
    s_statAttackSpeed = m_statAttackSpeed;
    s_trackStats = enabled && m_trackStats && Original_GetTurnInterval != nullptr;
    if (s_trackStats && ++s_compareFrameCounter >= 300) {
        SaveStatCompare();
        s_compareFrameCounter = 0;
    }
}

void EnergyAttackSpeedFeature::OnMenu() {
    if (!enabled) return;
    if (!Original_GetTurnInterval) {
        ImGui::TextColored(ImVec4(1, 0, 0, 1), "hook unavailable"); return;
    }
    ImGui::SliderFloat("Energy Mult (player only)", &m_energyMult, 0.0f, 100.0f, "%.1fx");
    ImGui::SliderFloat("Attack Speed Mult (player only)", &m_attackSpeedMult, 0.1f, 100.0f, "%.1fx");
    ImGui::InputInt("Player Side ID", &m_playerSide);
    ImGui::Checkbox("Track per-character stats", &m_trackStats);
    ImGui::Text("Only player units (side==%d) get turn-interval scaling", m_playerSide);
    ImGui::Text("Mana: %d  Speed: %d calls", s_manaCallCount, s_speedCallCount);
    ImGui::Text("Tracked unit/stat pairs: %d", s_statCompareCount);
    if (m_trackStats) {
        ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1),
            "Writing baseline/applied rows to el_native_stat_compare.txt");
        for (int i = 0; i < s_statCompareCount && i < 12; ++i) {
            const auto& entry = s_statCompare[i];
            ImGui::Text("%p s%d stat%d: %.2f -> %.2f (%d)", entry.unit, entry.side,
                entry.stat, entry.baseline, entry.applied, entry.calls);
        }
    }
}

static EnergyAttackSpeedFeature g_energyAttackSpeed;
static int g_registered = (RegisterFeature(&g_energyAttackSpeed), 0);
