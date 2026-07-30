#pragma once
#include "../feature.h"

struct ResourceTypeDumpFeature : Feature {
    ResourceTypeDumpFeature();
    void Init() override;
    void OnUpdate() override;
    void OnMenu() override;

private:
    bool m_dumped = false;
    int m_entryCount = 0;
    void* m_getStringByResourceType = nullptr;
    bool m_resolveFailed = false;
};
