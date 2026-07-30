#include "battle_shop.h"
#include "../framework.h"
#include "../il2cpp_resolve.h"
#include "../../minhook/include/MinHook.h"
#include "../feature.h"
#include "imgui.h"

// ── Free refresh / free slots ───────────────────────────────────────────
typedef void (__fastcall* SetRefreshPrice_t)(void* self, void* playerId, int32_t price, void* mi);
static SetRefreshPrice_t Original_SetRefreshPrice = nullptr;
static bool s_forceFreeRefresh = false;

typedef void (__fastcall* UpdateSlotPrice_t)(void* self, void* playerId, int32_t price, void* mi);
static UpdateSlotPrice_t Original_UpdateSlotPrice = nullptr;
static bool s_forceFreeSlots = false;

typedef int32_t (__fastcall* GetSlotPrice_t)(void* self, void* playerId, void* mi);
static GetSlotPrice_t Original_GetSlotPrice = nullptr;

static void __fastcall SetRefreshPriceHook(void* s, void* p, int32_t v, void* m) {
    if (s_forceFreeRefresh) v = 0; Original_SetRefreshPrice(s, p, v, m);
}
static void __fastcall UpdateSlotPriceHook(void* s, void* p, int32_t v, void* m) {
    if (s_forceFreeSlots) v = 0; Original_UpdateSlotPrice(s, p, v, m);
}
static int32_t __fastcall GetSlotPriceHook(void* s, void* p, void* m) {
    if (s_forceFreeSlots) return 0; return Original_GetSlotPrice ? Original_GetSlotPrice(s, p, m) : 0;
}

// ── Sell multiplier via GetSellingPrice ─────────────────────────────────
// MonsterDataUtils::GetSellingPrice(MonsterData monster) at 0x6A0440.
// This affects BOTH buy AND sell display (game uses same base cost).
// Set the multiplier to control how much you get when selling.
typedef int32_t (__fastcall* GetSellingPrice_t)(void* monster, void* methodInfo);
static GetSellingPrice_t Original_GetSellingPrice = nullptr;
static float s_sellMult = 1.0f;

static int32_t __fastcall GetSellingPriceHook(void* monster, void* mi) {
    int32_t base = Original_GetSellingPrice ? Original_GetSellingPrice(monster, mi) : 0;
    if (s_sellMult != 1.0f && base > 0)
        return (int32_t)(base * s_sellMult);
    return base;
}

// ── Feature ─────────────────────────────────────────────────────────────
BattleShopFeature::BattleShopFeature() { name = "BattleShop"; enabled = false; }

void BattleShopFeature::Init() {
    auto h = [](const char* ns, const char* cls, const char* m, int ac, LPVOID* orig, LPVOID func) {
        void* fn = ResolveMethodOrFallback("Assembly-CSharp", ns, cls, m, ac);
        if (fn && MH_CreateHook(fn, func, orig) == MH_OK && MH_EnableHook(fn) == MH_OK)
            LOG("[FEATURE] BattleShop: hooked %s @ %p", m, fn);
        else LOG("[FEATURE] BattleShop: %s fail", m);
    };
    h("AutoChess.LocalServer", "LocalShopController", "SetRefreshPrice", 2, (LPVOID*)&Original_SetRefreshPrice, &SetRefreshPriceHook);
    h("AutoChess.LocalServer", "LocalShopController", "UpdateSlotPrice", 2, (LPVOID*)&Original_UpdateSlotPrice, &UpdateSlotPriceHook);
    h("AutoChess.LocalServer", "LocalShopController", "GetSlotPrice", 1, (LPVOID*)&Original_GetSlotPrice, &GetSlotPriceHook);
    void* fn = ResolveMethodOrFallback("Assembly-CSharp", "", "MonsterDataUtils", "GetSellingPrice", 1);
    if (fn && MH_CreateHook(fn, &GetSellingPriceHook, (LPVOID*)&Original_GetSellingPrice) == MH_OK && MH_EnableHook(fn) == MH_OK)
        LOG("[FEATURE] BattleShop: hooked GetSellingPrice @ %p", fn);
    else LOG("[FEATURE] BattleShop: GetSellingPrice fail");
}

void BattleShopFeature::OnUpdate() {
    s_forceFreeRefresh = enabled && m_freeRefresh;
    s_forceFreeSlots = enabled && m_freeSlots;
    s_sellMult = enabled ? m_sellMult : 1.0f;
}

void BattleShopFeature::OnMenu() {
    if (!enabled) return;
    ImGui::Checkbox("Free shop refresh", &m_freeRefresh);
    ImGui::Checkbox("Free slot price", &m_freeSlots);
    ImGui::SliderFloat("Sell mult (also affects buy display)", &m_sellMult, 1.0f, 10.0f, "%.1fx");
}

static BattleShopFeature g_bs;
static int g_bsReg = (RegisterFeature(&g_bs), 0);