#include "battle_shop.h"
#include "../framework.h"
#include "../il2cpp_resolve.h"
#include "../../minhook/include/MinHook.h"
#include "../feature.h"
#include "imgui.h"

// ── PvE: local shop hooks (works in single-player) ────────────────────────
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

// ── Sell multiplier: 3 GetSellingPrice overloads ──────────────────────────
typedef int32_t (__fastcall* GetSellingPrice_t)(void* monster, void* methodInfo);
static GetSellingPrice_t Original_GetSellingPrice = nullptr;

typedef int32_t (__fastcall* GetSellingPrice2_t)(int32_t monsterId, int32_t grade, void* mi);
static GetSellingPrice2_t Original_GetSellingPrice2 = nullptr;

typedef int32_t (__fastcall* GetSellingPrice3_t)(int32_t grade, int32_t rarity, int32_t stackCost, int32_t basePrice, double sellDiscount, void* mi);
static GetSellingPrice3_t Original_GetSellingPrice3 = nullptr;

static float s_sellMult = 1.0f;

static int32_t __fastcall GetSellingPriceHook(void* monster, void* mi) {
    int32_t base = Original_GetSellingPrice ? Original_GetSellingPrice(monster, mi) : 0;
    if (s_sellMult != 1.0f && base > 0) return (int32_t)(base * s_sellMult);
    return base;
}
static int32_t __fastcall GetSellingPrice2Hook(int32_t monsterId, int32_t grade, void* mi) {
    int32_t base = Original_GetSellingPrice2 ? Original_GetSellingPrice2(monsterId, grade, mi) : 0;
    if (s_sellMult != 1.0f && base > 0) return (int32_t)(base * s_sellMult);
    return base;
}
static int32_t __fastcall GetSellingPrice3Hook(int32_t grade, int32_t rarity, int32_t stackCost, int32_t basePrice, double sellDiscount, void* mi) {
    int32_t base = Original_GetSellingPrice3 ? Original_GetSellingPrice3(grade, rarity, stackCost, basePrice, sellDiscount, mi) : 0;
    if (s_sellMult != 1.0f && base > 0) return (int32_t)(base * s_sellMult);
    return base;
}

// ── PVP: ServerParticipant hooks (works in all modes) ─────────────────────
// ServerParticipant::AddSlot() at 0xE15AB0 — increases max slots by 1
typedef void(__fastcall* AddSlot_t)(void* self, void* mi);
static AddSlot_t Original_AddSlot = nullptr;
static bool s_pvpExtraSlots = false;
static int s_extraSlotCount = 3;

static void __fastcall AddSlotHook(void* self, void* mi) {
    Original_AddSlot(self, mi);
    if (s_pvpExtraSlots)
        for (int i = 1; i < s_extraSlotCount; i++)
            Original_AddSlot(self, mi);
}

// PvP coins are observational only: read ServerParticipant::get_Coins.
typedef int32_t(__fastcall* GetCoins_t)(void* self, void* mi);
static GetCoins_t Original_GetCoins = nullptr;
static int32_t s_pvpCoins = 0;
static int32_t __fastcall GetCoinsHook(void* self, void* mi) {
    s_pvpCoins = Original_GetCoins ? Original_GetCoins(self, mi) : 0;
    return s_pvpCoins;
}

// ── PVP: LocalServerEmulator event overrides ──────────────────────────────
// InvokeOnSlotsPriceUpdate(string id, int slotPrice, IEventResponseData) at 0xC0EB30
typedef void(__fastcall* InvokeOnSlotsPriceUpdate_t)(void* self, void* id, int32_t price, void* resp, void* mi);
static InvokeOnSlotsPriceUpdate_t Original_InvokeOnSlotsPriceUpdate = nullptr;
static bool s_pvpFreeSlots = false;

static void __fastcall InvokeOnSlotsPriceUpdateHook(void* self, void* id, int32_t price, void* resp, void* mi) {
    if (s_pvpFreeSlots) price = 0;
    Original_InvokeOnSlotsPriceUpdate(self, id, price, resp, mi);
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

    // PvE local shop
    h("AutoChess.LocalServer", "LocalShopController", "SetRefreshPrice", 2, (LPVOID*)&Original_SetRefreshPrice, &SetRefreshPriceHook);
    h("AutoChess.LocalServer", "LocalShopController", "UpdateSlotPrice", 2, (LPVOID*)&Original_UpdateSlotPrice, &UpdateSlotPriceHook);
    h("AutoChess.LocalServer", "LocalShopController", "GetSlotPrice", 1, (LPVOID*)&Original_GetSlotPrice, &GetSlotPriceHook);

    // Sell multiplier
    h("", "MonsterDataUtils", "GetSellingPrice", 1, (LPVOID*)&Original_GetSellingPrice, &GetSellingPriceHook);
    h("", "MonsterDataUtils", "GetSellingPrice", 2, (LPVOID*)&Original_GetSellingPrice2, &GetSellingPrice2Hook);
    h("", "MonsterDataUtils", "GetSellingPrice", 5, (LPVOID*)&Original_GetSellingPrice3, &GetSellingPrice3Hook);

    // PVP: extra slots
    h("AutoChess.CoreGameplay.Participant", "ServerParticipant", "AddSlot", 0, (LPVOID*)&Original_AddSlot, &AddSlotHook);

    // PVP: read-only coins
    h("AutoChess.CoreGameplay.Participant", "ServerParticipant", "get_Coins", 0, (LPVOID*)&Original_GetCoins, &GetCoinsHook);

    // PVP: free slot price override (server events)
    h("AutoChess.LocalServer", "LocalServerEmulator", "InvokeOnSlotsPriceUpdate", 3, (LPVOID*)&Original_InvokeOnSlotsPriceUpdate, &InvokeOnSlotsPriceUpdateHook);
}

void BattleShopFeature::OnUpdate() {
    s_forceFreeRefresh = enabled && m_freeRefresh;
    s_forceFreeSlots = enabled && m_freeSlots;
    s_sellMult = enabled ? m_sellMult : 1.0f;
    s_pvpExtraSlots = enabled && m_pvpExtraSlots;
    s_pvpFreeSlots = enabled && m_pvpFreeSlots;
}

void BattleShopFeature::OnMenu() {
    if (!enabled) return;
    ImGui::Text("-- PvE --");
    ImGui::Checkbox("Free shop refresh", &m_freeRefresh);
    ImGui::Checkbox("Free slot price", &m_freeSlots);
    ImGui::SliderFloat("Sell mult", &m_sellMult, 1.0f, 10.0f, "%.1fx");

    ImGui::Separator();
    ImGui::Text("-- PVP (all modes) --");
    ImGui::Text("PvP coins (read-only): %d", s_pvpCoins);
    ImGui::Checkbox("Extra slots (PVP)", &m_pvpExtraSlots);
    ImGui::Checkbox("Free slot price (PVP)", &m_pvpFreeSlots);
}

static BattleShopFeature g_bs;
static int g_bsReg = (RegisterFeature(&g_bs), 0);
