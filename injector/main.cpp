#include "injector.h"
#include "launcher.h"
#include <stdio.h>
#include <tlhelp32.h>
#include <shlobj.h>
#include <string>
#include <fstream>
#include <iostream>

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

static std::wstring ConfigPath() {
    wchar_t appData[MAX_PATH] = {};
    SHGetFolderPathW(NULL, CSIDL_LOCAL_APPDATA, NULL, SHGFP_TYPE_CURRENT, appData);
    return std::wstring(appData) + L"\\EL_Native\\launcher.ini";
}

static std::wstring ReadStoredFolder() {
    std::wifstream file(ConfigPath().c_str());
    std::wstring folder;
    if (file) std::getline(file, folder);
    return folder;
}

static bool WriteStoredFolder(const std::wstring& folder) {
    const std::wstring path = ConfigPath();
    const std::wstring parent = path.substr(0, path.find_last_of(L'\\'));
    CreateDirectoryW(parent.c_str(), NULL);
    std::wofstream file(path.c_str(), std::ios::trunc);
    if (!file) return false;
    file << folder << L"\n";
    return true;
}

static std::string Narrow(const std::wstring& value) {
    if (value.empty()) return std::string();
    int size = WideCharToMultiByte(CP_UTF8, 0, value.c_str(), -1, NULL, 0, NULL, NULL);
    std::string output(size > 0 ? size : 0, '\0');
    if (!output.empty()) {
        WideCharToMultiByte(CP_UTF8, 0, value.c_str(), -1, &output[0], size, NULL, NULL);
        if (!output.empty() && output.back() == '\0') output.pop_back();
    }
    return output;
}

static std::wstring JoinPath(const std::wstring& left, const wchar_t* right) {
    return left + (left.empty() || left.back() == L'\\' ? L"" : L"\\") + right;
}

static void LaunchLog(const std::wstring& folder, const wchar_t* message) {
    const std::wstring path = JoinPath(folder, L"el_native_launcher.log");
    struct _stat64 st{};
    const bool rotate = _wstat64(path.c_str(), &st) == 0 && st.st_size >= 64 * 1024;
    std::wofstream file(path.c_str(), rotate ? std::ios::trunc : std::ios::app);
    if (file) file << message << L"\n";
    wprintf(L"%s\n", message);
}

struct SteamEnv {
    wchar_t appId[64] = {};
    wchar_t gameId[64] = {};
    DWORD appIdLength = 0;
    DWORD gameIdLength = 0;
    bool hadAppId = false;
    bool hadGameId = false;
};

static SteamEnv SetSteamEnvironment(const std::wstring& appId) {
    SteamEnv saved;
    saved.appIdLength = GetEnvironmentVariableW(L"SteamAppId", saved.appId, 64);
    saved.gameIdLength = GetEnvironmentVariableW(L"SteamGameId", saved.gameId, 64);
    saved.hadAppId = saved.appIdLength != 0;
    saved.hadGameId = saved.gameIdLength != 0;
    SetEnvironmentVariableW(L"SteamAppId", appId.c_str());
    SetEnvironmentVariableW(L"SteamGameId", appId.c_str());
    return saved;
}

static void RestoreSteamEnvironment(const SteamEnv& saved) {
    SetEnvironmentVariableW(L"SteamAppId", saved.hadAppId ? saved.appId : NULL);
    SetEnvironmentVariableW(L"SteamGameId", saved.hadGameId ? saved.gameId : NULL);
}

int wmain(int argc, wchar_t* argv[]) {
    bool configureFlag = false;
    for (int i = 1; i < argc; ++i) if (_wcsicmp(argv[i], L"--configure") == 0) configureFlag = true;

    std::wstring gameFolder = ReadStoredFolder();
    const bool shiftHeld = (GetAsyncKeyState(VK_SHIFT) & 0x8000) != 0;
    const LaunchMode mode = ResolveLaunchMode(!gameFolder.empty(), shiftHeld, configureFlag);
    if (mode == LaunchMode::Configure) {
        wprintf(L"Game folder (contains Everlusting Life.exe, GameAssembly.dll, UnityPlayer.dll): ");
        std::wstring input;
        std::getline(std::wcin, input);
        if (!input.empty()) gameFolder = input;
    }

    std::string error;
    if (!ValidateGameFolder(Narrow(gameFolder), "Everlusting Life.exe", error)) {
        wprintf(L"Configuration error: %S\n", error.c_str());
        return 1;
    }
    if (!WriteStoredFolder(gameFolder)) {
        wprintf(L"Configuration could not be saved\n");
        return 1;
    }

    const std::wstring gameExe = JoinPath(gameFolder, L"Everlusting Life.exe");
    const std::wstring dllPath = argc > 1 && argv[1][0] != L'-' ? argv[1] : JoinPath(gameFolder, L"el_native.dll");
    if (GetFileAttributesW(dllPath.c_str()) == INVALID_FILE_ATTRIBUTES) {
        wprintf(L"DLL missing: %s\n", dllPath.c_str());
        return 1;
    }

    LaunchLog(gameFolder, L"[launcher] validated game folder");

    const std::string steamAppId = DetectSteamAppId(Narrow(gameFolder));
    SteamEnv savedSteamEnv;
    bool steamEnvSet = false;
    if (!steamAppId.empty()) {
        LaunchLog(gameFolder, L"[launcher] Steam app id detected; exporting Steam context");
        const std::wstring appId(steamAppId.begin(), steamAppId.end());
        savedSteamEnv = SetSteamEnvironment(appId);
        steamEnvSet = true;
        LaunchLog(gameFolder, L"[launcher] SteamAppId/SteamGameId exported to child");
    } else {
        LaunchLog(gameFolder, L"[launcher] standalone folder detected; no Steam context required");
    }
    LaunchLog(gameFolder, L"[launcher] creating suspended process");

    if (!EnableDebugPrivilege()) {
        wprintf(L"Warning: Could not enable SeDebugPrivilege\n");
    }

    STARTUPINFOW si = { sizeof(si) };
    PROCESS_INFORMATION pi = {};

    if (!CreateProcessW(gameExe.c_str(), NULL, NULL, NULL, FALSE,
        CREATE_SUSPENDED | CREATE_UNICODE_ENVIRONMENT, NULL, gameFolder.c_str(), &si, &pi)) {
        if (steamEnvSet) RestoreSteamEnvironment(savedSteamEnv);
        wprintf(L"CreateProcessW failed: %lu\n", GetLastError());
        return 1;
    }
    if (steamEnvSet) RestoreSteamEnvironment(savedSteamEnv);

    printf("Process created: PID %lu\n", pi.dwProcessId);

    if (!InjectDLL(pi.hProcess, dllPath.c_str())) {
        printf("Injection failed\n");
        ResumeThread(pi.hThread);
        TerminateProcess(pi.hProcess, 1);
        CloseHandle(pi.hThread);
        CloseHandle(pi.hProcess);
        return 1;
    }

    ResumeThread(pi.hThread);
    LaunchLog(gameFolder, L"[launcher] DLL injected and process resumed");

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
