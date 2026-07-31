#include "hook.h"
#include "render.h"
#include "il2cpp_resolve.h"
#include "framework.h"
#include "feature.h"
#include "hook_registry.h"
#include "compatibility_patch.h"
#include "main_thread_dispatcher.h"
#include "..\minhook\include\MinHook.h"
#include <cstdlib>

typedef void(__fastcall* UpdateFunc)(void*);
UpdateFunc OriginalUpdate = nullptr;
volatile LONG g_callCount = 0;

void __fastcall UpdateHook(void* __this) {
    InterlockedIncrement(&g_callCount);
    GlobalMainThreadDispatcher().Tick();
    OriginalUpdate(__this);
}

DWORD WINAPI HookThread(LPVOID) {
    LOG("el_native: worker started");

    HMODULE hGA = nullptr;
    // Poll quickly enough to patch detector listeners before their first
    // startup callback, while still bounding DLL attach at 30 seconds.
    for (int i = 0; i < 1200; i++) {
        hGA = GetModuleHandleA("GameAssembly.dll");
        if (hGA) break;
        if (i == 1199) {
            LOG("el_native: TIMEOUT waiting for GameAssembly.dll");
            return 0;
        }
        Sleep(25);
    }

    uintptr_t base = (uintptr_t)hGA;
    LOG("el_native: base=0x%llX", base);

    MH_STATUS status = MH_Initialize();
    LOG("el_native: MH_Initialize = %d", status);

    // [RESOLVE] Initialize IL2CPP resolve engine
    ResolveInit();

    // Must run before overlay/render and before feature hooks.  All targets
    // are in-memory MinHook detours and are restored during detach.
    InstallAntiCheatEarlyPatches();
    // Metadata can lag the module load by a short interval. Retry unresolved
    // listener targets without touching already-hooked addresses.
    for (int retry = 0; retry < 40; ++retry) {
        bool pending = false;
        for (std::size_t i = 0; i < CompatibilityPatchCount(); ++i) {
            if (CompatibilityPatchAt(i).status == HookStatus::Unavailable) { pending = true; break; }
        }
        if (!pending) break;
        Sleep(25);
        InstallAntiCheatEarlyPatches();
    }

    // [P1] Resolve SM_destroyThisTimed::Update via API/fallback and hook
    void* resolvedTarget = ResolveMethodOrFallback(
        "Assembly-CSharp", "", "SM_destroyThisTimed", "Update", 0);
    LOG("[P1] SM_destroyThisTimed::Update resolved = %p (expected ~ base+0x6A3C00)",
        resolvedTarget);

    if (resolvedTarget && GlobalHookRegistry().Claim((uintptr_t)resolvedTarget, "core.update")) {
        GlobalHookRegistry().MarkResolved((uintptr_t)resolvedTarget);
        status = MH_CreateHook(resolvedTarget, &UpdateHook, (LPVOID*)&OriginalUpdate);
        LOG("el_native: MH_CreateHook = %d (original=%p)", status, OriginalUpdate);
        if (status == MH_OK) {
            status = MH_EnableHook(resolvedTarget);
            LOG("el_native: MH_EnableHook = %d", status);
            if (status == MH_OK) GlobalHookRegistry().MarkHooked((uintptr_t)resolvedTarget);
            else GlobalHookRegistry().MarkUnavailable((uintptr_t)resolvedTarget);
        }
    } else if (resolvedTarget) {
        LOG("el_native: Update hook conflict; owner=%s", GlobalHookRegistry().Owner((uintptr_t)resolvedTarget).c_str());
    } else {
        LOG("el_native: FATAL — could not resolve SM_destroyThisTimed::Update, hook SKIPPED");
    }

    char delayBuffer[32] = {};
    DWORD delayLength = GetEnvironmentVariableA("EL_NATIVE_OVERLAY_DELAY_MS", delayBuffer, sizeof(delayBuffer));
    DWORD overlayDelay = delayLength ? strtoul(delayBuffer, nullptr, 10) : 2000;
    Sleep(overlayDelay);
    LOG("[P1] overlay delay elapsed: %lu ms", overlayDelay);

    // [P1] Initialize D3D11 Present + ResizeBuffers hooks
    Render_Init();

    // [P1] Init features before loading typed profile settings.
    for (auto* f : g_features) {
        f->Init();
    }
    ConfigLoad();
    g_featuresReady = true;

    LOG("el_native: hook installed, waiting...");

    while (true) {
        Sleep(10000);
        LOG("el_native: call count = %ld", g_callCount);
    }

    return 0;
}
