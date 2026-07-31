#pragma once
#include "../feature.h"

struct BattleShopFeature : Feature {
    BattleShopFeature();
    void Init() override;
    void OnUpdate() override;
    void OnMenu() override;

    bool m_freeRefresh = false;
    bool m_freeSlots = false;
    float m_sellMult = 1.0f;

    // PVP additions
    bool m_pvpFreeRefresh = false;
    bool m_pvpFreeSlots = false;
    bool m_pvpExtraSlots = false;
    bool m_pvpReadOnlyCoins = true;
};
