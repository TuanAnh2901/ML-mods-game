#include "gacha.h"
#include "../framework.h"
#include "../il2cpp_resolve.h"
#include "../../minhook/include/MinHook.h"
#include "imgui.h"
#include <cstdint>
#include <cstring>

// ── Pool hooks (DrawMonstersFromPool) ────────────────────────────────────
// Called when the in-battle shop generates its roll offers.
typedef void* (__fastcall* DrawFromPool_t)(void* self, int32_t round,
                                           void* pveData, void* methodInfo);

struct PoolHook {
    const char* label;
    const char* ns;
    const char* klass;
    DrawFromPool_t original;
};

static PoolHook s_pools[4] = {
    {"Local",         "AutoChess.CoreGameplay.BattleShop",       "LocalMonsterPool",         nullptr},
    {"Pve",           "AutoChess.CoreGameplay.Pve.LocalServer",  "PveMonsterPool",           nullptr},
    {"Georgian",      "AutoChess.CoreGameplay.BattleShop",       "GeorgianLocalMonsterPool", nullptr},
    {"Labyrinth",     "AutoChess.CoreGameplay.Pve.LocalServer",  "LabyrinthMonsterPool",     nullptr},
};

static int32_t s_poolDrawn[8] = {0};
static int32_t s_poolCount = 0;
static const char* s_lastPool = "none";
static uint32_t s_poolFireCount = 0;
static int s_forceId = 0;

static void* RewritePool(int idx, void* self, int32_t round, void* pveData,
                         void* methodInfo) {
    PoolHook& p = s_pools[idx];
    void* list = p.original(self, round, pveData, methodInfo);
    if (!list) return list;

    s_lastPool = p.label;
    s_poolFireCount++;
    __try {
        void* items = *(void**)((uint8_t*)list + 0x10); // List._items
        int32_t size = *(int32_t*)((uint8_t*)list + 0x18); // List._size
        if (!items || size <= 0 || size > 64) return list;

        int32_t* data = (int32_t*)((uint8_t*)items + 0x20); // array[0]
        s_poolCount = size < 8 ? size : 8;
        for (int32_t i = 0; i < s_poolCount; ++i) s_poolDrawn[i] = data[i];

        // Force monster ID if set
        if (s_forceId > 0) {
            for (int32_t i = 0; i < size; ++i) data[i] = s_forceId;
        }
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        LOG("[FEATURE] Gacha: pool read SEH");
    }
    return list;
}
static void* __fastcall Draw0(void* s, int32_t r, void* d, void* m) { return RewritePool(0, s, r, d, m); }
static void* __fastcall Draw1(void* s, int32_t r, void* d, void* m) { return RewritePool(1, s, r, d, m); }
static void* __fastcall Draw2(void* s, int32_t r, void* d, void* m) { return RewritePool(2, s, r, d, m); }
static void* __fastcall Draw3(void* s, int32_t r, void* d, void* m) { return RewritePool(3, s, r, d, m); }
static DrawFromPool_t s_hookFns[4] = {&Draw0, &Draw1, &Draw2, &Draw3};
// ponytail: s_hookFns[] compiles to a 4-call jump dispatch; a computed goto
// table would be more elegant but this works.

// ── Shop hooks (RefreshShop + AddMonstersToQueue) ───────────────────────
typedef void (__fastcall* RefreshShop_t)(void* self, void* playerId,
                                          int32_t round, void* pveData,
                                          void* methodInfo);
typedef void (__fastcall* AddMonstersToQueue_t)(void* self, void* playerId,
                                                  void* monsterIds,
                                                  void* methodInfo);
static RefreshShop_t Original_RefreshShop = nullptr;
static AddMonstersToQueue_t Original_AddMonstersToQueue = nullptr;

static uint32_t s_shopRefreshCount = 0;
static uint32_t s_queueCount = 0;
static int32_t s_queueMonsters[8] = {0};
static int32_t s_queueSize = 0;

static void __fastcall RefreshShopHook(void* self, void* playerId, int32_t round,
                                        void* pveData, void* methodInfo) {
    s_shopRefreshCount++;
    LOG("[FEATURE] Gacha: RefreshShop(round=%d)", round);
    Original_RefreshShop(self, playerId, round, pveData, methodInfo);
}

