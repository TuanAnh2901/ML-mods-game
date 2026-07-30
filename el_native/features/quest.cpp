#include "quest.h"
#include "../framework.h"
#include "../il2cpp_resolve.h"
#include "../../minhook/include/MinHook.h"
#include "imgui.h"
#include <cstdint>

// Daily Quest auto-complete v2 — hook get_IsCompleted instead of get_sufficientProgress.
//
// v1 problem: get_sufficientProgress returns ObscuredInt (16 bytes → RAX:RDX),
//   which can't be modified via a void-return hook. The hidden-ptr assumption
//   was wrong for 16-byte structs in x64.
//
// v2 fix: hook get_IsCompleted which returns ObscuredBool (4 bytes → EAX).
//   ObscuredBool layout: byte0=cryptoKey, byte1=hiddenValue, byte2=fakeValue, byte3=inited.
//   actual = hiddenValue XOR cryptoKey. Force hiddenValue = cryptoKey ^ 1 → actual=true.

typedef int32_t(__fastcall* GetIsCompleted_t)(void* self, void* mi);
static GetIsCompleted_t Original_GetIsCompleted = nullptr;

static bool s_autoComplete = false;
static float s_rewardMult = 1.0f;

static int32_t __fastcall GetIsCompletedHook(void* self, void* mi) {
    int32_t result = Original_GetIsCompleted(self, mi);
    if (!s_autoComplete) return result;
    uint8_t cryptoKey = (uint8_t)(result & 0xFF);
    uint8_t hiddenTrue = cryptoKey ^ 1; // XOR 1 → actual bool = true
    result = (result & 0xFFFF0000) | ((uint32_t)hiddenTrue << 8) | cryptoKey;
    return result;
}

DailyQuestFeature::DailyQuestFeature() { name = "Quests"; enabled = false; }

void DailyQuestFeature::Init() {
    void* fn = ResolveMethodOrFallback("Assembly-CSharp",
        "QuestScripts", "QuestSaveData", "get_IsCompleted", 0);
    LOG("[FEATURE] Quests: get_IsCompleted=%p", fn);
    if (fn && MH_CreateHook(fn, &GetIsCompletedHook, (LPVOID*)&Original_GetIsCompleted) == MH_OK && MH_EnableHook(fn) == MH_OK)
        LOG("[FEATURE] Quests: auto-complete hooked @ %p", fn);
    else LOG("[FEATURE] Quests: hook failed");
}

void DailyQuestFeature::OnUpdate() {
    s_autoComplete = enabled && m_autoComplete && Original_GetIsCompleted != nullptr;
    s_rewardMult = enabled ? m_rewardMult : 1.0f;
}

void DailyQuestFeature::OnMenu() {
    if (!enabled) return;
    if (!Original_GetIsCompleted) {
        ImGui::TextColored(ImVec4(1, 0, 0, 1), "hook unavailable");
        return;
    }
    ImGui::Checkbox("Auto-complete all quests", &m_autoComplete);
    ImGui::SliderFloat("Reward multiplier", &m_rewardMult, 1.0f, 100.0f, "%.1fx");
    if (m_autoComplete)
        ImGui::TextColored(ImVec4(0, 1, 0, 1), "All quests auto-completed (IsCompleted forced true)");
}

static DailyQuestFeature g_quest;
static int g_questReg = (RegisterFeature(&g_quest), 0);
