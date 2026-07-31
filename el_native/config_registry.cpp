#include "config_registry.h"

#include <cstdlib>
#include <sstream>

namespace {
SettingDescriptor* Find(std::vector<SettingDescriptor>& values, const std::string& name) {
    for (auto& value : values) if (value.name == name) return &value;
    return nullptr;
}
const SettingDescriptor* Find(const std::vector<SettingDescriptor>& values, const std::string& name) {
    for (const auto& value : values) if (value.name == name) return &value;
    return nullptr;
}
}

bool ConfigRegistry::Register(const std::string& name, SettingType type, void* storage) {
    if (name.empty() || !storage || Find(m_settings, name)) return false;
    m_settings.push_back({name, type, storage});
    return true;
}

bool ConfigRegistry::RegisterBool(const std::string& name, bool* storage) { return Register(name, SettingType::Boolean, storage); }
bool ConfigRegistry::RegisterInteger(const std::string& name, int* storage) { return Register(name, SettingType::Integer, storage); }
bool ConfigRegistry::RegisterFloat(const std::string& name, float* storage) { return Register(name, SettingType::Float, storage); }
bool ConfigRegistry::RegisterString(const std::string& name, std::string* storage) { return Register(name, SettingType::String, storage); }
bool ConfigRegistry::RegisterHotkey(const std::string& name, std::string* storage) { return Register(name, SettingType::Hotkey, storage); }

bool ConfigRegistry::Set(const std::string& name, const std::string& value) {
    SettingDescriptor* descriptor = Find(m_settings, name);
    if (!descriptor) return false;
    char* end = nullptr;
    switch (descriptor->type) {
    case SettingType::Boolean:
        if (value == "true" || value == "1") *static_cast<bool*>(descriptor->storage) = true;
        else if (value == "false" || value == "0") *static_cast<bool*>(descriptor->storage) = false;
        else return false;
        return true;
    case SettingType::Integer: {
        long parsed = std::strtol(value.c_str(), &end, 10);
        if (!end || *end) return false;
        *static_cast<int*>(descriptor->storage) = static_cast<int>(parsed);
        return true;
    }
    case SettingType::Float: {
        float parsed = std::strtof(value.c_str(), &end);
        if (!end || *end) return false;
        *static_cast<float*>(descriptor->storage) = parsed;
        return true;
    }
    case SettingType::String:
    case SettingType::Hotkey:
        *static_cast<std::string*>(descriptor->storage) = value;
        return true;
    }
    return false;
}

std::string ConfigRegistry::Get(const std::string& name) const {
    const SettingDescriptor* descriptor = Find(m_settings, name);
    if (!descriptor) return std::string();
    std::ostringstream output;
    switch (descriptor->type) {
    case SettingType::Boolean: output << (*static_cast<const bool*>(descriptor->storage) ? "true" : "false"); break;
    case SettingType::Integer: output << *static_cast<const int*>(descriptor->storage); break;
    case SettingType::Float: output << *static_cast<const float*>(descriptor->storage); break;
    case SettingType::String:
    case SettingType::Hotkey: output << *static_cast<const std::string*>(descriptor->storage); break;
    }
    return output.str();
}

std::vector<SettingDescriptor> ConfigRegistry::Descriptors() const { return m_settings; }

ConfigRegistry& GlobalConfigRegistry() {
    static ConfigRegistry registry;
    return registry;
}
