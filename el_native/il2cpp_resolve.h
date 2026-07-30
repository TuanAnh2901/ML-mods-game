#pragma once
#include <windows.h>
#include <stdint.h>

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
