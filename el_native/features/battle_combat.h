#pragma once
#include "../feature.h"

struct BattleCombatFeature : Feature {
    BattleCombatFeature();
    void Init() override;
    void OnUpdate() override;
    void OnMenu() override;

    float m_healMult = 1.0f;
    float m_offenseMult = 1.0f;
    float m_defenseMult = 1.0f;
    bool m_godMode = false;
    int m_playerSide = 1;
    bool m_trackStats = false;
};
