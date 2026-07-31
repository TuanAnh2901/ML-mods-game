#include "profile_store.h"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <utility>

#ifdef _WIN32
#include <windows.h>
#endif

namespace {
std::string Escape(const std::string& value) {
    std::string escaped;
    for (char ch : value) {
        if (ch == '\\' || ch == '"') escaped.push_back('\\');
        if (ch == '\n') { escaped += "\\n"; continue; }
        escaped.push_back(ch);
    }
    return escaped;
}

std::string ReadAll(const std::string& path) {
    std::ifstream file(path.c_str(), std::ios::binary);
    if (!file) return std::string();
    std::ostringstream stream;
    stream << file.rdbuf();
    return stream.str();
}

void WriteBoolMap(std::ostringstream& out, const std::map<std::string, bool>& values) {
    out << "{";
    bool first = true;
    for (const auto& pair : values) {
        if (!first) out << ",";
        first = false;
        out << "\"" << Escape(pair.first) << "\":" << (pair.second ? "true" : "false");
    }
    out << "}";
}

void WriteFloatMap(std::ostringstream& out, const std::map<std::string, float>& values) {
    out << "{";
    bool first = true;
    for (const auto& pair : values) {
        if (!first) out << ",";
        first = false;
        out << "\"" << Escape(pair.first) << "\":" << pair.second;
    }
    out << "}";
}

void WriteStringMap(std::ostringstream& out, const std::map<std::string, std::string>& values) {
    out << "{";
    bool first = true;
    for (const auto& pair : values) {
        if (!first) out << ",";
        first = false;
        out << "\"" << Escape(pair.first) << "\":\"" << Escape(pair.second) << "\"";
    }
    out << "}";
}

std::string Serialize(const ProfileDocument& document) {
    std::ostringstream out;
    out << "{\"schema_version\":" << document.schemaVersion
        << ",\"current_profile\":\"" << Escape(document.currentProfile) << "\",\"shared\":{";
    out << "\"launcher_path\":\"" << Escape(document.shared.launcherPath) << "\","
        << "\"overlay_delay_ms\":" << document.shared.overlayDelayMs << ","
        << "\"toggle_hotkey\":\"" << Escape(document.shared.toggleHotkey) << "\","
        << "\"sidebar_open\":" << (document.shared.sidebarOpen ? "true" : "false") << "},\"profiles\":{";
    bool firstProfile = true;
    for (const auto& pair : document.profiles) {
        if (!firstProfile) out << ",";
        firstProfile = false;
        const Profile& profile = pair.second;
        out << "\"" << Escape(pair.first) << "\":{";
        out << "\"name\":\"" << Escape(profile.name) << "\",\"enabled\":";
        WriteBoolMap(out, profile.enabled);
        out << ",\"sliders\":";
        WriteFloatMap(out, profile.sliders);
        out << ",\"filters\":";
        WriteStringMap(out, profile.filters);
        out << ",\"settings\":";
        WriteStringMap(out, profile.settings);
        out << "}";
    }
    out << "}}";
    return out.str();
}

class Parser {
public:
    explicit Parser(const std::string& input) : m_input(input) {}

