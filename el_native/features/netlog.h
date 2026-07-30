#pragma once
#include "../feature.h"

struct NetLogFeature : Feature {
    NetLogFeature();
    void Init() override;
    void OnUpdate() override;
    void OnMenu() override;
};
