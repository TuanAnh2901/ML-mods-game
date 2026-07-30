#include "../feature.h"
#include "../il2cpp_resolve.h"
#include "../framework.h"
#include "../../minhook/include/MinHook.h"
#include "imgui.h"
#include <cstdint>
#include <cstdio>

// ── Show/Hide CheatWindow ──────────────────────────────────────────────
// CheatModuleOther::ShowHideConsole() doesn't use `self` — it toggles via
// static fields and creates the window if missing. Safe to call with nullptr.
typedef void (__fastcall* ShowHideConsole_t)(void* self, void* methodInfo);
static ShowHideConsole_t s_showConsole = nullptr;

// Capture instances for future use
extern "C" void CaptureCheatModuleGoTo(void* instance) {
    static void* s_goTo = nullptr;
    s_goTo = instance;
    LOG("[CHEAT] Captured CheatModuleGoTo @ %p", instance);
}
extern "C" void CaptureCheatModuleOther(void* instance) {
    static void* s_other = nullptr;
    s_other = instance;
    LOG("[CHEAT] Captured CheatModuleOther @ %p", instance);
}

// ── Debug.Log capture ──────────────────────────────────────────────────
// Hook Debug.Log(object) to capture cheat console output to our log.
typedef void (__fastcall* DebugLog_t)(void* message, void* methodInfo);
static DebugLog_t Original_DebugLog = nullptr;
static bool s_captureLogs = false;

static void ReadIL2CPPString(void* str, char* buf, int bufSize) {
    buf[0] = '\0';
    if (!str) return;
    __try {
        int32_t len = *(int32_t*)((uint8_t*)str + 0x10);
        if (len <= 0 || len > bufSize/2-2) return;
        const wchar_t* wcs = (const wchar_t*)((uint8_t*)str + 0x14);
        int out = 0;
        for (int32_t i = 0; i < len && out < bufSize-2; ++i)
            if (wcs[i] < 128) buf[out++] = (char)wcs[i];
            else buf[out++] = '?';
        buf[out] = '\0';
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        buf[0] = '\0';
    }
}

static void __fastcall DebugLogHook(void* message, void* methodInfo) {
    if (s_captureLogs && message) {
        char buf[512] = {0};
        ReadIL2CPPString(message, buf, sizeof(buf));
        if (buf[0]) {
            LOG("[CONSOLE] %s", buf);
        }
    }
    if (Original_DebugLog)
        Original_DebugLog(message, methodInfo);
}

// ── Feature ────────────────────────────────────────────────────────────
struct CheatTesterFeature : Feature {
    CheatTesterFeature();
    void Init() override;
    void OnUpdate() override;
    void OnMenu() override;

    bool m_captureDebugLog = false;
};

CheatTesterFeature::CheatTesterFeature() { name = "CheatTester"; enabled = false; }

void CheatTesterFeature::Init() {
    s_showConsole = (ShowHideConsole_t)ResolveMethodOrFallback(
        "Assembly-CSharp", "AutoChess.Prefabs.CheatsWindow.Scripts.Core.Modules",
        "CheatModuleOther", "ShowHideConsole", 0);

    void* debugLog = ResolveMethodOrFallback(
        "UnityEngine.CoreModule", "UnityEngine", "Debug", "Log", 1);
    if (debugLog && MH_CreateHook(debugLog, &DebugLogHook,
                                  (LPVOID*)&Original_DebugLog) == MH_OK &&
        MH_EnableHook(debugLog) == MH_OK)
        LOG("[FEATURE] CheatTester: hooked Debug.Log @ %p", debugLog);
    else
        LOG("[FEATURE] CheatTester: Debug.Log hook failed");

    LOG("[FEATURE] CheatTester: ShowHideConsole=%p DebugLog=%p",
        s_showConsole, debugLog);
}

void CheatTesterFeature::OnUpdate() {
    s_captureLogs = enabled && m_captureDebugLog;
}

void CheatTesterFeature::OnMenu() {
    if (!enabled) return;

    ImGui::TextColored(ImVec4(1, 1, 0, 1), "Dev Cheat Window");

    if (s_showConsole) {
        if (ImGui::Button("Show/Hide Cheat Window")) {
            __try {
                s_showConsole(nullptr, nullptr);
                LOG("[FEATURE] CheatTester: ShowHideConsole called");
            } __except (EXCEPTION_EXECUTE_HANDLER) {
                LOG("[FEATURE] CheatTester: ShowHideConsole SEH");
            }
        }
    } else {
        ImGui::TextColored(ImVec4(1, 0, 0, 1), "ShowHideConsole unavailable");
    }

    ImGui::Separator();
    ImGui::Checkbox("Capture Debug.Log to el_native.log", &m_captureDebugLog);
    if (m_captureDebugLog)
        ImGui::TextColored(ImVec4(0, 1, 0, 1), ">> Console output logged <<");
}

static CheatTesterFeature g_cheatTester;
static int g_cheatTesterRegistered = (RegisterFeature(&g_cheatTester), 0);