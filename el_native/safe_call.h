#pragma once
#include <windows.h>
#include "framework.h"

// SEH guard for EL Native entry points.
//
// C++ try/catch CANNOT intercept access violations (0xC0000005) — that is a
// structured exception (SEH), and only __try/__except (or a VEH) can catch it.
//
// MSVC requires the function containing __try to have no local objects that
// need unwinding (C2712), so the guarded work is always a callable passed by
// reference: the guard frame itself stays unwind-free, and any destructors the
// lambda creates live in the lambda's own frame, not here.
//
// C++ exceptions thrown inside fn() are NOT caught (they are not SEH under
// /EHsc) and propagate normally. That is the right trade-off: the crash vector
// we isolate is a bad memory access in a feature hook, not a deliberate throw.

inline int ElSehFilter(const char* name, DWORD code, EXCEPTION_POINTERS* ep) {
    const void* fault = (ep && ep->ExceptionRecord) ? ep->ExceptionRecord->ExceptionAddress : nullptr;
    LOG("[SEH] %s fault code=0x%08X addr=%p", name ? name : "?", code, fault);
    return EXCEPTION_EXECUTE_HANDLER;
}

template <typename Fn>
inline void ElGuard(const char* name, Fn&& fn) {
    __try {
        fn();
    } __except (ElSehFilter(name, GetExceptionCode(), GetExceptionInformation())) {
        // handled; the filter already logged it
    }
}
