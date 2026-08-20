#pragma once
#include <windows.h>

inline void LogWrite(const char* buf)
{
    if (!buf) return;
    char stamp[600];
    SYSTEMTIME st;
    GetLocalTime(&st);
    wsprintfA(stamp, "[%02u:%02u:%02u.%03u] %s\r\n",
        (unsigned)st.wHour, (unsigned)st.wMinute, (unsigned)st.wSecond, (unsigned)st.wMilliseconds, buf);
    OutputDebugStringA(stamp);

    HANDLE hFile = CreateFileA("el_native.log", FILE_APPEND_DATA, FILE_SHARE_READ | FILE_SHARE_WRITE,
        NULL, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile != INVALID_HANDLE_VALUE) {
        DWORD written = 0;
        DWORD len = (DWORD)lstrlenA(stamp);
        WriteFile(hFile, stamp, len, &written, NULL);
        CloseHandle(hFile);
    }
}

inline void LogFmt(const char* fmt, ...)
{
    if (!fmt) return;
    char buf[512];
    va_list ap;
    va_start(ap, fmt);
    wvsprintfA(buf, fmt, ap);
    va_end(ap);
    LogWrite(buf);
}

#define LOG_PHASE(phase, fmt, ...) LogFmt("[" phase "] " fmt, ##__VA_ARGS__)
#define LOG(fmt, ...) LogFmt(fmt, ##__VA_ARGS__)

