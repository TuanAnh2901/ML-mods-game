#include "game_speed.h"
#include "../framework.h"
#include "../il2cpp_resolve.h"
#include "../../minhook/include/MinHook.h"
#include "imgui.h"

// GameSpeed uses TWO mechanisms:
//   1. FixedUpdate re-entry — speeds up simulation steps (no server impact)
//   2. Time.timeScale — speeds up ALL Unity timers (animations, movement, etc.)
// Both are always applied when speed > 1.0 (no battle gate).

typedef void (*FixedUpdateFunc)(void* self, void* methodInfo);
static FixedUpdateFunc Original_FixedUpdate = nullptr;

typedef void (__fastcall* PhaseFunc)(void* self, void* methodInfo);
static PhaseFunc Original_StartBattle = nullptr;
static PhaseFunc Original_FinishBattle = nullptr;
static PhaseFunc Original_FinishBattleEarlier = nullptr;

static bool s_inBattle = false;
static float s_battleSpeed = 1.0f;
static float s_accumulator = 0.0f;
static bool s_inHook = false;
static uint32_t s_extraSteps = 0;

// Time.timeScale accessors
typedef void (*SetTimeScaleFunc)(float value);
typedef float (*GetTimeScaleFunc)();
static SetTimeScaleFunc Resolved_SetTimeScale = nullptr;
static GetTimeScaleFunc Resolved_GetTimeScale = nullptr;

static void __fastcall StartBattleHook(void* self, void* methodInfo) {
    Original_StartBattle(self, methodInfo);
    s_inBattle = true;
    s_accumulator = 0.0f;
}

static void __fastcall FinishBattleHook(void* self, void* methodInfo) {
    s_inBattle = false;
    Original_FinishBattle(self, methodInfo);
}

static void __fastcall FinishBattleEarlierHook(void* self, void* methodInfo) {
    s_inBattle = false;
    Original_FinishBattleEarlier(self, methodInfo);
}

static void FixedUpdateHook(void* self, void* methodInfo) {
    if (s_inHook) { Original_FixedUpdate(self, methodInfo); return; }
    Original_FixedUpdate(self, methodInfo);

    if (s_battleSpeed <= 1.0f) { s_accumulator = 0.0f; return; }

    s_accumulator += s_battleSpeed - 1.0f;
    int extra = (int)s_accumulator;
    if (extra > 16) extra = 16;
    s_accumulator -= (float)extra;

    s_inHook = true;
    for (int i = 0; i < extra; ++i) {
        Original_FixedUpdate(self, methodInfo);
        s_extraSteps++;
    }
    s_inHook = false;
}

GameSpeedFeature::GameSpeedFeature() {
    name = "GameSpeed";
    enabled = false;
}

void GameSpeedFeature::Init() {
    void* fixedUpdate = ResolveMethodOrFallback("Assembly-CSharp",
        "AutoChess.LocalServer", "LocalMatchTimer", "FixedUpdate", 0);
    if (fixedUpdate && MH_CreateHook(fixedUpdate, &FixedUpdateHook,
                                    (LPVOID*)&Original_FixedUpdate) == MH_OK &&
        MH_EnableHook(fixedUpdate) == MH_OK)
        LOG("[FEATURE] GameSpeed: hooked FixedUpdate @ %p", fixedUpdate);
    else
        LOG("[FEATURE] GameSpeed: FixedUpdate hook failed");

    // Phase hooks (informational only, no gate)
    struct { const char* n; PhaseFunc h; PhaseFunc* o; } phases[3] = {
        {"StartBattle", &StartBattleHook, &Original_StartBattle},
        {"FinishBattle", &FinishBattleHook, &Original_FinishBattle},
        {"FinishBattleEarlier", &FinishBattleEarlierHook, &Original_FinishBattleEarlier},
    };
    for (auto& p : phases) {
        void* fn = ResolveMethodOrFallback("Assembly-CSharp",
            "AutoChess.LocalServer", "LocalPhaseController", p.n, 0);
        if (fn && MH_CreateHook(fn, p.h, (LPVOID*)p.o) == MH_OK && MH_EnableHook(fn) == MH_OK)
            LOG("[FEATURE] GameSpeed: hooked %s @ %p", p.n, fn);
        else
            LOG("[FEATURE] GameSpeed: %s hook failed", p.n);
    }

    // Time.timeScale — needed for actual speedup (FixedUpdate alone doesn't speed rendering)
    Resolved_SetTimeScale = (SetTimeScaleFunc)ResolveMethodOrFallback(
        "UnityEngine.CoreModule", "UnityEngine", "Time", "set_timeScale", 1);
    Resolved_GetTimeScale = (GetTimeScaleFunc)ResolveMethodOrFallback(
        "UnityEngine.CoreModule", "UnityEngine", "Time", "get_timeScale", 0);
    LOG("[FEATURE] GameSpeed: Time.timeScale resolved: set=%p get=%p",
        Resolved_SetTimeScale, Resolved_GetTimeScale);
}

void GameSpeedFeature::OnUpdate() {
    s_battleSpeed = enabled ? m_multiplier : 1.0f;

    // Apply Time.timeScale every frame (game may reset it)
    if (Resolved_SetTimeScale && Resolved_GetTimeScale) {
        float want = enabled ? m_multiplier : 1.0f;
        if (Resolved_GetTimeScale() != want) {
            Resolved_SetTimeScale(want);
        }
    }
}

void GameSpeedFeature::OnMenu() {
    ImGui::SliderFloat("Speed", &m_multiplier, 0.5f, 8.0f, "%.1fx");
    ImGui::Text("in battle: %s  extra steps: %u",
                s_inBattle ? "YES" : "no", s_extraSteps);
    if (s_battleSpeed > 1.0f && enabled) {
        ImGui::TextColored(ImVec4(0, 1, 0, 1), ">> SPEED ACTIVE <<");
    }
    if (Resolved_GetTimeScale) {
        ImGui::Text("Time.timeScale: %.2f", Resolved_GetTimeScale());
    }
}

static GameSpeedFeature g_gameSpeed;
static int g_gameSpeedRegistered = (RegisterFeature(&g_gameSpeed), 0);