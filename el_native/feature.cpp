#include "feature.h"
#include "framework.h"
#include <cstdio>
#include <cstring>
#include <windows.h>
#include "config_registry.h"
#include "profile_store.h"

std::vector<Feature*> g_features;
bool g_featuresReady = false;

void RegisterFeature(Feature* f) {
    g_features.push_back(f);
    LOG("[P1] Feature registered: %s", f->name);
}

static const char* CONFIG_PATH = "el_native_config.ini";
static ProfileStore g_profileStore("el_native_profiles.json");
static bool g_configDirty = false;

static bool LoadIniConfig() {
    FILE* f = fopen(CONFIG_PATH, "r");
    if (!f) return false;
    char line[256];
    while (fgets(line, sizeof(line), f)) {
        if (line[0] == '\n' || line[0] == '#' || line[0] == ';') continue;
        size_t len = strlen(line);
        while (len > 0 && (line[len - 1] == '\n' || line[len - 1] == '\r')) line[--len] = '\0';
        char* eq = strchr(line, '=');
        if (!eq) continue;
        *eq = '\0';
        for (auto* feature : g_features) {
            if (strcmp(feature->name, line) == 0) feature->enabled = (strcmp(eq + 1, "1") == 0);
        }
        GlobalConfigRegistry().Set(line, eq + 1);
    }
    fclose(f);
    return true;
}

void ConfigApplyProfileDocument(const ProfileDocument& document) {
    auto current = document.profiles.find(document.currentProfile);
    if (current == document.profiles.end()) return;
    for (auto* feature : g_features) {
        auto flag = current->second.enabled.find(feature->name);
        if (flag != current->second.enabled.end()) feature->enabled = flag->second;
    }
    for (const auto& setting : current->second.settings)
        GlobalConfigRegistry().Set(setting.first, setting.second);
}

void ConfigLoad() {
    ProfileDocument document;
    if (g_profileStore.Load(document) || g_profileStore.RecoverBackup(document)) {
        ConfigApplyProfileDocument(document);
        LOG("[PROFILE] loaded/recovered %s (%s)", g_profileStore.Path().c_str(), document.currentProfile.c_str());
        return;
    }

    FILE* f = fopen(CONFIG_PATH, "r");
    if (!f) {
        LOG("[P1] Config: no config file found at %s", CONFIG_PATH);
        return;
    }

    char line[256];
    while (fgets(line, sizeof(line), f)) {
        // Skip empty lines and comments
        if (line[0] == '\n' || line[0] == '#' || line[0] == ';')
            continue;
        // Remove trailing newline
        size_t len = strlen(line);
        while (len > 0 && (line[len - 1] == '\n' || line[len - 1] == '\r'))
            line[--len] = '\0';
        // Find separator
        char* eq = strchr(line, '=');
        if (!eq) continue;
        *eq = '\0';
        const char* key = line;
        const char* val = eq + 1;
        // Match feature by name
        for (auto* f : g_features) {
            if (strcmp(f->name, key) == 0) {
                f->enabled = (strcmp(val, "1") == 0);
                LOG("[P1] Config: %s = %d", f->name, f->enabled);
                break;
            }
        }
    }
    fclose(f);

    if (LoadIniConfig()) {
        ProfileDocument migrated;
        if (g_profileStore.MigrateIni(CONFIG_PATH, migrated)) {
            for (auto* feature : g_features) migrated.profiles["default"].enabled[feature->name] = feature->enabled;
            g_profileStore.Save(migrated);
            LOG("[PROFILE] migrated enabled flags from %s", CONFIG_PATH);
        }
    }
}

void ConfigSave() {
    FILE* f = fopen(CONFIG_PATH, "w");
    if (!f) {
        LOG("[P1] Config: failed to write %s", CONFIG_PATH);
        return;
    }
    for (auto* feature : g_features) {
        fprintf(f, "%s=%d\n", feature->name, feature->enabled ? 1 : 0);
    }
    fclose(f);
    ProfileDocument document;
    if (!g_profileStore.Load(document) && !g_profileStore.RecoverBackup(document)) {
        document.currentProfile = "default";
        document.profiles["default"].name = "default";
    }
    if (document.currentProfile.empty() || !document.profiles.count(document.currentProfile)) {
        document.currentProfile = document.profiles.empty() ? "default" : document.profiles.begin()->first;
        document.profiles[document.currentProfile].name = document.currentProfile;
    }
    Profile& profile = document.profiles[document.currentProfile];
    profile.name = document.currentProfile;
    // Rebuild enabled flags instead of merging, so removed features (e.g.
    // legacy Pass/Mascot entries) cannot remain in the active profile.
    profile.enabled.clear();
    for (auto* feature : g_features) profile.enabled[feature->name] = feature->enabled;
    for (const auto& descriptor : GlobalConfigRegistry().Descriptors())
        profile.settings[descriptor.name] = GlobalConfigRegistry().Get(descriptor.name);
    g_profileStore.Save(document);
    g_configDirty = false;
    LOG("[P1] Config saved");
}

static ULONGLONG g_dirtyAt = 0;
void ConfigMarkDirty() {
    g_configDirty = true;
    g_dirtyAt = GetTickCount64();
}

void ConfigAutosaveTick() {
    if (!g_configDirty || !g_dirtyAt) return;
    if (GetTickCount64() - g_dirtyAt >= 3000) {
        ConfigSave();
        g_dirtyAt = 0;
    }
}

// ============================================================
// No-op placeholder feature (demonstrates registration)
// ============================================================
struct DebugInfoFeature : Feature {
    DebugInfoFeature() {
        name = "DebugInfo";
        enabled = false;
    }
};

static DebugInfoFeature g_debugInfo;
static int g_debugInfoRegistered = (RegisterFeature(&g_debugInfo), 0);
