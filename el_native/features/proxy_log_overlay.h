#pragma once
#include "../feature.h"

struct ProxyLogOverlayFeature : Feature {
    ProxyLogOverlayFeature();
    void Init() override;
    void OnUpdate() override;
    void OnMenu() override;
    void OnOverlay() override;
};

void ProxyLogOverlayRender();
