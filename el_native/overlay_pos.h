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
    float w = 0.0f;
    float h = 0.0f;
    float defX = 0.0f;
    float defY = 0.0f;
    bool registered = false;

    void Register() {
        if (registered) return;
        registered = true;
        char kx[96], ky[96], kw[96], kh[96];
        snprintf(kx, sizeof(kx), "%s.x", key);
        snprintf(ky, sizeof(ky), "%s.y", key);
        snprintf(kw, sizeof(kw), "%s.w", key);
        snprintf(kh, sizeof(kh), "%s.h", key);
        GlobalConfigRegistry().RegisterFloat(kx, &x);
        GlobalConfigRegistry().RegisterFloat(ky, &y);
        GlobalConfigRegistry().RegisterFloat(kw, &w);
        GlobalConfigRegistry().RegisterFloat(kh, &h);
    }

    void Apply() {
        const float px = (x == 0.0f && y == 0.0f) ? defX : x;
        const float py = (x == 0.0f && y == 0.0f) ? defY : y;
        if (px != 0.0f || py != 0.0f)
            ImGui::SetNextWindowPos(ImVec2(px, py), ImGuiCond_FirstUseEver);
        if (w > 0.0f && h > 0.0f)
            ImGui::SetNextWindowSize(ImVec2(w, h), ImGuiCond_FirstUseEver);
    }

    void Save() {
        ImVec2 pos = ImGui::GetWindowPos();
        ImVec2 size = ImGui::GetWindowSize();
        if (pos.x != x || pos.y != y || size.x != w || size.y != h) {
            x = pos.x;
            y = pos.y;
            w = size.x;
            h = size.y;
            ConfigMarkDirty();
        }
    }
};

