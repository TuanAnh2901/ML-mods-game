#pragma once

#include <map>
#include <string>

struct SharedProfileSettings {
    std::string launcherPath;
    int overlayDelayMs = 2000;
    std::string toggleHotkey = "F6";
    bool sidebarOpen = true;
};

struct Profile {
    std::string name;
    std::map<std::string, bool> enabled;
    std::map<std::string, float> sliders;
    std::map<std::string, std::string> filters;
    std::map<std::string, std::string> settings;
};

struct ProfileDocument {
    int schemaVersion = 1;
    std::string currentProfile = "default";
    SharedProfileSettings shared;
    std::map<std::string, Profile> profiles;
};

class ProfileStore {
public:
    explicit ProfileStore(std::string path);
    bool Save(const ProfileDocument& document) const;
    bool Load(ProfileDocument& document) const;
    bool RecoverBackup(ProfileDocument& document) const;
    bool MigrateIni(const std::string& iniPath, ProfileDocument& document) const;

    bool Clone(ProfileDocument& document, const std::string& source, const std::string& destination) const;
    bool Rename(ProfileDocument& document, const std::string& source, const std::string& destination) const;
    bool Remove(ProfileDocument& document, const std::string& profile) const;
    bool SetCurrent(ProfileDocument& document, const std::string& profile) const;
    const std::string& Path() const { return m_path; }

private:
    bool Parse(const std::string& json, ProfileDocument& document) const;
    std::string m_path;
};

void ProfileUiRender();
