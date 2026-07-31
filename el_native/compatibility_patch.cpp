#include "compatibility_patch.h"

#include "framework.h"
#include "il2cpp_resolve.h"
#include "../minhook/include/MinHook.h"

#include <cstring>

namespace {
using VoidMethod = void(__fastcall*)(void*, void*);

struct Target {
    const char* name;
    const char* ns;
    const char* klass;
    const char* method;
    int argc;
    VoidMethod original;
    std::uintptr_t address;
    HookStatus status;
    bool hooked;
};

// These methods are listener/notification entry points with void returns.  We
// deliberately do not patch generic detector Start methods or network UI
// methods whose return contracts differ between game versions.
Target g_targets[] = {
    {"detector.network_listener", "CodeStage.AntiCheat.Detectors", "DetectorListenerWithNetworkWindow", "OnCheatingDetected", 0, nullptr, 0, HookStatus::Unavailable, false},
    {"detector.obscured_listener", "CodeStage.AntiCheat.Detectors", "ObscuredCheatingDetectorListener", "TryShowError", 0, nullptr, 0, HookStatus::Unavailable, false},
};

void __fastcall NoopDetectorListener(void*, void*) {
    // Compatibility-only listener suppression.  No request or game state is
    // modified and the original bytes remain restorable through MinHook.
}

void Install(Target& target) {
    if (target.hooked) return;
    void* address = ResolveMethodOrFallback("Assembly-CSharp", target.ns,
        target.klass, target.method, target.argc);
    if (!address) {
        LOG("[COMPAT] %s unresolved", target.name);
        return;
    }
    target.address = reinterpret_cast<std::uintptr_t>(address);
    if (!GlobalHookRegistry().Claim(target.address, target.name)) {
        target.status = HookStatus::Conflict;
        LOG("[COMPAT] %s conflict owner=%s", target.name,
            GlobalHookRegistry().Owner(target.address).c_str());
        return;
    }
    GlobalHookRegistry().MarkResolved(target.address);
    MH_STATUS status = MH_CreateHook(address, &NoopDetectorListener,
        reinterpret_cast<LPVOID*>(&target.original));
    if (status == MH_OK) status = MH_EnableHook(address);
    if (status == MH_OK) {
        target.hooked = true;
        target.status = HookStatus::Hooked;
        GlobalHookRegistry().MarkHooked(target.address);
        LOG("[COMPAT] %s hooked @ %p", target.name, address);
    } else {
        target.status = HookStatus::Unavailable;
        GlobalHookRegistry().MarkUnavailable(target.address);
        LOG("[COMPAT] %s hook failed status=%d", target.name, status);
    }
}
}

void InstallAntiCheatEarlyPatches() {
    for (auto& target : g_targets) Install(target);
}

void RestoreAntiCheatEarlyPatches() {
    for (auto& target : g_targets) {
        if (!target.hooked || !target.address) continue;
        MH_DisableHook(reinterpret_cast<LPVOID>(target.address));
        MH_RemoveHook(reinterpret_cast<LPVOID>(target.address));
        target.hooked = false;
        target.status = HookStatus::Unavailable;
        LOG("[COMPAT] %s restored", target.name);
    }
}

std::size_t CompatibilityPatchCount() {
    return sizeof(g_targets) / sizeof(g_targets[0]);
}

CompatibilityPatchStatus CompatibilityPatchAt(std::size_t index) {
    if (index >= CompatibilityPatchCount()) return {};
    const auto& target = g_targets[index];
    return {target.name, target.address, target.status};
}
