#include "injector.h"
#include <stdio.h>
#include <tlhelp32.h>

static DWORD FindProcessByName(LPCWSTR name) {
    HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (hSnapshot == INVALID_HANDLE_VALUE)
        return 0;

    PROCESSENTRY32W pe = { sizeof(pe) };
    DWORD pid = 0;

    if (Process32FirstW(hSnapshot, &pe)) {
        do {
            if (_wcsicmp(pe.szExeFile, name) == 0) {
                pid = pe.th32ProcessID;
                break;
            }
        } while (Process32NextW(hSnapshot, &pe));
    }

    CloseHandle(hSnapshot);
    return pid;
}

int wmain(int argc, wchar_t* argv[]) {
    if (argc < 2) {
        wprintf(L"Usage:\n");
        wprintf(L"  Mode 1 (create+inject): injector.exe <game_exe_path> <dll_path>\n");
        wprintf(L"  Mode 2 (attach+inject): injector.exe <dll_path>\n");
        return 1;
    }

    if (!EnableDebugPrivilege()) {
        wprintf(L"Warning: Could not enable SeDebugPrivilege\n");
    }

    if (argc == 2) {
        DWORD pid = FindProcessByName(L"Everlusting Life.exe");
        if (pid == 0) {
            wprintf(L"Error: Everlusting Life.exe not found. Is the game running?\n");
            return 1;
        }

        printf("Found process PID %lu, injecting...\n", pid);

        HANDLE hProcess = OpenProcess(PROCESS_ALL_ACCESS, FALSE, pid);
        if (!hProcess) {
            wprintf(L"OpenProcess failed: %lu\n", GetLastError());
            return 1;
        }

        BOOL ok = InjectDLL(hProcess, argv[1]);
        CloseHandle(hProcess);

        if (!ok) {
            printf("Injection failed\n");
            return 1;
        }

        printf("DLL injected into running process\n");
        return 0;
    }

    STARTUPINFOW si = { sizeof(si) };
    PROCESS_INFORMATION pi;

    if (!CreateProcessW(argv[1], NULL, NULL, NULL, FALSE,
        CREATE_SUSPENDED, NULL, NULL, &si, &pi)) {
        wprintf(L"CreateProcessW failed: %lu\n", GetLastError());
        return 1;
    }

    printf("Process created: PID %lu\n", pi.dwProcessId);

    if (!InjectDLL(pi.hProcess, argv[2])) {
        printf("Injection failed\n");
        TerminateProcess(pi.hProcess, 1);
        return 1;
    }

    ResumeThread(pi.hThread);
    printf("DLL injected, thread resumed\n");

    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
    return 0;
}

BOOL EnableDebugPrivilege() {
    HANDLE hToken;
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES, &hToken))
        return FALSE;
    TOKEN_PRIVILEGES tp = { 1, { { LUID{ 0,0 }, SE_PRIVILEGE_ENABLED } } };
    LookupPrivilegeValueA(NULL, SE_DEBUG_NAME, &tp.Privileges[0].Luid);
    BOOL result = AdjustTokenPrivileges(hToken, FALSE, &tp, sizeof(tp), NULL, NULL);
    CloseHandle(hToken);
    return result;
}

BOOL InjectDLL(HANDLE hProcess, LPCWSTR dllPath) {
    SIZE_T pathLen = (wcslen(dllPath) + 1) * sizeof(wchar_t);
    LPVOID remoteMem = VirtualAllocEx(hProcess, NULL, pathLen,
        MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    if (!remoteMem) return FALSE;

    if (!WriteProcessMemory(hProcess, remoteMem, dllPath, pathLen, NULL)) {
        VirtualFreeEx(hProcess, remoteMem, 0, MEM_RELEASE);
        return FALSE;
    }

    LPVOID loadLib = (LPVOID)GetProcAddress(GetModuleHandleW(L"kernel32.dll"), "LoadLibraryW");
    HANDLE hThread = CreateRemoteThread(hProcess, NULL, 0,
        (LPTHREAD_START_ROUTINE)loadLib, remoteMem, 0, NULL);
    if (!hThread) {
        VirtualFreeEx(hProcess, remoteMem, 0, MEM_RELEASE);
        return FALSE;
    }

    WaitForSingleObject(hThread, INFINITE);
    CloseHandle(hThread);
    VirtualFreeEx(hProcess, remoteMem, 0, MEM_RELEASE);
    return TRUE;
}
