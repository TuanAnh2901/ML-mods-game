#pragma once
#include <stdint.h>
#include <windows.h>

#define RESOLVE_LOG_MAX 20
#define RESOLVE_NAME_MAX 128

enum ResolveSource {
    RESOLVE_SOURCE_API = 0,
    RESOLVE_SOURCE_FALLBACK = 1,
    RESOLVE_SOURCE_FAIL = 2,
    RESOLVE_SOURCE_CACHE = 3,
};

struct ResolveEntry {
    char name[RESOLVE_NAME_MAX];
    uintptr_t ptr;
    int source;
};

void LogResolve(const char* name, uintptr_t ptr, int source);
int GetResolveLogCount();
const ResolveEntry* GetResolveLogEntry(int index);
