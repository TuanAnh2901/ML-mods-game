#pragma once
#include <windows.h>
#include <stdint.h>
#include <cstddef>

struct ResolvedMethod {
    void* pointer = nullptr;
    void* methodInfo = nullptr;
    bool fromFallback = false;
};

// Resolve method pointer via IL2CPP API.
// Walks assemblies -> image -> class -> method to find the native pointer.
// Returns nullptr on failure (no crash).
void* ResolveMethod(const char* imageHint, const char* ns, const char* cls,
                    const char* method, int argc);

// One-time init of IL2CPP exports. Safe to call multiple times.
void ResolveInit();

// Resolve with thread-safe cache and fallback table.
// 1. Check cache (hit -> return cached ptr)
// 2. Try ResolveMethod API
// 3. If API fails, scan fallback RVA table (from method_fallback.inc)
// 4. If both fail, return nullptr
// LOGs [RESOLVE] with path used: cache / API / fallback / FAILED
void* ResolveMethodOrFallback(const char* imageHint, const char* ns,
                              const char* cls, const char* method, int argc);

// Method-info-aware lookup.  The API path walks parent classes, which is
// required for inherited generic UI methods such as WindowScriptCore<T>.
ResolvedMethod ResolveMethodInfoOrFallback(const char* imageHint,
    const char* ns, const char* cls, const char* method, int argc);

// Resolve an instance-field offset through IL2CPP metadata.  The lookup walks
// parent classes, so a field declared on UnitCore remains available when the
// requested class is BattleUnit.  Returns -1 when metadata is unavailable or
// the named field does not exist; callers must retain a method-based fallback.
int32_t ResolveFieldOffset(const char* imageHint, const char* ns,
                           const char* cls, const char* field);

// Runtime allocation helpers used by read-only/session UI features. They
// return nullptr when the corresponding IL2CPP export or class is unavailable.
void* ResolveRuntimeClass(const char* imageHint, const char* ns, const char* cls);
void* AllocateIl2CppArray(void* elementClass, std::size_t length);
