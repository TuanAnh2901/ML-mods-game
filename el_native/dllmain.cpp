#include <windows.h>
#include "hook.h"
#include "framework.h"

BOOL APIENTRY DllMain(HMODULE hModule, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(hModule);

        char dllPath[MAX_PATH];
        DWORD len = GetModuleFileNameA(hModule, dllPath, MAX_PATH);
        LOG_PHASE("P1", "el_native.dll loaded from %s", len ? dllPath : "unknown");

        HANDLE hThread = CreateThread(NULL, 0, HookThread, hModule, 0, NULL);
        if (hThread) CloseHandle(hThread);
    }
    return TRUE;
}
