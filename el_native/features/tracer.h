#pragma once
#include "../feature.h"

struct TracerFeature : Feature {
    TracerFeature();
    void Init() override;
    void OnUpdate() override;
    void OnMenu() override;

    bool m_dumpStats = false;
};