static void __fastcall AddMonstersToQueueHook(void* self, void* playerId,
                                               void* monsterIds,
                                               void* methodInfo) {
    s_queueCount++;
    // Read List<int> from monsterIds
    __try {
        void* items = *(void**)((uint8_t*)monsterIds + 0x10);
        int32_t size = *(int32_t*)((uint8_t*)monsterIds + 0x18);
        if (items && size > 0 && size <= 64) {
            int32_t* data = (int32_t*)((uint8_t*)items + 0x20);
            s_queueSize = size < 8 ? size : 8;
            for (int32_t i = 0; i < s_queueSize; ++i) s_queueMonsters[i] = data[i];
        }
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        LOG("[FEATURE] Gacha: queue read SEH");
    }
    Original_AddMonstersToQueue(self, playerId, monsterIds, methodInfo);
}

// ── Feature ─────────────────────────────────────────────────────────────
static bool s_active = false;

GachaFeature::GachaFeature() { name = "Gacha"; enabled = false; }

void GachaFeature::Init() {
    // Pool hooks
    for (int i = 0; i < 4; ++i) {
        PoolHook& p = s_pools[i];
        void* fn = ResolveMethodOrFallback("Assembly-CSharp", p.ns, p.klass,
                                          "DrawMonstersFromPool", 2);
        if (!fn) { LOG("[FEATURE] Gacha: %s unresolved", p.label); continue; }
        if (MH_CreateHook(fn, s_hookFns[i], (LPVOID*)&p.original) == MH_OK &&
            MH_EnableHook(fn) == MH_OK)
            LOG("[FEATURE] Gacha: hooked %s @ %p", p.label, fn);
        else
            { p.original = nullptr; LOG("[FEATURE] Gacha: hook failed %s", p.label); }
    }

    // Shop hooks
    void* fn = ResolveMethodOrFallback("Assembly-CSharp",
        "AutoChess.LocalServer", "LocalShopController", "RefreshShop", 3);
    if (fn && MH_CreateHook(fn, &RefreshShopHook,
                            (LPVOID*)&Original_RefreshShop) == MH_OK &&
        MH_EnableHook(fn) == MH_OK)
        LOG("[FEATURE] Gacha: hooked RefreshShop @ %p", fn);
    else
        LOG("[FEATURE] Gacha: RefreshShop hook failed");

    fn = ResolveMethodOrFallback("Assembly-CSharp",
        "AutoChess.LocalServer", "LocalShopController", "AddMonstersToQueue", 2);
    if (fn && MH_CreateHook(fn, &AddMonstersToQueueHook,
                            (LPVOID*)&Original_AddMonstersToQueue) == MH_OK &&
        MH_EnableHook(fn) == MH_OK)
        LOG("[FEATURE] Gacha: hooked AddMonstersToQueue @ %p", fn);
    else
        LOG("[FEATURE] Gacha: AddMonstersToQueue hook failed");
}

void GachaFeature::OnUpdate() {
    bool any = false;
    for (auto& p : s_pools) if (p.original) any = true;
    s_active = enabled && (any || Original_RefreshShop);
    s_forceId = m_forceMonsterId;
}

void GachaFeature::OnMenu() {
    if (!enabled) return;

    ImGui::InputInt("ForceMonsterId", &m_forceMonsterId);
    ImGui::Text("0 = observe only");
    ImGui::Separator();

    ImGui::Text("Pool draws: %u  last pool: %s", s_poolFireCount, s_lastPool);
    char buf[128] = {0}; int off = 0;
    for (int32_t i = 0; i < s_poolCount && off < 100; ++i)
        off += snprintf(buf + off, sizeof(buf) - off, "%d ", s_poolDrawn[i]);
    ImGui::Text("pool drawn: %s", buf);

    ImGui::Separator();
    ImGui::Text("Shop refreshes: %u  queue adds: %u", s_shopRefreshCount, s_queueCount);
    buf[0] = 0; off = 0;
    for (int32_t i = 0; i < s_queueSize && off < 100; ++i)
        off += snprintf(buf + off, sizeof(buf) - off, "%d ", s_queueMonsters[i]);
    ImGui::Text("queue monsters: %s", buf);
}

static GachaFeature g_gacha;
static int g_gachaRegistered = (RegisterFeature(&g_gacha), 0);