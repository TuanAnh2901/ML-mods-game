#pragma once
#include "../feature.h"

struct GameSpeedFeature : Feature {
    GameSpeedFeature();
    void Init() override;
    void OnUpdate() override;
    void OnMenu() override;

    float m_multiplier = 1.0f;
};
