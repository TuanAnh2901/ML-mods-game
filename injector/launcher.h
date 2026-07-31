#pragma once

#include <string>

enum class LaunchMode { Configure, Launch };

LaunchMode ResolveLaunchMode(bool hasStoredFolder, bool shiftHeld, bool configureFlag);
bool ValidateGameFolder(const std::string& folder, const std::string& exeName, std::string& error);
// Returns the Steam app id from the library manifest containing folder, or an
// empty string for a standalone/non-Steam installation.
std::string DetectSteamAppId(const std::string& folder);
