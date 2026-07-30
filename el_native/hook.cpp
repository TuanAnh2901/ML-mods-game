#include "hook.h"
#include "render.h"
#include "il2cpp_resolve.h"
#include "framework.h"
#include "feature.h"
#include "..\minhook\include\MinHook.h"

typedef void(__fastcall* UpdateFunc)(void*);
UpdateFunc OriginalUpdate = nullptr;
volatile LONG g_callCount = 0;

void __fastcall UpdateHook(void* __this) {
    InterlockedIncrement(&g_callCount);
    OriginalUpdate(__this);
}

DWORD WINAPI HookThread(LPVOID) {
    LOG("el_native: worker started");

    HMODULE hGA = nullptr;
    for (int i = 0; i < 15; i++) {
        hGA = GetModuleHandleA("GameAssembly.dll");
        if (hGA) break;
        if (i == 14) {
            LOG("el_native: TIMEOUT waiting for GameAssembly.dll");
            return 0;
        }
        Sleep(2000);
    }

    uintptr_t base = (uintptr_t)hGA;
    LOG("el_native: base=0x%llX", base);

    MH_STATUS status = MH_Initialize();
    LOG("el_native: MH_Initialize = %d", status);

    // [RESOLVE] Initialize IL2CPP resolve engine
    ResolveInit();

    // [P1] Resolve SM_destroyThisTimed::Update via API/fallback and hook
    void* resolvedTarget = ResolveMethodOrFallback(
        "Assembly-CSharp", "", "SM_destroyThisTimed", "Update", 0);
    LOG("[P1] SM_destroyThisTimed::Update resolved = %p (expected ~ base+0x6A3C00)",
        resolvedTarget);

    if (resolvedTarget) {
        status = MH_CreateHook(resolvedTarget, &UpdateHook, (LPVOID*)&OriginalUpdate);
        LOG("el_native: MH_CreateHook = %d (original=%p)", status, OriginalUpdate);
        if (status == MH_OK) {
            status = MH_EnableHook(resolvedTarget);
            LOG("el_native: MH_EnableHook = %d", status);
        }
    } else {
        LOG("el_native: FATAL — could not resolve SM_destroyThisTimed::Update, hook SKIPPED");
    }

    // [P1] Initialize D3D11 Present + ResizeBuffers hooks
    Render_Init();

    // [P1] Load config and init features
    ConfigLoad();
    for (auto* f : g_features) {
        f->Init();
    }
    g_featuresReady = true;

    LOG("el_native: hook installed, waiting...");

    while (true) {
        Sleep(10000);
        LOG("el_native: call count = %ld", g_callCount);
    }

    return 0;
}
