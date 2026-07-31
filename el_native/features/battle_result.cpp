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

// Tournament/session progression consumes ServerParticipant::UpdateStreaks
// after the battle result arrives.  This path is separate from the local
// LocalMatchData converter, so force-win must cover it as well.
typedef void(__fastcall* UpdateStreaks_t)(void* self, int32_t result, void* mi);
typedef bool(__fastcall* IsLocal_t)(void* self, void* mi);
// get_Uid returns Il2CppString* for ServerParticipant — used to match tournament participant to local player
typedef void*(__fastcall* GetUid_t)(void* self, void* mi);
static UpdateStreaks_t Original_UpdateStreaks = nullptr;
static IsLocal_t Original_IsLocal = nullptr;
static GetUid_t Original_GetUid = nullptr;
static int32_t s_forcedResult = -1;
static bool s_localBattleHooked = false;
static bool s_socketBattleHooked = false;
static bool s_streakHooked = false;
typedef void*(__fastcall* NameGetInstance_t)(void* mi);
typedef void*(__fastcall* NameGetMyProfileId_t)(void* self, void* mi);
static NameGetInstance_t s_nameGetInstance = nullptr;
static NameGetMyProfileId_t s_nameGetMyProfileId = nullptr;

static void* CurrentLocalPlayerIdObject() {
    if (!s_nameGetInstance || !s_nameGetMyProfileId) return nullptr;
    __try {
        void* module = s_nameGetInstance(nullptr);
        return module ? s_nameGetMyProfileId(module, nullptr) : nullptr;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return nullptr;
    }
}

// Phase 3: Capture localPlayerId from LocalPhaseController constructor
static char s_localPlayerId[256] = {0};
static bool s_forceWin = false;

static int32_t __fastcall ConvertWinningSideToBattleResultHook(int32_t winningSide, int32_t playerSide, void* mi) {
    if (!Original_ConvertWinningSideToBattleResult) return 0;
    if (!s_forceWin)
        return Original_ConvertWinningSideToBattleResult(winningSide, playerSide, mi);
    s_forcedResult = Original_ConvertWinningSideToBattleResult(playerSide, playerSide, mi);
    return s_forcedResult;
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
    void* localIdObject = CurrentLocalPlayerIdObject();
    if (s_forceWin && (localIdObject || s_localPlayerId[0])) {
        const char* winner = ReadIl2CppString(winnerId);
        const char* localId = localIdObject ? ReadIl2CppString(localIdObject) : s_localPlayerId;
        if (winner && localIdObject && localId && localId[0] && strcmp(winner, localId) != 0) {
            LOG("[FEATURE] BattleResult: LocalMatchData winner overridden");
            winnerId = localIdObject;
        }
    }
    Original_ApplyBattleResult(self, battleId, winnerId, unitUids, mi);
}

typedef void*(__fastcall* LocalBattleResult_t)(void* self, void* winnerId, void* unitUids,
    void* battleId, int32_t roundId, bool applyNow, int32_t battleHash, void* mi);
static LocalBattleResult_t Original_LocalBattleResult = nullptr;
static LocalBattleResult_t Original_SocketBattleResult = nullptr;
typedef void(__fastcall* SocketBattleCalculateResult_t)(void* self, int32_t matchId,
    void* battleId, void* winnerId, void* remainingMobsIds, void* mi);
static SocketBattleCalculateResult_t Original_SocketBattleCalculateResult = nullptr;

static void* __fastcall LocalBattleResultHook(void* self, void* winnerId, void* unitUids,
    void* battleId, int32_t roundId, bool applyNow, int32_t battleHash, void* mi) {
    void* localIdObject = CurrentLocalPlayerIdObject();
    if (s_forceWin && localIdObject) {
        LOG("[FEATURE] BattleResult: LocalServerEmulator winner overridden round=%d", roundId);
        winnerId = localIdObject;
    }
    return Original_LocalBattleResult
        ? Original_LocalBattleResult(self, winnerId, unitUids, battleId, roundId, applyNow, battleHash, mi)
        : nullptr;
}

