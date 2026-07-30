#include "battle_result.h"
#include "../framework.h"
#include "../il2cpp_resolve.h"
#include "../../minhook/include/MinHook.h"
#include "imgui.h"

// NewAssets.Scripts.UtilScripts.MyUtil::ConvertWinningSideToBattleResult
// (ArmySide winningSide, ArmySide playerSide) → BattleResultEnum (int32)
// Converts battle winner + player side → result enum.
// Force: when player would lose, pretend player side won → sends "Win" to server.
typedef int32_t(__fastcall* ConvertWinningSideToBattleResult_t)(int32_t winningSide, int32_t playerSide, void* methodInfo);
static ConvertWinningSideToBattleResult_t Original_ConvertWinningSideToBattleResult = nullptr;
static bool s_forceWin = false;

static int32_t __fastcall ConvertWinningSideToBattleResultHook(int32_t winningSide, int32_t playerSide, void* mi) {
    if (!Original_ConvertWinningSideToBattleResult) return 0;
    if (!s_forceWin)
        return Original_ConvertWinningSideToBattleResult(winningSide, playerSide, mi);
    // Player side = 1. If player would lose (winningSide != playerSide),
    // force the result by pretending winningSide == playerSide.
    return Original_ConvertWinningSideToBattleResult(playerSide, playerSide, mi);
}

BattleResultFeature::BattleResultFeature() { name = "BattleResult"; enabled = false; }

void BattleResultFeature::Init() {
    void* fn = ResolveMethodOrFallback("Assembly-CSharp", "NewAssets.Scripts.UtilScripts", "MyUtil", "ConvertWinningSideToBattleResult", 2);
    LOG("[FEATURE] BattleResult: ConvertWinningSideToBattleResult=%p", fn);
    if (fn && MH_CreateHook(fn, &ConvertWinningSideToBattleResultHook, (LPVOID*)&Original_ConvertWinningSideToBattleResult) == MH_OK && MH_EnableHook(fn) == MH_OK)
        LOG("[FEATURE] BattleResult: hooked @ %p", fn);
    else LOG("[FEATURE] BattleResult: hook failed");
}

void BattleResultFeature::OnUpdate() { s_forceWin = enabled && m_forceWin && Original_ConvertWinningSideToBattleResult != nullptr; }

void BattleResultFeature::OnMenu() {
    if (!enabled) return;
    if (!Original_ConvertWinningSideToBattleResult) { ImGui::TextColored(ImVec4(1, 0, 0, 1), "hook unavailable"); return; }
    ImGui::Checkbox("Force Win (all modes)", &m_forceWin);
}

static BattleResultFeature g_br;
static int g_brReg = (RegisterFeature(&g_br), 0);
