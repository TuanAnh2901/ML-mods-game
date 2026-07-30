#pragma once
#include <vector>
#include <cstdint>

struct Feature {
    const char* name;
    bool enabled;
    virtual void Init() {}
    virtual void OnUpdate() {}
    virtual void OnMenu() {}
};

extern std::vector<Feature*> g_features;

// Present hook fires before HookThread finishes Init(), so features must not
// run until every Init() has resolved its pointers.
extern bool g_featuresReady;

void RegisterFeature(Feature* f);
void ConfigLoad();
void ConfigSave();
