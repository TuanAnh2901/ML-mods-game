#pragma once
#include "../feature.h"

struct GachaFeature : Feature {
    GachaFeature();
    void Init() override;
    void OnUpdate() override;
    void OnMenu() override;

    int m_forceMonsterId = 0;
};
