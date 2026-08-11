#pragma once
#include "../feature.h"

struct ActionTracerFeature : Feature {
    ActionTracerFeature();
    void Init() override;
    void OnUpdate() override;
    void OnMenu() override;
    void OnOverlay() override;
};

void ActionTracerRender();
