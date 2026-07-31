#include "relationship.h"
#include "../framework.h"
#include "../il2cpp_resolve.h"
#include "../hook_registry.h"
#include "../../minhook/include/MinHook.h"
#include "imgui.h"
#include <cstdint>

// === Runtime toggles (sync from Feature members each frame) ===
static bool s_enableMult = false;
static int s_pointsMult = 100;
static bool s_enableNoGift = true;
static bool s_enableNoCost = true;

// ============================================================
// Hook 1: RelationshipsModel::SendPoints(ResourceType, Int32)
//   Multiplies points parameter. Toggle off = passthrough.
//   x64: RCX=self RDX=resourceType R8=points R9=methodInfo
// ============================================================
typedef void(__fastcall* SendPoints_t)(void* self, int32_t resourceType,
                                       int32_t points, void* methodInfo);
static SendPoints_t Original_SendPoints = nullptr;

static void __fastcall SendPointsHook(void* self, int32_t resourceType,
                                       int32_t points, void* methodInfo) {
    if (!Original_SendPoints) return;
    if (s_enableMult) {
        int32_t modPoints = points * s_pointsMult;
        LOG("[FEATURE] Relationship: SendPoints(%d, %d) -> %d",
            resourceType, points, modPoints);
        Original_SendPoints(self, resourceType, modPoints, methodInfo);
    } else {
        Original_SendPoints(self, resourceType, points, methodInfo);
    }
}

// ============================================================
// Hook 2: RelationshipsModel::IsNotEnoughGifts()
//   Returns false when toggle on -> bypass gift requirement.
//   x64: RCX=self RDX=methodInfo AL=return
// ============================================================
typedef uint8_t(__fastcall* IsNotEnoughGifts_t)(void* self, int32_t resourceType, void* methodInfo);
static IsNotEnoughGifts_t Original_IsNotEnoughGifts = nullptr;

static uint8_t __fastcall IsNotEnoughGiftsHook(void* self, int32_t resourceType, void* methodInfo) {
    if (s_enableNoGift) {
        LOG("[FEATURE] Relationship: IsNotEnoughGifts -> false (bypass)");
        return 0;
    }
    return Original_IsNotEnoughGifts
        ? Original_IsNotEnoughGifts(self, resourceType, methodInfo) : 1;
}

// ============================================================
// Hook 3: RelationshipsModel::GetCostResources()
//   Clears SortedList when toggle on -> no resource cost.
//   x64: RCX=self RDX=methodInfo RAX=return (SortedList*)
// ============================================================
typedef void*(__fastcall* GetCostResources_t)(void* self, void* methodInfo);
static GetCostResources_t Original_GetCostResources = nullptr;
static bool s_sendResolved = false;
static bool s_giftResolved = false;
static bool s_costResolved = false;

template <typename T>
static bool InstallRelationshipHook(const char* owner, void* target, T detour, LPVOID* original) {
    if (!target) return false;
    const auto address = reinterpret_cast<std::uintptr_t>(target);
    if (!GlobalHookRegistry().Claim(address, owner)) return false;
    GlobalHookRegistry().MarkResolved(address);
    MH_STATUS status = MH_CreateHook(target, reinterpret_cast<LPVOID>(detour), original);
    if (status == MH_OK) status = MH_EnableHook(target);
    if (status == MH_OK) { GlobalHookRegistry().MarkHooked(address); return true; }
    GlobalHookRegistry().MarkUnavailable(address);
    return false;
}

static void* __fastcall GetCostResourcesHook(void* self, void* methodInfo) {
    if (!Original_GetCostResources) return nullptr;
    void* list = Original_GetCostResources(self, methodInfo);
    if (s_enableNoCost && list) {
        *(int32_t*)((uint8_t*)list + 0x18) = 0;
        LOG("[FEATURE] Relationship: GetCostResources -> cleared");
    }
    return list;
}

// ============================================================
// Feature
// ============================================================
RelationshipFeature::RelationshipFeature() {
    name = "Relationship";
    enabled = false;
}

void RelationshipFeature::Init() {
    // Hook 1: SendPoints
    void* send = ResolveMethodOrFallback("Assembly-CSharp",
        "AutoChess.MonsterInfoNew.MonsterInfoTabs.Relationships",
        "RelationshipsModel", "SendPoints", 2);
    LOG("[FEATURE] Relationship: SendPoints @ %p", send);
    s_sendResolved = InstallRelationshipHook("relationship.send_points", send,
        &SendPointsHook, (LPVOID*)&Original_SendPoints);
    LOG("[FEATURE] Relationship: SendPoints %s", s_sendResolved ? "hooked" : "unresolved/conflict");

    // Hook 2: IsNotEnoughGifts
    void* notEnough = ResolveMethodOrFallback("Assembly-CSharp",
        "AutoChess.MonsterInfoNew.MonsterInfoTabs.Relationships",
        "RelationshipsModel", "IsNotEnoughGifts", 1);
    LOG("[FEATURE] Relationship: IsNotEnoughGifts @ %p", notEnough);
    s_giftResolved = InstallRelationshipHook("relationship.gift_check", notEnough,
        &IsNotEnoughGiftsHook, (LPVOID*)&Original_IsNotEnoughGifts);
    LOG("[FEATURE] Relationship: IsNotEnoughGifts %s", s_giftResolved ? "hooked" : "unresolved/conflict");

    // Hook 3: GetCostResources
    void* cost = ResolveMethodOrFallback("Assembly-CSharp",
        "AutoChess.MonsterInfoNew.MonsterInfoTabs.Relationships",
        "RelationshipsModel", "GetCostResources", 0);
    LOG("[FEATURE] Relationship: GetCostResources @ %p", cost);
    s_costResolved = InstallRelationshipHook("relationship.cost_resources", cost,
        &GetCostResourcesHook, (LPVOID*)&Original_GetCostResources);
    LOG("[FEATURE] Relationship: GetCostResources %s", s_costResolved ? "hooked" : "unresolved/conflict");
}

void RelationshipFeature::OnUpdate() {
    s_pointsMult = m_pointsMultiplier;
    s_enableMult = enabled && m_enableMult;
    s_enableNoGift = enabled && m_enableNoGift;
    s_enableNoCost = enabled && m_enableNoCost;
}

void RelationshipFeature::OnMenu() {
    if (!enabled) return;

    ImGui::Checkbox("Multiply Points", &m_enableMult);
    if (m_enableMult)
        ImGui::SliderInt("Multiplier", &m_pointsMultiplier, 1, 10000, "%dx");

    ImGui::Checkbox("Bypass Gift Check (IsNotEnough=0)", &m_enableNoGift);
    ImGui::Checkbox("Zero Resource Cost (GetCostResources=empty)", &m_enableNoCost);

    if (!s_sendResolved)
        ImGui::TextColored(ImVec4(1, 0, 0, 1), "X SendPoints unresolved");
    if (!s_giftResolved)
        ImGui::TextColored(ImVec4(1, 0, 0, 1), "X IsNotEnoughGifts unresolved");
    if (!s_costResolved)
        ImGui::TextColored(ImVec4(1, 0, 0, 1), "X GetCostResources unresolved");
}

static RelationshipFeature g_rel;
static int g_relReg = (RegisterFeature(&g_rel), 0);
