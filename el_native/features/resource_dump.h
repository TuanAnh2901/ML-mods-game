#pragma once
#include "../feature.h"

struct ResourceTypeDumpFeature : Feature {
    ResourceTypeDumpFeature();
    void Init() override;
    void OnUpdate() override;
    void OnMenu() override;

private:
    bool m_dumped = false;
    bool m_resolveFailed = false;
    int m_entryCount = 0;
    int m_resolveSource = 0;
    char m_sourceReason[128] = {};
    void* m_getAllResourceTypes = nullptr;
    void* m_getStringByResourceType = nullptr;
    void* m_getResourceName = nullptr;
};
