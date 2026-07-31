#pragma once

#include "hook_registry.h"
#include <cstddef>
#include <cstdint>

struct CompatibilityPatchStatus {
    const char* name = nullptr;
    std::uintptr_t address = 0;
    HookStatus status = HookStatus::Unavailable;
};

// Installs only in-memory detector listener hooks after GameAssembly is
// available.  Every target is version/prologue resolved through the shared
// resolver and owns a HookRegistry entry.
void InstallAntiCheatEarlyPatches();
void RestoreAntiCheatEarlyPatches();
std::size_t CompatibilityPatchCount();
CompatibilityPatchStatus CompatibilityPatchAt(std::size_t index);
