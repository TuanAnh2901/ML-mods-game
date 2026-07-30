#include "battle_combat.h"
#include "../framework.h"
#include "../il2cpp_resolve.h"
#include "../../minhook/include/MinHook.h"
#include "imgui.h"
#include <cstdint>

// Battle Combat — S-tier method hooks with side detection.
//
// Heal(IFieldElement source, float healAmount) at 0xE4EEC0
//   Multiply heal amount for player side.
//
// CheckDeath(IFieldElement source, bool dispose) → bool at 0xE4CB50
//   Return false for player side → units never die (GodMode v2, single hook).
//
// GetOffenseAmplify(DamageType type) → float at 0xE4D760
//   Multiply offense amplification % for player side → more damage output.
//
// GetProtectionAmplify(DamageType type) → float at 0xE4D880
//   Multiply defense amplification % for player side → less damage taken.

typedef void(__fastcall* Heal_t)(void* self, void* source, float healAmount, void* mi);
typedef bool(__fastcall* CheckDeath_t)(void* self, void* source, bool dispose, void* mi);
typedef float(__fastcall* GetAmplify_t)(void* self, int32_t dmgType, void* mi);
typedef int32_t(__fastcall* GetArmySide_t)(void* self, void* mi);

static Heal_t Original_Heal = nullptr;
static CheckDeath_t Original_CheckDeath = nullptr;
static GetAmplify_t Original_GetOffenseAmplify = nullptr;
static GetAmplify_t Original_GetProtectionAmplify = nullptr;
static GetArmySide_t Resolved_Side = nullptr;

static bool s_active = false;
static bool s_godMode = false;
static float s_healMult = 1.0f;
static float s_offenseMult = 1.0f;
static float s_defenseMult = 1.0f;
static int s_playerSide = 1;

static void __fastcall HealHook(void* self, void* source, float healAmount, void* mi) {
    if (s_active && Resolved_Side) {
        int side = Resolved_Side(self, nullptr);
        if (side == s_playerSide) healAmount *= s_healMult;
    }
    Original_Heal(self, source, healAmount, mi);
}

static bool __fastcall CheckDeathHook(void* self, void* source, bool dispose, void* mi) {
    if (s_godMode && Resolved_Side) {
        int side = Resolved_Side(self, nullptr);
        if (side == s_playerSide) return false; // never die
    }
    return Original_CheckDeath(self, source, dispose, mi);
}

static float __fastcall GetOffenseAmplifyHook(void* self, int32_t dmgType, void* mi) {
    float val = Original_GetOffenseAmplify(self, dmgType, mi);
    if (s_active && Resolved_Side && s_offenseMult != 1.0f) {
        int side = Resolved_Side(self, nullptr);
        if (side == s_playerSide) val *= s_offenseMult;
    }
    return val;
}

static float __fastcall GetProtectionAmplifyHook(void* self, int32_t dmgType, void* mi) {
    float val = Original_GetProtectionAmplify(self, dmgType, mi);
    if (s_active && Resolved_Side && s_defenseMult != 1.0f) {
        int side = Resolved_Side(self, nullptr);
        if (side == s_playerSide) val *= s_defenseMult;
    }
    return val;
}

BattleCombatFeature::BattleCombatFeature() { name = "Combat"; enabled = false; }

void BattleCombatFeature::Init() {
    Resolved_Side = (GetArmySide_t)ResolveMethodOrFallback(
        "Assembly-CSharp", "AutoChess.CoreGameplay.Fight.Units",
        "UnitCore", "get_ArmySide", 0);

    auto h = [](const char* m, int ac, LPVOID* orig, LPVOID func) {
        void* fn = ResolveMethodOrFallback("Assembly-CSharp",
            "AutoChess.CoreGameplay.Fight.Units", "BattleUnit", m, ac);
        if (fn && MH_CreateHook(fn, func, orig) == MH_OK && MH_EnableHook(fn) == MH_OK)
            LOG("[FEATURE] Combat: hooked %s @ %p", m, fn);
        else LOG("[FEATURE] Combat: %s fail (RVA may be shared)", m);
    };

    h("Heal", 2, (LPVOID*)&Original_Heal, &HealHook);
    h("CheckDeath", 2, (LPVOID*)&Original_CheckDeath, &CheckDeathHook);
    h("GetOffenseAmplify", 1, (LPVOID*)&Original_GetOffenseAmplify, &GetOffenseAmplifyHook);
    h("GetProtectionAmplify", 1, (LPVOID*)&Original_GetProtectionAmplify, &GetProtectionAmplifyHook);

    LOG("[FEATURE] Combat: get_ArmySide=%p init done", Resolved_Side);
}

void BattleCombatFeature::OnUpdate() {
    s_active = enabled && Resolved_Side != nullptr;
    s_godMode = enabled && m_godMode;
    s_healMult = enabled ? m_healMult : 1.0f;
    s_offenseMult = enabled ? m_offenseMult : 1.0f;
    s_defenseMult = enabled ? m_defenseMult : 1.0f;
    s_playerSide = m_playerSide;
}

void BattleCombatFeature::OnMenu() {
    if (!enabled) return;
    if (!Resolved_Side) { ImGui::TextColored(ImVec4(1, 0, 0, 1), "get_ArmySide unresolved"); return; }

    ImGui::Checkbox("God Mode (prevent death)", &m_godMode);
    ImGui::SliderFloat("Heal Mult", &m_healMult, 0.0f, 100.0f, "%.1fx");
    ImGui::SliderFloat("Offense Amplify", &m_offenseMult, 0.0f, 10.0f, "%.1fx");
    ImGui::SliderFloat("Defense Amplify", &m_defenseMult, 0.0f, 10.0f, "%.1fx");
    ImGui::InputInt("Player Side ID", &m_playerSide);
    if (m_godMode)
        ImGui::TextColored(ImVec4(0, 1, 0, 1), "Player units cannot die");
}

static BattleCombatFeature g_combat;
static int g_combatReg = (RegisterFeature(&g_combat), 0);
