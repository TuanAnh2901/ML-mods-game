#pragma once
// Shared, thread-safe action trace for Automation + game UI events.  Header-only
// (inline function statics give one instance across TUs, same pattern as
// framework.h).  Events land in a ring for the overlay and in a JSONL file for
// post-session review.

#include <windows.h>
#include <cstdio>
#include <cstdarg>
#include <cstring>
#include <sys/stat.h>
#include <mutex>

namespace actiontrace {
constexpr int kRing = 64;

struct Event {
    unsigned long long tick = 0;
    char source[24] = {};
    char text[256] = {};
};

inline Event* Ring() { static Event r[kRing] = {}; return r; }
inline int* Head() { static int h = 0; return &h; }
inline int* Count() { static int c = 0; return &c; }
inline std::mutex& Mx() { static std::mutex m; return m; }
inline FILE*& File() { static FILE* f = nullptr; return f; }

inline void Push(const char* source, const char* fmt, ...) {
    char buf[256];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    {
        std::lock_guard<std::mutex> lock(Mx());
        Event& e = Ring()[*Head()];
        e.tick = GetTickCount64();
        strncpy_s(e.source, source, _TRUNCATE);
        strncpy_s(e.text, buf, _TRUNCATE);
        *Head() = (*Head() + 1) % kRing;
        if (*Count() < kRing) ++*Count();
    }
    FILE* f = File();
    if (!f) {
        struct _stat64 st{};
        const char* path = "el_native_action_trace.jsonl";
        const bool rotate = _stat64(path, &st) == 0 && st.st_size >= 512 * 1024;
        f = fopen(path, rotate ? "wb" : "ab");
        File() = f;
    }
    if (f) {
        fprintf(f, "{\"t\":%llu,\"s\":\"%s\",\"m\":\"%s\"}\n",
            GetTickCount64(), source ? source : "", buf);
        fflush(f);
    }
}

// Oldest-first snapshot of the ring (i <= Count()).
inline void Snapshot(Event* out, int* count) {
    if (!out || !count) return;
    std::lock_guard<std::mutex> lock(Mx());
    *count = *Count();
    for (int i = 0; i < *count; i++) {
        out[i] = Ring()[(*Head() - *count + i + kRing) % kRing];
    }
}
} // namespace actiontrace
