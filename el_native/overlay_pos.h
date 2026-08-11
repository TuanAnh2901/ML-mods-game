#pragma once
// Chest-Indicator-style overlay window position persistence.  Each overlay
// window gets two float settings (x, y) in the global config; the position is
// restored on next launch and re-saved whenever the user drags the window.

#include "config_registry.h"
#include "../third_party/imgui/imgui.h"
#include <cstdio>

struct OverlayPos {
    const char* key;      // config key prefix, e.g. "actiontrace.window"
    float x = 0.0f;
    float y = 0.0f;
    bool registered = false;

    void Register() {
        if (registered) return;
        registered = true;
        char kx[96], ky[96];
        snprintf(kx, sizeof(kx), "%s.x", key);
        snprintf(ky, sizeof(ky), "%s.y", key);
        GlobalConfigRegistry().RegisterFloat(kx, &x);
        GlobalConfigRegistry().RegisterFloat(ky, &y);
    }

    void Apply() {
        if (x != 0.0f || y != 0.0f)
            ImGui::SetNextWindowPos(ImVec2(x, y), ImGuiCond_FirstUseEver);
    }

    void Save() {
        ImVec2 pos = ImGui::GetWindowPos();
        if (pos.x != x || pos.y != y) {
            x = pos.x;
            y = pos.y;
            ConfigMarkDirty();
        }
    }
};
