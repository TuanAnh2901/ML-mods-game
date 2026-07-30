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
// Three overloads: MonsterData, (int,int), (int,Rarity,int,int,double).
// Only hooking the MonsterData overload made display show multiplied price
// but actual SellUnit used the (int,int) overload → price reverted.
// Fix: hook ALL three overloads so SellUnit gets multiplied price too.

typedef int32_t (__fastcall* GetSellingPrice_t)(void* monster, void* methodInfo);
static GetSellingPrice_t Original_GetSellingPrice = nullptr;
static float s_sellMult = 1.0f;

static int32_t __fastcall GetSellingPriceHook(void* monster, void* mi) {
    int32_t base = Original_GetSellingPrice ? Original_GetSellingPrice(monster, mi) : 0;
    if (s_sellMult != 1.0f && base > 0)
        return (int32_t)(base * s_sellMult);
    return base;
}

// Overload: GetSellingPrice(int monsterId, int grade) — used by SellUnit
typedef int32_t (__fastcall* GetSellingPrice2_t)(int32_t monsterId, int32_t grade, void* mi);
static GetSellingPrice2_t Original_GetSellingPrice2 = nullptr;

static int32_t __fastcall GetSellingPrice2Hook(int32_t monsterId, int32_t grade, void* mi) {
    int32_t base = Original_GetSellingPrice2 ? Original_GetSellingPrice2(monsterId, grade, mi) : 0;
    if (s_sellMult != 1.0f && base > 0)
        return (int32_t)(base * s_sellMult);
    return base;
}

// Overload: GetSellingPrice(int grade, Rarity, int stackCost, int basePrice, double sellDiscount)
typedef int32_t (__fastcall* GetSellingPrice3_t)(int32_t grade, int32_t rarity, int32_t stackCost, int32_t basePrice, double sellDiscount, void* mi);
static GetSellingPrice3_t Original_GetSellingPrice3 = nullptr;

static int32_t __fastcall GetSellingPrice3Hook(int32_t grade, int32_t rarity, int32_t stackCost, int32_t basePrice, double sellDiscount, void* mi) {
    int32_t base = Original_GetSellingPrice3 ? Original_GetSellingPrice3(grade, rarity, stackCost, basePrice, sellDiscount, mi) : 0;
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

    // Hook all 3 GetSellingPrice overloads — only hooking the MonsterData
    // overload caused display to show multiplied price but actual SellUnit
    // used the (int,int) overload, reverting to original price on server sync.
    void* sp1 = ResolveMethodOrFallback("Assembly-CSharp", "", "MonsterDataUtils", "GetSellingPrice", 1);
    if (sp1 && MH_CreateHook(sp1, &GetSellingPriceHook, (LPVOID*)&Original_GetSellingPrice) == MH_OK && MH_EnableHook(sp1) == MH_OK)
        LOG("[FEATURE] BattleShop: hooked GetSellingPrice(MonsterData) @ %p", sp1);
    else LOG("[FEATURE] BattleShop: GetSellingPrice(MonsterData) fail");

    void* sp2 = ResolveMethodOrFallback("Assembly-CSharp", "", "MonsterDataUtils", "GetSellingPrice", 2);
    if (sp2 && MH_CreateHook(sp2, &GetSellingPrice2Hook, (LPVOID*)&Original_GetSellingPrice2) == MH_OK && MH_EnableHook(sp2) == MH_OK)
        LOG("[FEATURE] BattleShop: hooked GetSellingPrice(int,int) @ %p", sp2);
    else LOG("[FEATURE] BattleShop: GetSellingPrice(int,int) fail");

    void* sp3 = ResolveMethodOrFallback("Assembly-CSharp", "", "MonsterDataUtils", "GetSellingPrice", 5);
    if (sp3 && MH_CreateHook(sp3, &GetSellingPrice3Hook, (LPVOID*)&Original_GetSellingPrice3) == MH_OK && MH_EnableHook(sp3) == MH_OK)
        LOG("[FEATURE] BattleShop: hooked GetSellingPrice(grade,rarity,stack,base,sellDiscount) @ %p", sp3);
    else LOG("[FEATURE] BattleShop: GetSellingPrice(...) 5-arg fail");
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