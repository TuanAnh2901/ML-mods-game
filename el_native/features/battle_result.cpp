#include "battle_result.h"
#include "../framework.h"
#include "../il2cpp_resolve.h"
#include "../../minhook/include/MinHook.h"
#include "imgui.h"
#include <cstring>

// Phase 1: ConvertWinningSideToBattleResult — force local result to "Win"
typedef int32_t(__fastcall* ConvertWinningSideToBattleResult_t)(int32_t winningSide, int32_t playerSide, void* mi);
static ConvertWinningSideToBattleResult_t Original_ConvertWinningSideToBattleResult = nullptr;

// Phase 2: ApplyBattleResult — server sends back result, we override winnerId
// LocalMatchData::ApplyBattleResult(string battleId, string winnerId, List<string> unitUids)
typedef void(__fastcall* ApplyBattleResult_t)(void* self, void* battleId, void* winnerId, void* unitUids, void* mi);
static ApplyBattleResult_t Original_ApplyBattleResult = nullptr;

// Phase 3: Capture localPlayerId from LocalPhaseController constructor
static char s_localPlayerId[256] = {0};
static bool s_forceWin = false;

static int32_t __fastcall ConvertWinningSideToBattleResultHook(int32_t winningSide, int32_t playerSide, void* mi) {
    if (!Original_ConvertWinningSideToBattleResult) return 0;
    if (!s_forceWin)
        return Original_ConvertWinningSideToBattleResult(winningSide, playerSide, mi);
    return Original_ConvertWinningSideToBattleResult(playerSide, playerSide, mi);
}

// Il2CppString layout: first 8 bytes = vtable/klass ptr, then int32 length, then wchar chars
static const char* ReadIl2CppString(void* strPtr) {
    if (!strPtr) return nullptr;
    // Offset 8 = length (int32), Offset 12 = first char (wchar)
    int32_t len = *(int32_t*)((char*)strPtr + 8);
    if (len <= 0 || len > 128) return nullptr;
    // Simple ASCII extraction from wchar (first byte of each wchar)
    static char buf[256];
    wchar_t* wstr = (wchar_t*)((char*)strPtr + 12);
    int i;
    for (i = 0; i < len && i < 255; i++)
        buf[i] = (char)(wstr[i] & 0xFF);
    buf[i] = 0;
    return buf;
}

static void __fastcall ApplyBattleResultHook(void* self, void* battleId, void* winnerId, void* unitUids, void* mi) {
    if (!Original_ApplyBattleResult) return;
    if (s_forceWin && s_localPlayerId[0]) {
        const char* winner = ReadIl2CppString(winnerId);
        if (winner && strcmp(winner, s_localPlayerId) != 0) {
            LOG("[FEATURE] BattleResult: server winner=%s, forcing to %s", winner, s_localPlayerId);
            // Replace winnerId string content with s_localPlayerId
            // We can't modify the Il2CppString directly (it's interned),
            // so we rely on ConvertWinningSideToBattleResult to have already
            // sent "win" to the server.
        }
    }
    Original_ApplyBattleResult(self, battleId, winnerId, unitUids, mi);
}

// Capture localPlayerId from LocalPhaseController::.ctor
// Signature: .ctor(IModeDataProvider, LocalMatchTimer, LocalMatchData,
//                  ILocalServerEmulator, ILocalServerAiDecider,
//                  string localPlayerId, bool testMode)
// x64: RCX=self, RDX,R8,R9=[0-2], stack+0x20=emu, +0x28=ai, +0x30=localPlayerId
typedef void(__fastcall* PhaseControllerCtor_t)(void* self, void* md, void* mt, void* lmd,
    void* emu, void* ai, void* pid, bool testMode, void* mi);
static PhaseControllerCtor_t Original_PhaseControllerCtor = nullptr;

static void __fastcall PhaseControllerCtorHook(void* self, void* md, void* mt, void* lmd,
    void* emu, void* ai, void* pid, bool testMode, void* mi) {
    const char* playerId = ReadIl2CppString(pid);
    if (playerId && playerId[0]) {
        strncpy_s(s_localPlayerId, playerId, sizeof(s_localPlayerId) - 1);
        LOG("[FEATURE] BattleResult: captured localPlayerId=%s", s_localPlayerId);
    }
    Original_PhaseControllerCtor(self, md, mt, lmd, emu, ai, pid, testMode, mi);
}

BattleResultFeature::BattleResultFeature() { name = "BattleResult"; enabled = false; }

void BattleResultFeature::Init() {
    // Hook 1: ConvertWinningSideToBattleResult
    void* fn1 = ResolveMethodOrFallback("Assembly-CSharp",
        "NewAssets.Scripts.UtilScripts", "MyUtil",
        "ConvertWinningSideToBattleResult", 2);
    LOG("[FEATURE] BattleResult: ConvertWinningSide=%p", fn1);
    if (fn1 && MH_CreateHook(fn1, &ConvertWinningSideToBattleResultHook,
        (LPVOID*)&Original_ConvertWinningSideToBattleResult) == MH_OK &&
        MH_EnableHook(fn1) == MH_OK)
        LOG("[FEATURE] BattleResult: hooked ConvertWinningSide @ %p", fn1);
    else LOG("[FEATURE] BattleResult: ConvertWinningSide fail");

    // Hook 2: ApplyBattleResult
    void* fn2 = ResolveMethodOrFallback("Assembly-CSharp",
        "AutoChess.LocalServer", "LocalMatchData",
        "ApplyBattleResult", 3);
    LOG("[FEATURE] BattleResult: ApplyBattleResult=%p", fn2);
    if (fn2 && MH_CreateHook(fn2, &ApplyBattleResultHook,
        (LPVOID*)&Original_ApplyBattleResult) == MH_OK &&
        MH_EnableHook(fn2) == MH_OK)
        LOG("[FEATURE] BattleResult: hooked ApplyBattleResult @ %p", fn2);
    else LOG("[FEATURE] BattleResult: ApplyBattleResult fail");

    // Hook 3: Capture localPlayerId from LocalPhaseController constructor
    void* fn3 = ResolveMethodOrFallback("Assembly-CSharp",
        "AutoChess.LocalServer", "LocalPhaseController",
        ".ctor", 7);
    LOG("[FEATURE] BattleResult: PhaseController.ctor=%p", fn3);
    if (fn3 && MH_CreateHook(fn3, &PhaseControllerCtorHook,
        (LPVOID*)&Original_PhaseControllerCtor) == MH_OK &&
        MH_EnableHook(fn3) == MH_OK)
        LOG("[FEATURE] BattleResult: hooked PhaseController.ctor @ %p", fn3);
    else LOG("[FEATURE] BattleResult: PhaseController.ctor fail (OK if PvE mode)");
}

void BattleResultFeature::OnUpdate() {
    s_forceWin = enabled && m_forceWin && Original_ConvertWinningSideToBattleResult != nullptr;
}

void BattleResultFeature::OnMenu() {
    if (!enabled) return;
    if (!Original_ConvertWinningSideToBattleResult) {
        ImGui::TextColored(ImVec4(1, 0, 0, 1), "hook unavailable"); return;
    }
    ImGui::Checkbox("Force Win (all modes)", &m_forceWin);
    if (s_localPlayerId[0])
        ImGui::Text("Player ID: %s", s_localPlayerId);
    if (m_forceWin)
        ImGui::TextColored(ImVec4(0, 1, 0, 1), "All battles = Win");
}

static BattleResultFeature g_br;
static int g_brReg = (RegisterFeature(&g_br), 0);
