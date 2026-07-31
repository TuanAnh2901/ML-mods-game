#pragma once

#include <string>
#include <vector>

enum class SettingType { Boolean, Integer, Float, String, Hotkey };

struct SettingDescriptor {
    std::string name;
    SettingType type = SettingType::String;
    void* storage = nullptr;
};

class ConfigRegistry {
public:
    bool RegisterBool(const std::string& name, bool* storage);
    bool RegisterInteger(const std::string& name, int* storage);
    bool RegisterFloat(const std::string& name, float* storage);
    bool RegisterString(const std::string& name, std::string* storage);
    bool RegisterHotkey(const std::string& name, std::string* storage);
    bool Set(const std::string& name, const std::string& value);
    std::string Get(const std::string& name) const;
    std::vector<SettingDescriptor> Descriptors() const;

private:
    bool Register(const std::string& name, SettingType type, void* storage);
    std::vector<SettingDescriptor> m_settings;
};

ConfigRegistry& GlobalConfigRegistry();