static void* __fastcall SocketBattleResultHook(void* self, void* winnerId, void* unitUids,
    void* battleId, int32_t roundId, bool applyNow, int32_t battleHash, void* mi) {
    void* localIdObject = CurrentLocalPlayerIdObject();
    if (s_forceWin && localIdObject) {
        LOG("[FEATURE] BattleResult: ChessSocketsController winner overridden round=%d", roundId);
        winnerId = localIdObject;
    }
    return Original_SocketBattleResult
        ? Original_SocketBattleResult(self, winnerId, unitUids, battleId, roundId, applyNow, battleHash, mi)
        : nullptr;
}

static void __fastcall SocketBattleCalculateResultHook(void* self, int32_t matchId,
    void* battleId, void* winnerId, void* remainingMobsIds, void* mi) {
    void* localIdObject = CurrentLocalPlayerIdObject();
    if (s_forceWin && localIdObject) {
        LOG("[FEATURE] BattleResult: ChessSocketsController calculate winner overridden match=%d", matchId);
        winnerId = localIdObject;
    }
    if (Original_SocketBattleCalculateResult)
        Original_SocketBattleCalculateResult(self, matchId, battleId, winnerId, remainingMobsIds, mi);
}

static void __fastcall UpdateStreaksHook(void* self, int32_t result, void* mi) {
    if (!Original_UpdateStreaks) return;
    int32_t applied = result;
    // Treat an unresolved IsLocal getter as unknown/false.  Falling back to
    // true would incorrectly force every server-synchronised participant.
    bool local = false;
    if (Original_IsLocal) {
        __try { local = Original_IsLocal(self, nullptr); }
        __except (EXCEPTION_EXECUTE_HANDLER) { local = false; }
    }
    // Tournament: IsLocal=false for server-synced participants — match by UID instead.
    bool isOurParticipant = local;
    if (!isOurParticipant && Original_GetUid) {
        void* localIdObj = CurrentLocalPlayerIdObject();
        if (localIdObj) {
            __try {
                void* uid = Original_GetUid(self, nullptr);
                const char* uidStr = ReadIl2CppString(uid);
                const char* localStr = ReadIl2CppString(localIdObj);
                if (uidStr && localStr && strcmp(uidStr, localStr) == 0) {
                    isOurParticipant = true;
                    LOG("[FEATURE] BattleResult: Tournament UID match (IsLocal was false)");
                }
            } __except (EXCEPTION_EXECUTE_HANDLER) {}
        }
    }
    if (s_forceWin && isOurParticipant && s_forcedResult >= 0) {
        LOG("[FEATURE] BattleResult: %s UpdateStreaks %d -> %d",
            local ? "local" : "tournament", result, s_forcedResult);
        applied = s_forcedResult;
    }
    Original_UpdateStreaks(self, applied, mi);
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

    void* fn4 = ResolveMethodOrFallback("Assembly-CSharp",
        "AutoChess.CoreGameplay.Participant", "ServerParticipant",
        "UpdateStreaks", 1);
    void* fn5 = ResolveMethodOrFallback("Assembly-CSharp",
        "AutoChess.CoreGameplay.Participant", "ServerParticipant",
        "get_IsLocal", 0);
    void* fn6 = ResolveMethodOrFallback("Assembly-CSharp", "UserData",
        "NameModule", "get_instance", 0);
    void* fn7 = ResolveMethodOrFallback("Assembly-CSharp", "UserData",
        "NameModule", "get_MyProfileId", 0);
    s_nameGetInstance = reinterpret_cast<NameGetInstance_t>(fn6);
    s_nameGetMyProfileId = reinterpret_cast<NameGetMyProfileId_t>(fn7);
    void* fnUid = ResolveMethodOrFallback("Assembly-CSharp",
        "AutoChess.CoreGameplay.Participant", "ServerParticipant",
        "get_Uid", 0);
    Original_GetUid = reinterpret_cast<GetUid_t>(fnUid);
    LOG("[FEATURE] BattleResult: get_Uid=%p", fnUid);
    void* fn8 = ResolveMethodOrFallback("Assembly-CSharp", "AutoChess.LocalServer",
        "LocalServerEmulator", "BattleResult", 6);
    if (fn8 && MH_CreateHook(fn8, &LocalBattleResultHook,
        (LPVOID*)&Original_LocalBattleResult) == MH_OK && MH_EnableHook(fn8) == MH_OK) {
        s_localBattleHooked = true;
        LOG("[FEATURE] BattleResult: hooked LocalServerEmulator.BattleResult @ %p", fn8);
    } else LOG("[FEATURE] BattleResult: LocalServerEmulator.BattleResult unavailable");
    void* fn9 = ResolveMethodOrFallback("Assembly-CSharp", "AutoChess.ChessSockets",
        "ChessSocketsController", "ChessSockets.IChessSocketsController.BattleResult", 6);
    void* fn10 = ResolveMethodOrFallback("Assembly-CSharp", "AutoChess.ChessSockets",
        "ChessSocketsController", "ChessSockets.IChessSocketsController.BattleCalculateResult", 4);
    if (fn9 && MH_CreateHook(fn9, &SocketBattleResultHook,
        (LPVOID*)&Original_SocketBattleResult) == MH_OK && MH_EnableHook(fn9) == MH_OK) {
        s_socketBattleHooked = true;
        LOG("[FEATURE] BattleResult: hooked ChessSocketsController.BattleResult @ %p", fn9);
    } else LOG("[FEATURE] BattleResult: ChessSocketsController.BattleResult unavailable");
    if (fn10 && MH_CreateHook(fn10, &SocketBattleCalculateResultHook,
        (LPVOID*)&Original_SocketBattleCalculateResult) == MH_OK && MH_EnableHook(fn10) == MH_OK)
        LOG("[FEATURE] BattleResult: hooked ChessSocketsController.BattleCalculateResult @ %p", fn10);
    else LOG("[FEATURE] BattleResult: ChessSocketsController.BattleCalculateResult unavailable");
    LOG("[FEATURE] BattleResult: UpdateStreaks=%p get_IsLocal=%p", fn4, fn5);
    if (fn5)
        Original_IsLocal = reinterpret_cast<IsLocal_t>(fn5);
    if (fn4 && MH_CreateHook(fn4, &UpdateStreaksHook,
        (LPVOID*)&Original_UpdateStreaks) == MH_OK && MH_EnableHook(fn4) == MH_OK) {
        s_streakHooked = true;
        LOG("[FEATURE] BattleResult: hooked ServerParticipant.UpdateStreaks @ %p", fn4);
    } else LOG("[FEATURE] BattleResult: UpdateStreaks fail");

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
    s_forcedResult = -1;
    if (s_forceWin)
        s_forcedResult = Original_ConvertWinningSideToBattleResult(1, 1, nullptr);
}

void BattleResultFeature::OnMenu() {
    if (!enabled) return;
    if (!Original_ConvertWinningSideToBattleResult) {
        ImGui::TextColored(ImVec4(1, 0, 0, 1), "hook unavailable"); return;
    }
    ImGui::Checkbox("Force Win (all modes)", &m_forceWin);
    ImGui::Text("Paths: converter=%s local-emulator=%s socket=%s tournament-streak=%s",
        Original_ConvertWinningSideToBattleResult ? "hooked" : "missing",
        s_localBattleHooked ? "hooked" : "missing",
        s_socketBattleHooked ? "hooked" : "missing",
        s_streakHooked ? "hooked" : "missing");
    if (s_localPlayerId[0])
        ImGui::Text("Player ID: %s", s_localPlayerId);
    if (m_forceWin)
        ImGui::TextColored(ImVec4(0, 1, 0, 1), "Local battle + tournament streak result = Win");
}

static BattleResultFeature g_br;
static int g_brReg = (RegisterFeature(&g_br), 0);
