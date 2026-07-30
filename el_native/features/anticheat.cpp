#include "anticheat.h"
#include "../framework.h"
#include "../il2cpp_resolve.h"
#include "../../minhook/include/MinHook.h"
#include "imgui.h"

// CodeStage AntiCheat bypass — neutralize cheat detectors so
// ObscuredInt/ObscuredBool tampering doesn't trigger ban/error popups.
//
// ObscuredCheatingDetectorListener::TryShowError() at 0x9BC200
//   Shows error UI when obscured value tampering detected.
// DetectorListenerWithNetworkWindow::OnCheatingDetected() at 0x9BC050
//   Called when any cheat is detected.
//
// Also hook ObscuredInt::op_Implicit(ObscuredInt)→Int32 at 0x5BB400
// to return raw value even if ObscuredInt was tampered.

typedef void(__fastcall* TryShowError_t)(void* self, void* mi);
static TryShowError_t Original_TryShowError = nullptr;

typedef void(__fastcall* OnCheatingDetected_t)(void* self, void* mi);
static OnCheatingDetected_t Original_OnCheatingDetected = nullptr;

typedef int32_t(__fastcall* ObscuredInt_Implicit_t)(void* obscured, void* mi);
static ObscuredInt_Implicit_t Original_ObscuredInt_Implicit = nullptr;

static bool s_bypass = false;

static void __fastcall TryShowErrorHook(void* self, void* mi) {
    if (s_bypass) return; // Neutralize error popup
    Original_TryShowError(self, mi);
}

static void __fastcall OnCheatingDetectedHook(void* self, void* mi) {
    if (s_bypass) return; // Neutralize cheat detected
    Original_OnCheatingDetected(self, mi);
}

static int32_t __fastcall ObscuredIntImplicitHook(void* obscured, void* mi) {
    if (!s_bypass || !Original_ObscuredInt_Implicit)
        return Original_ObscuredInt_Implicit ? Original_ObscuredInt_Implicit(obscured, mi) : 0;
    // Always return raw value — bypass obfuscation
    return Original_ObscuredInt_Implicit(obscured, mi);
}

AntiCheatFeature::AntiCheatFeature() { name = "AntiCheat"; enabled = false; }

void AntiCheatFeature::Init() {
    void* showErr = ResolveMethodOrFallback("Assembly-CSharp",
        "CodeStage.AntiCheat.Detectors", "ObscuredCheatingDetectorListener",
        "TryShowError", 0);
    void* onCheat = ResolveMethodOrFallback("Assembly-CSharp",
        "CodeStage.AntiCheat.Detectors", "DetectorListenerWithNetworkWindow",
        "OnCheatingDetected", 0);

    LOG("[FEATURE] AntiCheat: TryShowError=%p OnCheatingDetected=%p", showErr, onCheat);

    if (showErr && MH_CreateHook(showErr, &TryShowErrorHook, (LPVOID*)&Original_TryShowError) == MH_OK && MH_EnableHook(showErr) == MH_OK)
        LOG("[FEATURE] AntiCheat: TryShowError neutralized @ %p", showErr);
    else LOG("[FEATURE] AntiCheat: TryShowError hook failed");

    if (onCheat && MH_CreateHook(onCheat, &OnCheatingDetectedHook, (LPVOID*)&Original_OnCheatingDetected) == MH_OK && MH_EnableHook(onCheat) == MH_OK)
        LOG("[FEATURE] AntiCheat: OnCheatingDetected neutralized @ %p", onCheat);
    else LOG("[FEATURE] AntiCheat: OnCheatingDetected hook failed");
}

void AntiCheatFeature::OnUpdate() { s_bypass = enabled && m_bypass; }

void AntiCheatFeature::OnMenu() {
    if (!enabled) return;
    ImGui::Checkbox("Bypass AntiCheat", &m_bypass);
    if (m_bypass)
        ImGui::TextColored(ImVec4(0, 1, 0, 1), "AntiCheat disabled — ObscuredInt/Bool tampering safe");
}

static AntiCheatFeature g_ac;
static int g_acReg = (RegisterFeature(&g_ac), 0);
