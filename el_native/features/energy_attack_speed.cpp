#include "energy_attack_speed.h"
#include "../framework.h"
#include "../il2cpp_resolve.h"
#include "../../minhook/include/MinHook.h"
#include "imgui.h"
#include <cstdint>

// Energy/AttackSpeed feature — side-locked multipliers.
//
// Attack Speed: hooks BattleUnit::GetCurrentValue(StatType) at 0xE4D400.
//   When StatType == 9 (attack speed from Tracer dump), multiply return value.
//   GetTurnInterval (0xE4DB20) was also hooked but Tracer shows it's never called.
//
// Energy (Mana): hooks BattleUnit::ChangeMana(float) at 0xE4C130.
//   Multiply delta for player side only.

typedef void(__fastcall* ChangeMana_t)(void* self, float delta, void* mi);
typedef float(__fastcall* GetCurrentValue_t)(void* self, int32_t stat, void* mi);
typedef int32_t(__fastcall* GetArmySide_t)(void* self, void* mi);

static ChangeMana_t Original_ChangeMana = nullptr;
static GetCurrentValue_t Original_GetCurrentValue = nullptr;
static GetArmySide_t Resolved_GetArmySide = nullptr;

static bool s_active = false;
static float s_energyMult = 1.0f;
static float s_attackSpeedMult = 1.0f;
static int s_playerSide = 1;
static int s_statAttackSpeed = 9;  // StatType from Tracer dump
static int s_manaCallCount = 0;
static int s_speedCallCount = 0;

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
    int side = Resolved_GetArmySide(self, nullptr);
    if (side == s_playerSide)
        delta *= s_energyMult;
    Original_ChangeMana(self, delta, mi);
}

static float __fastcall GetCurrentValueHook(void* self, int32_t stat, void* mi) {
    float val = Original_GetCurrentValue(self, stat, mi);
    if (!s_active || !Resolved_GetArmySide || stat != s_statAttackSpeed)
        return val;
    int side = Resolved_GetArmySide(self, nullptr);
    if (side == s_playerSide && s_attackSpeedMult > 0.0f) {
        if (s_speedCallCount == 0) {
            LOG("[FEATURE] EnergyAttackSpeed: AttackSpeed stat=%d val=%.2f mult=%.1f", stat, val, s_attackSpeedMult);
            s_speedCallCount = 1;
        }
        return val * s_attackSpeedMult;
    }
    return val;
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

    void* getCurrentValue = ResolveMethodOrFallback(
        "Assembly-CSharp", "AutoChess.CoreGameplay.Fight.Units",
        "BattleUnit", "GetCurrentValue", 1);

    LOG("[FEATURE] EnergyAttackSpeed: ChangeMana=%p GetCurrentValue=%p get_ArmySide=%p",
        changeMana, getCurrentValue, Resolved_GetArmySide);

    if (changeMana && MH_CreateHook(changeMana, &ChangeManaHook,
        (LPVOID*)&Original_ChangeMana) == MH_OK && MH_EnableHook(changeMana) == MH_OK)
        LOG("[FEATURE] EnergyAttackSpeed: ChangeMana hooked @ %p", changeMana);
    else LOG("[FEATURE] EnergyAttackSpeed: ChangeMana fail");

    if (getCurrentValue && MH_CreateHook(getCurrentValue, &GetCurrentValueHook,
        (LPVOID*)&Original_GetCurrentValue) == MH_OK && MH_EnableHook(getCurrentValue) == MH_OK)
        LOG("[FEATURE] EnergyAttackSpeed: GetCurrentValue hooked @ %p (stat=%d = atk speed)", getCurrentValue, s_statAttackSpeed);
    else LOG("[FEATURE] EnergyAttackSpeed: GetCurrentValue fail");
}

void EnergyAttackSpeedFeature::OnUpdate() {
    s_active = enabled && Original_GetCurrentValue != nullptr;
    s_energyMult = m_energyMult;
    s_attackSpeedMult = m_attackSpeedMult;
    s_playerSide = m_playerSide;
    s_statAttackSpeed = m_statAttackSpeed;
}

void EnergyAttackSpeedFeature::OnMenu() {
    if (!enabled) return;
    if (!Original_GetCurrentValue) {
        ImGui::TextColored(ImVec4(1, 0, 0, 1), "hook unavailable"); return;
    }
    ImGui::SliderFloat("Energy Mult (player only)", &m_energyMult, 0.0f, 100.0f, "%.1fx");
    ImGui::SliderFloat("Attack Speed Mult (player only)", &m_attackSpeedMult, 0.1f, 100.0f, "%.1fx");
    ImGui::InputInt("Player Side ID", &m_playerSide);
    ImGui::InputInt("StatType for AtkSpeed", &m_statAttackSpeed);
    ImGui::Text("Only units with side==%d, statType==%d get multipliers", m_playerSide, m_statAttackSpeed);
    ImGui::Text("Mana: %d  Speed: %d calls", s_manaCallCount, s_speedCallCount);
}

static EnergyAttackSpeedFeature g_energyAttackSpeed;
static int g_registered = (RegisterFeature(&g_energyAttackSpeed), 0);
