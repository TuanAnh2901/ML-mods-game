#include "anticheat.h"
#include "../framework.h"
#include "../il2cpp_resolve.h"
#include "../compatibility_patch.h"
#include "imgui.h"

static bool s_detectorResolved = false;
static bool s_compatibility = false;

AntiCheatFeature::AntiCheatFeature() { name = "AntiCheat Diagnostics"; enabled = false; }

void AntiCheatFeature::Init() {
    std::size_t resolved = 0;
    for (std::size_t i = 0; i < CompatibilityPatchCount(); ++i) {
        const auto status = CompatibilityPatchAt(i);
        if (status.status == HookStatus::Hooked || status.status == HookStatus::Resolved) ++resolved;
    }
    s_detectorResolved = resolved != 0;
    s_compatibility = resolved == CompatibilityPatchCount();
    LOG("[DIAGNOSTIC] detector targets=%zu/%zu compatibility=%d", resolved,
        CompatibilityPatchCount(), s_compatibility);
}

void AntiCheatFeature::OnUpdate() {}

void AntiCheatFeature::OnMenu() {
    if (!enabled) return;
    ImGui::Checkbox("Show detector diagnostics", &m_showDiagnostics);
    if (!m_showDiagnostics) return;
    ImGui::Text("Detector status: %s", s_detectorResolved ? "resolved" : "unavailable");
    ImGui::Text("Compatibility: %s", s_compatibility ? "compatible" : "unknown");
    for (std::size_t i = 0; i < CompatibilityPatchCount(); ++i) {
        const auto status = CompatibilityPatchAt(i);
        const char* label = status.status == HookStatus::Hooked ? "hooked" :
            status.status == HookStatus::Conflict ? "conflict" :
            status.status == HookStatus::Resolved ? "resolved" : "unavailable";
        ImGui::Text("%s: %s", status.name ? status.name : "?", label);
    }
}

static AntiCheatFeature g_ac;
static int g_acReg = (RegisterFeature(&g_ac), 0);
