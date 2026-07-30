#include "energy_attack_speed.h"
#include "../framework.h"
#include "../il2cpp_resolve.h"
#include "../../minhook/include/MinHook.h"
#include "imgui.h"
#include <cstdint>

typedef void(__fastcall* ChangeMana_t)(void* self, float delta, void* methodInfo);
typedef float(__fastcall* GetTurnInterval_t)(void* self, void* methodInfo);

static ChangeMana_t Original_ChangeMana = nullptr;
static GetTurnInterval_t Original_GetTurnInterval = nullptr;

static bool s_active = false;
static float s_energyMult = 1.0f;
static float s_attackSpeedMult = 1.0f;

static void __fastcall ChangeManaHook(void* self, float delta, void* methodInfo) {
    if (!Original_ChangeMana) return;
    if (!s_active) {
        Original_ChangeMana(self, delta, methodInfo);
        return;
    }
    Original_ChangeMana(self, delta * s_energyMult, methodInfo);
}

static float __fastcall GetTurnIntervalHook(void* self, void* methodInfo) {
    if (!Original_GetTurnInterval) return 1.0f;
    if (!s_active) return Original_GetTurnInterval(self, methodInfo);
    return Original_GetTurnInterval(self, methodInfo) / s_attackSpeedMult;
}

EnergyAttackSpeedFeature::EnergyAttackSpeedFeature() {
    name = "EnergyAttackSpeed";
    enabled = false;
}

void EnergyAttackSpeedFeature::Init() {
    void* changeMana = ResolveMethodOrFallback(
        "Assembly-CSharp", "AutoChess.CoreGameplay.Fight.Units",
        "BattleUnit", "ChangeMana", 1);

    void* getTurnInterval = ResolveMethodOrFallback(
        "Assembly-CSharp", "AutoChess.CoreGameplay.Fight.Units",
        "BattleUnit", "GetTurnInterval", 0);

    LOG("[FEATURE] EnergyAttackSpeed: ChangeMana=%p GetTurnInterval=%p",
        changeMana, getTurnInterval);

    if (changeMana) {
        if (MH_CreateHook(changeMana, &ChangeManaHook,
                          (LPVOID*)&Original_ChangeMana) != MH_OK ||
            MH_EnableHook(changeMana) != MH_OK) {
            Original_ChangeMana = nullptr;
            LOG("[FEATURE] EnergyAttackSpeed: ChangeMana hook failed");
        } else {
            LOG("[FEATURE] EnergyAttackSpeed: ChangeMana hooked @ %p", changeMana);
        }
    }

    if (getTurnInterval) {
        if (MH_CreateHook(getTurnInterval, &GetTurnIntervalHook,
                          (LPVOID*)&Original_GetTurnInterval) != MH_OK ||
            MH_EnableHook(getTurnInterval) != MH_OK) {
            Original_GetTurnInterval = nullptr;
            LOG("[FEATURE] EnergyAttackSpeed: GetTurnInterval hook failed");
        } else {
            LOG("[FEATURE] EnergyAttackSpeed: GetTurnInterval hooked @ %p", getTurnInterval);
        }
    }
}

void EnergyAttackSpeedFeature::OnUpdate() {
    s_active = enabled && Original_ChangeMana && Original_GetTurnInterval;
    s_energyMult = m_energyMult;
    s_attackSpeedMult = m_attackSpeedMult;
}

void EnergyAttackSpeedFeature::OnMenu() {
    if (!enabled) return;
    if (!Original_ChangeMana || !Original_GetTurnInterval) {
        ImGui::TextColored(ImVec4(1, 0, 0, 1), "hook unavailable");
        return;
    }
    ImGui::SliderFloat("Energy Mult", &m_energyMult, 0.0f, 100.0f, "%.1fx");
    ImGui::SliderFloat("Attack Speed Mult", &m_attackSpeedMult, 0.1f, 100.0f, "%.1fx");
}

static EnergyAttackSpeedFeature g_energyAttackSpeed;
static int g_registered = (RegisterFeature(&g_energyAttackSpeed), 0);
