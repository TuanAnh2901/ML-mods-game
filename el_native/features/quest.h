#pragma once
#include "../feature.h"

struct DailyQuestFeature : Feature {
    DailyQuestFeature();
    void Init() override;
    void OnUpdate() override;
    void OnMenu() override;

    bool m_autoComplete = false;
    float m_rewardMult = 1.0f;
};
