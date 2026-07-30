#pragma once
#include <windows.h>

BOOL InjectDLL(HANDLE hProcess, LPCWSTR dllPath);
BOOL EnableDebugPrivilege();
