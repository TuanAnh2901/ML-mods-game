#include "currency.h"
#include "../framework.h"
#include "../il2cpp_resolve.h"
#include "../../minhook/include/MinHook.h"
#include "../feature.h"
#include "imgui.h"
#include <cstdio>

// Monitor ItemModule::ChangeResource — read-only display.
// Actual currency modification is done by mitmproxy (mitm_addon.py).
// This tracks what the game THINKS the values are.

typedef void (__fastcall* ChangeResource_t)(void* instance, int32_t type,
                                             int32_t delta, void* updateData,
                                             void* methodInfo);
static ChangeResource_t Original_ChangeResource = nullptr;

static int32_t s_types[32] = {0};
static int32_t s_bals[32] = {0};
static int s_count = 0;
static struct { int32_t t; int32_t d; } s_rec[16] = {0};
static int s_ridx = 0, s_rcnt = 0;

static int Find(int32_t t) {
    for (int i = 0; i < s_count; ++i) if (s_types[i] == t) return i;
    if (s_count < 32) { int i = s_count++; s_types[i] = t; s_bals[i] = 0; return i; }
    return -1;
}

static void __fastcall Hook(void* inst, int32_t t, int32_t d, void* ud, void* mi) {
    int idx = Find(t);
    if (idx >= 0) s_bals[idx] += d;
    s_rec[s_ridx] = {t, d};
    s_ridx = (s_ridx + 1) % 16;
    if (s_rcnt < 16) s_rcnt++;
    LOG("[FEATURE] Currency: type=%d delta=%+d bal=%d", t, d, idx >= 0 ? s_bals[idx] : -1);
    Original_ChangeResource(inst, t, d, ud, mi);
}

CurrencyFeature::CurrencyFeature() { name = "Currency"; enabled = false; }

void CurrencyFeature::Init() {
    void* fn = ResolveMethodOrFallback("Assembly-CSharp",
        "AutoChess.DataClasses.UserData", "ItemModule", "ChangeResource", 3);
    if (fn && MH_CreateHook(fn, &Hook, (LPVOID*)&Original_ChangeResource) == MH_OK && MH_EnableHook(fn) == MH_OK)
        LOG("[FEATURE] Currency: hooked @ %p (monitor only)", fn);
    else
        LOG("[FEATURE] Currency: hook fail");
}

void CurrencyFeature::OnUpdate() {}

void CurrencyFeature::OnMenu() {
    if (!enabled) return;
    if (!Original_ChangeResource) { ImGui::Text("hook unavailable"); return; }

    ImGui::Text("Monitor mode — real values set by mitmproxy");
    ImGui::Separator();

    if (s_count > 0) {
        for (int i = 0; i < s_count; ++i) {
            int32_t t = s_types[i];
            const char* n = "";
            if (t==1) n = "gold";
            else if (t==2) n = "gems/ruby";
            else if (t==18) n = "energy";
            ImGui::Text("  [%2d] %-12s = %+d", t, n, s_bals[i]);
        }
    }
    if (s_rcnt > 0) {
        char b[256] = {0}; int o = 0;
        int s = s_ridx - s_rcnt; if (s < 0) s += 16;
        for (int i = 0; i < s_rcnt && o < 240; ++i) {
            int x = (s + i) % 16;
            o += snprintf(b+o, sizeof(b)-o, "%d:%+d ", s_rec[x].t, s_rec[x].d);
        }
        ImGui::Text("Recent: %s", b);
    }
}

static CurrencyFeature g_c;
static int g_r = (RegisterFeature(&g_c), 0);