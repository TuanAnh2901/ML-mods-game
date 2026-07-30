#pragma once
#include "../feature.h"
#include <cstdint>

struct DamageFeature : Feature {
    DamageFeature();
    void Init() override;
    void OnUpdate() override;
    void OnMenu() override;

    float m_playerMult = 1.0f;   // multiply when attacker == m_playerSide
    float m_enemyMult = 1.0f;    // multiply when attacker != m_playerSide
    int m_playerSide = -1;       // -1 = unknown, set via "capture" button
    int m_godModeSide = -1;      // -1 = off, side that cannot die
};
