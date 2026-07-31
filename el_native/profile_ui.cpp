#include "profile_store.h"
#include "feature.h"
#include "../third_party/imgui/imgui.h"
#include <windows.h>

void ProfileUiRender() {
    static ProfileStore store("el_native_profiles.json");
    static ProfileDocument document;
    static bool loaded = false;
    static char profileName[64] = {};
    static ULONGLONG lastRefresh = 0;
    const ULONGLONG now = GetTickCount64();
    if (!loaded || now - lastRefresh >= 500) {
        if (!store.Load(document)) {
            document.currentProfile = "default";
            document.profiles["default"].name = "default";
        }
        loaded = true;
        lastRefresh = now;
    }
    ImGui::SeparatorText("Profiles");
    ImGui::Text("Current: %s", document.currentProfile.c_str());
    ImGui::TextWrapped("Profile changes are manual. Save current writes live toggles and typed settings; Load current restores the selected profile.");
    if (ImGui::Button("Save current")) {
        ConfigSave();
        store.Load(document);
    }
    ImGui::SameLine();
    if (ImGui::Button("Load current")) {
        ConfigLoad();
        store.Load(document);
    }
    ImGui::Separator();
    ImGui::InputText("Profile name", profileName, sizeof(profileName));
    if (ImGui::Button("Create")) {
        if (profileName[0] && store.Clone(document, document.currentProfile, profileName)) {
            store.Save(document);
            ConfigMarkDirty();
        }
        profileName[0] = '\0';
    }
    ImGui::SameLine();
    if (ImGui::Button("Rename") && profileName[0]) {
        if (store.Rename(document, document.currentProfile, profileName)) {
            store.Save(document);
            ConfigApplyProfileDocument(document);
        }
        profileName[0] = '\0';
    }
    ImGui::SameLine();
    if (ImGui::Button("Duplicate current")) {
        std::string name = document.currentProfile + "_copy";
        int suffix = 2;
        while (document.profiles.count(name)) name = document.currentProfile + "_copy" + std::to_string(suffix++);
        if (store.Clone(document, document.currentProfile, name)) {
            store.Save(document);
            ConfigMarkDirty();
        }
    }
    ImGui::SameLine();
    if (ImGui::Button("Delete current") && document.profiles.size() > 1) {
        if (store.Remove(document, document.currentProfile)) {
            store.Save(document);
            ConfigApplyProfileDocument(document);
        }
    }
    for (auto& pair : document.profiles) {
        bool selected = pair.first == document.currentProfile;
        if (ImGui::Selectable(pair.first.c_str(), selected)) {
            store.SetCurrent(document, pair.first);
            store.Save(document);
            ConfigApplyProfileDocument(document);
        }
    }
}
