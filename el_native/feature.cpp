#include "feature.h"
#include "framework.h"
#include <cstdio>
#include <cstring>

std::vector<Feature*> g_features;
bool g_featuresReady = false;

void RegisterFeature(Feature* f) {
    g_features.push_back(f);
    LOG("[P1] Feature registered: %s", f->name);
}

static const char* CONFIG_PATH = "el_native_config.ini";

void ConfigLoad() {
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
    LOG("[P1] Config saved");
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
