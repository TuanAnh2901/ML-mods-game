#pragma once
#include "../feature.h"

struct RouletteTraceFeature : Feature {
    RouletteTraceFeature();
    void Init() override;
    void OnUpdate() override;
    void OnMenu() override;
};

void RouletteTraceRender();
