#pragma once
#include "../feature.h"

struct RelationshipFeature : Feature {
    RelationshipFeature();
    void Init() override;
    void OnUpdate() override;
    void OnMenu() override;

    bool m_enableMult = false;
    int m_pointsMultiplier = 100;
    bool m_enableNoGift = true;
    bool m_enableNoCost = true;
};
