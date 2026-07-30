#pragma once
#include "../feature.h"
#include <cstdint>

struct EnergyAttackSpeedFeature : Feature {
    EnergyAttackSpeedFeature();
    void Init() override;
    void OnUpdate() override;
    void OnMenu() override;

    float m_energyMult = 1.0f;
    float m_attackSpeedMult = 1.0f;
    int m_playerSide = 1;
    int m_statAttackSpeed = 9; // StatType for Attack Speed (from Tracer dump)
};
