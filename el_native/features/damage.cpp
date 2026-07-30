#include "damage.h"
#include "../framework.h"
#include "../il2cpp_resolve.h"
#include "../../minhook/include/MinHook.h"
#include "imgui.h"
#include <cstdint>

// BattleUnit::ApplyDamage(float, DamageType, IFieldElement, bool, PeriodicAction+Type)
// choke point cho mọi nguồn damage. self = victim, source = attacker (IFieldElement).
typedef bool (__fastcall* ApplyDamage_t)(void* self, float damage, int32_t type,
                                         void* source, bool isMainTarget,
                                         int32_t periodicType, void* methodInfo);
typedef int32_t (__fastcall* GetArmySide_t)(void* instance, void* methodInfo);

static ApplyDamage_t Original_ApplyDamage = nullptr;
static GetArmySide_t Resolved_GetArmySide = nullptr;

// Runtime state (read from hook, displayed in overlay)
static bool s_active = false;
static float s_playerMult = 1.0f;
static float s_enemyMult = 1.0f;
static int s_playerSide = -1;
static int s_godModeSide = -1;
static int s_lastAttackerSide = -2;
static int s_lastVictimSide = -2;
static float s_lastDamage = 0.0f;
static uint32_t s_hitCount = 0;

static bool __fastcall ApplyDamageHook(void* self, float damage, int32_t type,
                                        void* source, bool isMainTarget,
                                        int32_t periodicType, void* methodInfo) {
    if (!Original_ApplyDamage) {
        return false;
    }
    if (!s_active) {
        return Original_ApplyDamage(self, damage, type, source, isMainTarget,
                                     periodicType, methodInfo);
    }

    s_hitCount++;
    s_lastDamage = damage;

    // Read sides
    if (self && Resolved_GetArmySide) {
        s_lastVictimSide = Resolved_GetArmySide(self, nullptr);
    }
    if (source && Resolved_GetArmySide) {
        s_lastAttackerSide = Resolved_GetArmySide(source, nullptr);
    } else {
        s_lastAttackerSide = -2;
    }

    // GodMode: victim side cannot die
    if (s_godModeSide >= 0 && s_lastVictimSide == s_godModeSide) {
        return false; // block death
    }

    // Determine multiplier: if attacker side == playerSide, use playerMult
    float mult = 1.0f;
    if (s_playerSide >= 0 && s_lastAttackerSide == s_playerSide) {
        mult = s_playerMult;
    } else if (s_playerSide >= 0) {
        mult = s_enemyMult;
    }
    // If playerSide is -1 (unknown), apply no multiplier

    return Original_ApplyDamage(self, damage * mult, type, source,
                                 isMainTarget, periodicType, methodInfo);
}

DamageFeature::DamageFeature() {
    name = "Damage";
    enabled = false;
}

void DamageFeature::Init() {
    Resolved_GetArmySide = (GetArmySide_t)ResolveMethodOrFallback(
        "Assembly-CSharp", "AutoChess.CoreGameplay.Fight.Units",
        "UnitCore", "get_ArmySide", 0);

    void* apply = ResolveMethodOrFallback(
        "Assembly-CSharp", "AutoChess.CoreGameplay.Fight.Units",
        "BattleUnit", "ApplyDamage", 5);

    LOG("[FEATURE] Damage: BattleUnit.ApplyDamage=%p get_ArmySide=%p",
        apply, Resolved_GetArmySide);
    if (!apply) {
        LOG("[FEATURE] Damage: ApplyDamage unresolved");
        return;
    }
    if (MH_CreateHook(apply, &ApplyDamageHook,
                      (LPVOID*)&Original_ApplyDamage) != MH_OK ||
        MH_EnableHook(apply) != MH_OK) {
        Original_ApplyDamage = nullptr;
        LOG("[FEATURE] Damage: MinHook failed");
        return;
    }
    LOG("[FEATURE] Damage: hooked BattleUnit.ApplyDamage @ %p", apply);
}

void DamageFeature::OnUpdate() {
    s_active = enabled && Original_ApplyDamage != nullptr;
    s_playerMult = m_playerMult;
    s_enemyMult = m_enemyMult;
    s_playerSide = m_playerSide;
    s_godModeSide = m_godModeSide;
}

void DamageFeature::OnMenu() {
    if (!enabled) return;
    if (!Original_ApplyDamage) {
        ImGui::TextColored(ImVec4(1, 0, 0, 1), "hook unavailable");
        return;
    }

    ImGui::SliderFloat("Player Dmg Mult", &m_playerMult, 0.0f, 100.0f, "%.1fx");
    ImGui::SliderFloat("Enemy Dmg Mult", &m_enemyMult, 0.0f, 100.0f, "%.1fx");

    ImGui::InputInt("Player Side", &m_playerSide);
    ImGui::SameLine();
    if (ImGui::SmallButton("Capture")) {
        m_playerSide = s_lastAttackerSide;
    }

    ImGui::InputInt("GodMode Side", &m_godModeSide);

    ImGui::Text("hits: %u  last dmg: %.1f", s_hitCount, s_lastDamage);
    ImGui::Text("attacker side: %d  victim side: %d",
                s_lastAttackerSide, s_lastVictimSide);

    if (s_playerSide < 0) {
        ImGui::TextColored(ImVec4(1, 1, 0, 1),
            "Tip: enter battle, press Capture after a hit to set PlayerSide");
    }
}

static DamageFeature g_damage;
static int g_damageRegistered = (RegisterFeature(&g_damage), 0);