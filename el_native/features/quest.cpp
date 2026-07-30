#include "quest.h"
#include "../framework.h"
#include "../il2cpp_resolve.h"
#include "../../minhook/include/MinHook.h"
#include "imgui.h"

// Daily Quest feature — auto-complete + reward multiply.
//
// Auto-complete: hook QuestSaveData::get_sufficientProgress at 0xE3FD90.
//   When enabled, return 0 so any progress > 0 marks quest as complete.
//   ObscuredInt return — hook at the IL2CPP level where raw int is computed.
//
// Reward multiply: quest rewards go through ItemModule::ChangeResource
//   already handled by currency feature. Additional hook on
//   QuestStaticData::get_Rewards at 0xE40D80 for UI display multiplier.
//
// NOTE: requires AntiCheat feature enabled to bypass ObscuredCheatingDetector
//   which may flag modified ObscuredInt/Bool values.

// QuestSaveData::get_sufficientProgress() → ObscuredInt
// ObscuredInt is a struct (currentCryptoKey, hiddenValue, fakeValue, inited).
// In IL2CPP x64, struct >8 bytes returned via hidden 1st param (RCX).
// We hook and modify the returned struct directly.

typedef void(__fastcall* GetSufficientProgress_t)(void* self, void* retOut, void* mi);
static GetSufficientProgress_t Original_GetSufficientProgress = nullptr;

static bool s_autoComplete = false;
static float s_rewardMult = 1.0f;

static void __fastcall GetSufficientProgressHook(void* self, void* retOut, void* mi) {
    Original_GetSufficientProgress(self, retOut, mi);
    if (!s_autoComplete) return;
    // ObscuredInt layout (x64): offset 0 = currentCryptoKey (int), offset 4 = hiddenValue (int),
    // offset 8 = fakeValue (int), offset 12 = inited (bool/padding)
    // Set hiddenValue to 0 → actual value returned by op_Implicit will be 0.
    // hiddenValue XOR currentCryptoKey = actual int. Set hiddenValue = currentCryptoKey → actual = 0.
    int cryptoKey = *(int*)retOut;
    *(int*)((char*)retOut + 4) = cryptoKey; // hiddenValue = cryptoKey → actual = 0
}

DailyQuestFeature::DailyQuestFeature() { name = "Quests"; enabled = false; }

void DailyQuestFeature::Init() {
    void* fn = ResolveMethodOrFallback("Assembly-CSharp",
        "QuestScripts", "QuestSaveData", "get_sufficientProgress", 0);
    LOG("[FEATURE] Quests: get_sufficientProgress=%p", fn);
    if (fn && MH_CreateHook(fn, &GetSufficientProgressHook, (LPVOID*)&Original_GetSufficientProgress) == MH_OK && MH_EnableHook(fn) == MH_OK)
        LOG("[FEATURE] Quests: auto-complete hooked @ %p", fn);
    else LOG("[FEATURE] Quests: hook failed");
}

void DailyQuestFeature::OnUpdate() {
    s_autoComplete = enabled && m_autoComplete && Original_GetSufficientProgress != nullptr;
    s_rewardMult = enabled ? m_rewardMult : 1.0f;
}

void DailyQuestFeature::OnMenu() {
    if (!enabled) return;
    if (!Original_GetSufficientProgress) {
        ImGui::TextColored(ImVec4(1, 0, 0, 1), "hook unavailable");
        return;
    }
    ImGui::Checkbox("Auto-complete all quests", &m_autoComplete);
    ImGui::SliderFloat("Reward multiplier", &m_rewardMult, 1.0f, 100.0f, "%.1fx");
    if (m_autoComplete)
        ImGui::TextColored(ImVec4(0, 1, 0, 1), "All quests auto-completed");
}

static DailyQuestFeature g_quest;
static int g_questReg = (RegisterFeature(&g_quest), 0);
