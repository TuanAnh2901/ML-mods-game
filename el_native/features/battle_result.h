#pragma once
#include "../feature.h"

struct BattleResultFeature : Feature {
    BattleResultFeature();
    void Init() override;
    void OnUpdate() override;
    void OnMenu() override;

    bool m_forceWin = false;
};
