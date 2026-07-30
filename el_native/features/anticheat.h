#pragma once
#include "../feature.h"

struct AntiCheatFeature : Feature {
    AntiCheatFeature();
    void Init() override;
    void OnUpdate() override;
    void OnMenu() override;

    bool m_bypass = false;
};
