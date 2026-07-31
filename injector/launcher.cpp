#include "launcher.h"

#include <windows.h>
#include <algorithm>
#include <cctype>
#include <fstream>
#include <sstream>

namespace {
std::string Join(const std::string& left, const std::string& right) {
    if (left.empty()) return right;
    const char last = left[left.size() - 1];
    return left + ((last == '\\' || last == '/') ? "" : "\\") + right;
}

bool Exists(const std::string& path) {
    const DWORD attr = GetFileAttributesA(path.c_str());
    return attr != INVALID_FILE_ATTRIBUTES && !(attr & FILE_ATTRIBUTE_DIRECTORY);
}

std::string Lower(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(),
        [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return value;
}

std::string Leaf(const std::string& path) {
    const std::size_t end = path.find_last_not_of("\\/");
    if (end == std::string::npos) return {};
    const std::size_t begin = path.find_last_of("\\/", end);
    return path.substr(begin == std::string::npos ? 0 : begin + 1, end - (begin == std::string::npos ? 0 : begin + 1) + 1);
}

std::string Parent(const std::string& path) {
    const std::size_t end = path.find_last_not_of("\\/");
    const std::size_t cut = path.find_last_of("\\/", end == std::string::npos ? 0 : end);
    return cut == std::string::npos ? std::string() : path.substr(0, cut);
}

std::string ReadQuoted(const std::string& text, const char* key) {
    const std::string needle = std::string("\"") + key + "\"";
    const std::size_t at = text.find(needle);
    if (at == std::string::npos) return {};
    const std::size_t first = text.find('"', at + needle.size());
    if (first == std::string::npos) return {};
    const std::size_t second = text.find('"', first + 1);
    return second == std::string::npos ? std::string() : text.substr(first + 1, second - first - 1);
}
}

LaunchMode ResolveLaunchMode(bool hasStoredFolder, bool shiftHeld, bool configureFlag) {
    return (!hasStoredFolder || shiftHeld || configureFlag) ? LaunchMode::Configure : LaunchMode::Launch;
}

bool ValidateGameFolder(const std::string& folder, const std::string& exeName, std::string& error) {
    if (folder.empty()) { error = "folder is empty"; return false; }
    const DWORD attr = GetFileAttributesA(folder.c_str());
    if (attr == INVALID_FILE_ATTRIBUTES || !(attr & FILE_ATTRIBUTE_DIRECTORY)) {
        error = "folder does not exist";
        return false;
    }
    if (!Exists(Join(folder, exeName))) { error = "game executable missing"; return false; }
    if (!Exists(Join(folder, "GameAssembly.dll"))) { error = "GameAssembly.dll missing"; return false; }
    if (!Exists(Join(folder, "UnityPlayer.dll"))) { error = "UnityPlayer.dll missing"; return false; }
    error.clear();
    return true;
}

std::string DetectSteamAppId(const std::string& folder) {
    // Steam's supported offline/developer launch path is a small
    // steam_appid.txt beside the executable. Prefer it over any library
    // probing so a copied/non-default install behaves exactly like Steam.
    {
        std::ifstream appIdFile(Join(folder, "steam_appid.txt").c_str());
        std::string appId;
        if (appIdFile && std::getline(appIdFile, appId)) {
            appId.erase(std::remove_if(appId.begin(), appId.end(),
                [](unsigned char c) { return std::isspace(c) != 0; }), appId.end());
            if (!appId.empty() && std::all_of(appId.begin(), appId.end(),
                [](unsigned char c) { return std::isdigit(c) != 0; }))
                return appId;
        }
    }
    // A Steam install has ...\\steamapps\\common\\<installdir>. Walk from
    // the selected folder instead of assuming the default C: library.
    const std::string common = Parent(folder);
    if (Lower(Leaf(common)) != "common") return {};
    const std::string steamapps = Parent(common);
    if (Lower(Leaf(steamapps)) != "steamapps") return {};

    WIN32_FIND_DATAA data = {};
    const std::string pattern = Join(steamapps, "appmanifest_*.acf");
    HANDLE find = FindFirstFileA(pattern.c_str(), &data);
    if (find == INVALID_HANDLE_VALUE) return {};
    const std::string selected = Lower(Leaf(folder));
    std::string result;
    do {
        const std::string manifest = Join(steamapps, data.cFileName);
        std::ifstream file(manifest.c_str(), std::ios::binary);
        std::stringstream buffer;
        buffer << file.rdbuf();
        const std::string text = buffer.str();
        if (Lower(ReadQuoted(text, "installdir")) == selected) {
            result = ReadQuoted(text, "appid");
            break;
        }
    } while (FindNextFileA(find, &data));
    FindClose(find);
    return result;
}
