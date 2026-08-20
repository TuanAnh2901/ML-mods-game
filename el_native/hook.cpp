#include "hook.h"
#include "render.h"
#include "il2cpp_resolve.h"
#include "framework.h"
#include "feature.h"
#include "hook_registry.h"
#include "compatibility_patch.h"
#include "main_thread_dispatcher.h"
#include "..\minhook\include\MinHook.h"
#include "safe_call.h"
#include <cstdlib>

typedef void(__fastcall* UpdateFunc)(void*);
UpdateFunc OriginalUpdate = nullptr;
volatile LONG g_callCount = 0;

void __fastcall UpdateHook(void* __this) {
    InterlockedIncrement(&g_callCount);
    // Dispatcher tasks are already guarded per-task; this outer guard also
    // isolates the dispatcher's own bookkeeping on the game Update thread.
    ElGuard("core.update", [&] {
        GlobalMainThreadDispatcher().Tick();
        OriginalUpdate(__this);
    });
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

    Render_Init();

    for (auto* f : g_features) {
        if (!f) continue;
        ElGuard("feature.init", [&] { f->Init(); });
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
