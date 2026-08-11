#pragma once
#include "../feature.h"

using NetRequestFilter = bool(*)(void* url, void* payload);
void RegisterNetRequestFilter(NetRequestFilter filter);

struct NetLogFeature : Feature {
    NetLogFeature();
    void Init() override;
    void OnUpdate() override;
    void OnMenu() override;
};
