#include "currency.h"
#include "../framework.h"
#include "../il2cpp_resolve.h"
#include "../../minhook/include/MinHook.h"
#include "../feature.h"
#include "imgui.h"
#include <cstdio>

// Monitor ItemModule::ChangeResource — read-only display.
// This tracks observed local resource transitions.

typedef void (__fastcall* ChangeResource_t)(void* instance, int32_t type,
                                             int32_t delta, void* updateData,
                                             void* methodInfo);
static ChangeResource_t Original_ChangeResource = nullptr;

static int32_t s_types[32] = {0};
static int32_t s_bals[32] = {0};
static int s_count = 0;
static struct { int32_t t; int32_t d; } s_rec[16] = {0};
static int s_ridx = 0, s_rcnt = 0;

static const char* ResourceName(int32_t type) {
    switch (type) {
    case 1: return "gold";
    case 2: return "gems";
    case 3: return "scroll_common";
    case 4: return "scroll_uncommon";
    case 5: return "scroll_special";
    case 6: return "contribution";
    case 7: return "elixir";
    case 16: return "star_jewel";
    case 17: return "lucky_coin";
    case 18: return "energy_turn";
    case 19: return "dungeon_coin";
    case 20: return "lucky_coin_journey";
    case 25: return "labyrinth_attempt";
    case 26: return "labyrinth_roulette_coin";
    case 27: return "labyrinth_shop_coin";
    case 28: return "ticket";
    case 29: return "dust";
    case 34: return "forge_coin";
    case 45: return "wl_crown";
    case 56: return "jewel_gold";
    case 57: return "jewel_silver";
    case 58: return "friends_coin";
    case 59: return "pvp_medal";
    case 60: return "pvp_ticket";
    case 65: return "spin";
    case 66: return "pvp_ticket_friends";
    case 103: return "harem_energy";
    case 104: return "recharge_points";
    default: return "resource";
    }
}

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
    LOG("[FEATURE] Currency: %s[%d] delta=%+d tracked=%d", ResourceName(t), t,
        d, idx >= 0 ? s_bals[idx] : -1);
    Original_ChangeResource(inst, t, d, ud, mi);
}

CurrencyFeature::CurrencyFeature() { name = "Currency"; enabled = false; }

void CurrencyFeature::Init() {
}

void CurrencyFeature::OnUpdate() {}

void CurrencyFeature::OnMenu() {
    ImGui::Text("Currency monitor currently disabled.");
}

static CurrencyFeature g_c;
static int g_r = (RegisterFeature(&g_c), 0);
