#pragma once
#include <vector>
#include <cstdint>
#include "profile_store.h"

struct Feature {
    const char* name;
    bool enabled;
    virtual void Init() {}
    virtual void OnUpdate() {}
    virtual void OnMenu() {}
    // Called every frame while enabled, even when the main debug overlay is
    // hidden (Chest-Indicator style HUD mini-window).
    virtual void OnOverlay() {}
};

extern std::vector<Feature*> g_features;

// Present hook fires before HookThread finishes Init(), so features must not
// run until every Init() has resolved its pointers.
extern bool g_featuresReady;

void RegisterFeature(Feature* f);
// Apply the selected profile to live feature flags and typed settings without
// re-reading the legacy INI file.  Profile UI and startup both use this path.
void ConfigApplyProfileDocument(const ProfileDocument& document);
void ConfigLoad();
void ConfigSave();
void ConfigMarkDirty();
void ConfigAutosaveTick();
