#pragma once
#include <windows.h>
#include <cstdio>
#include <cstdarg>
#include <sys/stat.h>

// Single shared FILE* for the whole DLL. An inline function's static local has
// one instance across all TUs, so no extra .cpp / build.bat entry is needed.
// Previously every LOG did fopen+fflush+fclose, which made per-request NetLog
// logging a measurable frame cost.
inline void LogWrite(const char* buf)
{
    OutputDebugStringA(buf);
    static FILE* f = []() -> FILE* {
        struct _stat64 st{};
        const char* path = "el_native.log";
        const bool rotate = _stat64(path, &st) == 0 && st.st_size >= 256 * 1024;
        return fopen(path, rotate ? "w" : "a");
    }();
    if (f) { fputs(buf, f); fputc('\n', f); fflush(f); }
}

inline void LogFmt(const char* fmt, ...)
{
    char buf[512];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    LogWrite(buf);
}

// Phase-prefixed log for phase gate contract
// Usage: LOG_PHASE("P1", "msg %d", val) -> "[P1] msg val"
#define LOG_PHASE(phase, fmt, ...) LogFmt("[" phase "] " fmt, ##__VA_ARGS__)

// Backward-compatible LOG (no phase prefix)
#define LOG(fmt, ...) LogFmt(fmt, ##__VA_ARGS__)