    bool ParseDocument(ProfileDocument& document) {
        Skip();
        if (!Consume('{')) return false;
        bool schema = false, current = false, shared = false, profiles = false;
        while (true) {
            Skip();
            if (Consume('}')) break;
            std::string key;
            if (!String(key) || !Consume(':')) return false;
            if (key == "schema_version") { if (!Integer(document.schemaVersion)) return false; schema = true; }
            else if (key == "current_profile") { if (!String(document.currentProfile)) return false; current = true; }
            else if (key == "shared") { if (!Shared(document.shared)) return false; shared = true; }
            else if (key == "profiles") { if (!Profiles(document.profiles)) return false; profiles = true; }
            else if (!SkipValue()) return false;
            Skip();
            if (Consume('}')) break;
            if (!Consume(',')) return false;
        }
        Skip();
        return schema && current && shared && profiles && m_pos == m_input.size();
    }

private:
    void Skip() { while (m_pos < m_input.size() && (m_input[m_pos] == ' ' || m_input[m_pos] == '\n' || m_input[m_pos] == '\r' || m_input[m_pos] == '\t')) ++m_pos; }
    bool Consume(char expected) { Skip(); if (m_pos >= m_input.size() || m_input[m_pos] != expected) return false; ++m_pos; return true; }
    bool String(std::string& value) {
        Skip(); if (m_pos >= m_input.size() || m_input[m_pos] != '"') return false; ++m_pos; value.clear();
        while (m_pos < m_input.size()) {
            char ch = m_input[m_pos++];
            if (ch == '"') return true;
            if (ch == '\\' && m_pos < m_input.size()) {
                char escaped = m_input[m_pos++];
                value.push_back(escaped == 'n' ? '\n' : escaped);
            } else value.push_back(ch);
        }
        return false;
    }
    bool Token(const char* token) { Skip(); const std::string value(token); if (m_input.compare(m_pos, value.size(), value) != 0) return false; m_pos += value.size(); return true; }
    bool Integer(int& value) { Skip(); const char* begin = m_input.c_str() + m_pos; char* end = nullptr; long parsed = std::strtol(begin, &end, 10); if (end == begin) return false; m_pos += static_cast<std::size_t>(end - begin); value = static_cast<int>(parsed); return true; }
    bool Float(float& value) { Skip(); const char* begin = m_input.c_str() + m_pos; char* end = nullptr; double parsed = std::strtod(begin, &end); if (end == begin) return false; m_pos += static_cast<std::size_t>(end - begin); value = static_cast<float>(parsed); return true; }
    bool Bool(bool& value) { if (Token("true")) { value = true; return true; } if (Token("false")) { value = false; return true; } return false; }
    bool SkipValue() {
        Skip();
        if (m_pos >= m_input.size()) return false;
        if (m_input[m_pos] == '"') { std::string ignored; return String(ignored); }
        if (m_input[m_pos] == '{') { ++m_pos; int depth = 1; bool quoted = false; while (m_pos < m_input.size() && depth) { char ch = m_input[m_pos++]; if (ch == '"' && (m_pos < 2 || m_input[m_pos - 2] != '\\')) quoted = !quoted; if (!quoted && ch == '{') ++depth; if (!quoted && ch == '}') --depth; } return depth == 0; }
        if (m_input[m_pos] == '[') { ++m_pos; int depth = 1; while (m_pos < m_input.size() && depth) { char ch = m_input[m_pos++]; if (ch == '[') ++depth; if (ch == ']') --depth; } return depth == 0; }
        bool ignoredBool = false; if (Bool(ignoredBool)) return true; float ignoredFloat = 0.0f; return Float(ignoredFloat);
    }
    bool FloatMap(std::map<std::string, float>& values) {
        if (!Consume('{')) return false; while (true) { Skip(); if (Consume('}')) return true; std::string key; if (!String(key) || !Consume(':')) return false; float value = 0.0f; if (!Float(value)) return false; values[key] = value; Skip(); if (Consume('}')) return true; if (!Consume(',')) return false; }
    }
    bool StringMap(std::map<std::string, std::string>& values) {
        if (!Consume('{')) return false; while (true) { Skip(); if (Consume('}')) return true; std::string key; if (!String(key) || !Consume(':')) return false; std::string value; if (!String(value)) return false; values[key] = value; Skip(); if (Consume('}')) return true; if (!Consume(',')) return false; }
    }
    bool BoolMap(std::map<std::string, bool>& values) {
        if (!Consume('{')) return false; while (true) { Skip(); if (Consume('}')) return true; std::string key; if (!String(key) || !Consume(':')) return false; bool value = false; if (!Bool(value)) return false; values[key] = value; Skip(); if (Consume('}')) return true; if (!Consume(',')) return false; }
    }
    bool Shared(SharedProfileSettings& shared) {
        if (!Consume('{')) return false; while (true) { Skip(); if (Consume('}')) return true; std::string key; if (!String(key) || !Consume(':')) return false; if (key == "launcher_path") { if (!String(shared.launcherPath)) return false; } else if (key == "overlay_delay_ms") { if (!Integer(shared.overlayDelayMs)) return false; } else if (key == "toggle_hotkey") { if (!String(shared.toggleHotkey)) return false; } else if (key == "sidebar_open") { if (!Bool(shared.sidebarOpen)) return false; } else if (!SkipValue()) return false; Skip(); if (Consume('}')) return true; if (!Consume(',')) return false; }
    }
    bool ProfileObject(Profile& profile) {
        if (!Consume('{')) return false; while (true) { Skip(); if (Consume('}')) return true; std::string key; if (!String(key) || !Consume(':')) return false; if (key == "name") { if (!String(profile.name)) return false; } else if (key == "enabled") { if (!BoolMap(profile.enabled)) return false; } else if (key == "sliders") { if (!FloatMap(profile.sliders)) return false; } else if (key == "filters") { if (!StringMap(profile.filters)) return false; } else if (key == "settings") { if (!StringMap(profile.settings)) return false; } else if (!SkipValue()) return false; Skip(); if (Consume('}')) return true; if (!Consume(',')) return false; }
    }
    bool Profiles(std::map<std::string, Profile>& profiles) {
        if (!Consume('{')) return false; while (true) { Skip(); if (Consume('}')) return true; std::string key; if (!String(key) || !Consume(':')) return false; Profile profile; if (!ProfileObject(profile)) return false; if (profile.name.empty()) profile.name = key; profiles[key] = profile; Skip(); if (Consume('}')) return true; if (!Consume(',')) return false; }
    }
    const std::string& m_input;
    std::size_t m_pos = 0;
};
}

