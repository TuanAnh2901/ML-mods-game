#pragma once

#include "../combat_runtime.h"
#include "../feature.h"

struct CombatRuntimeFeature : Feature {
    CombatRuntimeFeature();
    void Init() override;
    void OnUpdate() override;
    void OnMenu() override;

    float m_attackSpeedMult = 1.0f;
    int m_multiHit = 1;
    bool m_freezeEnemies = false;
    bool m_dumbEnemies = false;
    bool m_entityManager = true;
    int m_playerSide = 1;
};
