#pragma once
#include "../feature.h"

// Hooks in-match shop economy:
// - SetRefreshPrice / UpdateSlotPrice → free reroll & slots
// - MonsterDataUtils::GetSellingPrice → sell price multiplier
struct BattleShopFeature : Feature {
    BattleShopFeature();
    void Init() override;
    void OnUpdate() override;
    void OnMenu() override;

    bool m_freeRefresh = false;
    bool m_freeSlots = false;
    float m_sellMult = 1.0f;
};