ProfileStore::ProfileStore(std::string path) : m_path(std::move(path)) {}

bool ProfileStore::Save(const ProfileDocument& document) const {
    const std::string temp = m_path + ".tmp";
    std::ofstream file(temp.c_str(), std::ios::trunc | std::ios::binary);
    if (!file) return false;
    file << Serialize(document);
    file.close();
    if (!file) return false;
#ifdef _WIN32
    if (!CopyFileA(temp.c_str(), (m_path + ".bak").c_str(), FALSE)) {
        // A first save has no source backup; it remains valid to continue.
        DeleteFileA((m_path + ".bak").c_str());
        CopyFileA(temp.c_str(), (m_path + ".bak").c_str(), FALSE);
    }
    if (!MoveFileExA(temp.c_str(), m_path.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) return false;
#else
    std::remove((m_path + ".bak").c_str());
    std::rename(m_path.c_str(), (m_path + ".bak").c_str());
    return std::rename(temp.c_str(), m_path.c_str()) == 0;
#endif
    return true;
}

bool ProfileStore::Load(ProfileDocument& document) const {
    const std::string json = ReadAll(m_path);
    if (json.empty()) return false;
    return Parse(json, document);
}

bool ProfileStore::RecoverBackup(ProfileDocument& document) const {
    const std::string json = ReadAll(m_path + ".bak");
    if (json.empty()) return false;
    return Parse(json, document);
}

bool ProfileStore::Parse(const std::string& json, ProfileDocument& document) const {
    Parser parser(json);
    return parser.ParseDocument(document);
}

bool ProfileStore::MigrateIni(const std::string& iniPath, ProfileDocument& document) const {
    std::ifstream file(iniPath.c_str());
    if (!file) return false;
    document.schemaVersion = 1;
    document.currentProfile = "default";
    Profile& profile = document.profiles["default"];
    profile.name = "default";
    std::string line;
    while (std::getline(file, line)) {
        const std::size_t equal = line.find('=');
        if (equal == std::string::npos) continue;
        std::string key = line.substr(0, equal);
        std::string value = line.substr(equal + 1);
        if (key.empty()) continue;
        if (value == "0" || value == "1") profile.enabled[key] = value == "1";
        else profile.settings[key] = value;
    }
    return true;
}

bool ProfileStore::Clone(ProfileDocument& document, const std::string& source, const std::string& destination) const {
    if (destination.empty() || document.profiles.count(destination) || !document.profiles.count(source)) return false;
    document.profiles[destination] = document.profiles[source];
    document.profiles[destination].name = destination;
    return true;
}

bool ProfileStore::Rename(ProfileDocument& document, const std::string& source, const std::string& destination) const {
    if (destination.empty() || document.profiles.count(destination) || !document.profiles.count(source)) return false;
    Profile profile = document.profiles[source];
    profile.name = destination;
    document.profiles.erase(source);
    document.profiles[destination] = profile;
    if (document.currentProfile == source) document.currentProfile = destination;
    return true;
}

bool ProfileStore::Remove(ProfileDocument& document, const std::string& profile) const {
    if (document.profiles.size() <= 1 || !document.profiles.count(profile)) return false;
    document.profiles.erase(profile);
    if (document.currentProfile == profile) document.currentProfile = document.profiles.begin()->first;
    return true;
}

bool ProfileStore::SetCurrent(ProfileDocument& document, const std::string& profile) const {
    if (!document.profiles.count(profile)) return false;
    document.currentProfile = profile;
    return true;
}
