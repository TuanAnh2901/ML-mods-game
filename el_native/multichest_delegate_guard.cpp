#include "multichest_delegate_guard.h"

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include <cstddef>
#include <limits>

namespace {

constexpr size_t kInvokeImplOffset = 0x18;

DWORD BaseProtection(DWORD protection) {
    return protection & 0xff;
}

bool IsReadableProtection(DWORD protection) {
    switch (BaseProtection(protection)) {
    case PAGE_READONLY:
    case PAGE_READWRITE:
    case PAGE_WRITECOPY:
    case PAGE_EXECUTE_READ:
    case PAGE_EXECUTE_READWRITE:
    case PAGE_EXECUTE_WRITECOPY:
        return true;
    default:
        return false;
    }
}

bool IsWritableProtection(DWORD protection) {
    switch (BaseProtection(protection)) {
    case PAGE_READWRITE:
    case PAGE_WRITECOPY:
    case PAGE_EXECUTE_READWRITE:
    case PAGE_EXECUTE_WRITECOPY:
        return true;
    default:
        return false;
    }
}

bool IsExecutableProtection(DWORD protection) {
    switch (BaseProtection(protection)) {
    case PAGE_EXECUTE:
    case PAGE_EXECUTE_READ:
    case PAGE_EXECUTE_READWRITE:
    case PAGE_EXECUTE_WRITECOPY:
        return true;
    default:
        return false;
    }
}

bool HasProtection(uintptr_t address, size_t size, bool requireRead,
                   bool requireWrite, bool requireExecute) {
    if (address == 0 || size == 0 ||
        address > std::numeric_limits<uintptr_t>::max() - size) {
        return false;
    }

    const uintptr_t end = address + size;
    uintptr_t current = address;
    while (current < end) {
        MEMORY_BASIC_INFORMATION region{};
        if (VirtualQuery(reinterpret_cast<const void*>(current), &region, sizeof(region)) == 0 ||
            region.State != MEM_COMMIT || (region.Protect & (PAGE_GUARD | PAGE_NOACCESS)) != 0 ||
            (requireRead && !IsReadableProtection(region.Protect)) ||
            (requireWrite && !IsWritableProtection(region.Protect)) ||
            (requireExecute && !IsExecutableProtection(region.Protect))) {
            return false;
        }

        const uintptr_t regionBase = reinterpret_cast<uintptr_t>(region.BaseAddress);
        if (region.RegionSize == 0 ||
            regionBase > std::numeric_limits<uintptr_t>::max() - region.RegionSize) {
            return false;
        }
        const uintptr_t regionEnd = regionBase + region.RegionSize;
        if (regionEnd <= current) return false;
        current = regionEnd < end ? regionEnd : end;
    }
    return true;
}

bool ReadPointer(uintptr_t address, void** value) {
    if (!value || !HasProtection(address, sizeof(void*), true, false, false)) return false;
    __try {
        *value = *reinterpret_cast<void* const*>(address);
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        *value = nullptr;
        return false;
    }
}

bool WritePointer(uintptr_t address, void* value) {
    if (!HasProtection(address, sizeof(void*), true, true, false)) return false;
    __try {
        *reinterpret_cast<void**>(address) = value;
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}

bool IsSentinelPointer(void* value) {
    return reinterpret_cast<uintptr_t>(value) == std::numeric_limits<uintptr_t>::max();
}

}  // namespace

const char* MultichestDelegateGuardResultName(MultichestDelegateGuardResult result) {
    switch (result) {
    case MultichestDelegateGuardResult::InvalidInput: return "invalid-input";
    case MultichestDelegateGuardResult::NoDelegate: return "no-delegate";
    case MultichestDelegateGuardResult::ValidDelegate: return "valid-delegate";
    case MultichestDelegateGuardResult::ClearedInvalidDelegate: return "cleared-invalid-delegate";
    case MultichestDelegateGuardResult::FieldUnreadable: return "field-unreadable";
    case MultichestDelegateGuardResult::ClearFailed: return "clear-failed";
    default: return "unknown";
    }
}

bool GetLoadedModuleImageRange(const char* moduleName, uintptr_t* begin, uintptr_t* end) {
    if (!moduleName || !begin || !end) return false;
    *begin = 0;
    *end = 0;

    HMODULE module = GetModuleHandleA(moduleName);
    if (!module) return false;

    const auto base = reinterpret_cast<uintptr_t>(module);
    const auto* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(base);
    if (dos->e_magic != IMAGE_DOS_SIGNATURE || dos->e_lfanew <= 0) return false;
    const uintptr_t ntAddress = base + static_cast<uintptr_t>(dos->e_lfanew);
    const auto* nt = reinterpret_cast<const IMAGE_NT_HEADERS64*>(ntAddress);
    if (nt->Signature != IMAGE_NT_SIGNATURE || nt->OptionalHeader.SizeOfImage == 0 ||
        base > std::numeric_limits<uintptr_t>::max() - nt->OptionalHeader.SizeOfImage) {
        return false;
    }

    *begin = base;
    *end = base + nt->OptionalHeader.SizeOfImage;
    return true;
}

MultichestDelegateGuardResult SanitizeMultichestBeforeHideAction(
    void* window, int32_t fieldOffset, uintptr_t moduleBegin, uintptr_t moduleEnd,
    void** observedDelegate, void** observedInvokeImpl) {
    if (observedDelegate) *observedDelegate = nullptr;
    if (observedInvokeImpl) *observedInvokeImpl = nullptr;
    if (!window || fieldOffset < 0 || moduleBegin == 0 || moduleEnd <= moduleBegin) {
        return MultichestDelegateGuardResult::InvalidInput;
    }

    const uintptr_t windowAddress = reinterpret_cast<uintptr_t>(window);
    const uintptr_t offset = static_cast<uintptr_t>(fieldOffset);
    if (windowAddress > std::numeric_limits<uintptr_t>::max() - offset) {
        return MultichestDelegateGuardResult::InvalidInput;
    }
    const uintptr_t fieldAddress = windowAddress + offset;

    void* delegate = nullptr;
    if (!ReadPointer(fieldAddress, &delegate)) {
        return MultichestDelegateGuardResult::FieldUnreadable;
    }
    if (observedDelegate) *observedDelegate = delegate;
    if (!delegate) return MultichestDelegateGuardResult::NoDelegate;

    void* invokeImpl = nullptr;
    const uintptr_t delegateAddress = reinterpret_cast<uintptr_t>(delegate);
    bool valid = !IsSentinelPointer(delegate) &&
                 (delegateAddress % alignof(void*) == 0) &&
                 delegateAddress <= std::numeric_limits<uintptr_t>::max() - kInvokeImplOffset &&
                 ReadPointer(delegateAddress + kInvokeImplOffset, &invokeImpl);
    if (observedInvokeImpl) *observedInvokeImpl = invokeImpl;

    const uintptr_t invokeAddress = reinterpret_cast<uintptr_t>(invokeImpl);
    valid = valid && invokeImpl && !IsSentinelPointer(invokeImpl) &&
            invokeAddress >= moduleBegin && invokeAddress < moduleEnd &&
            HasProtection(invokeAddress, 1, false, false, true);
    if (valid) return MultichestDelegateGuardResult::ValidDelegate;

    return WritePointer(fieldAddress, nullptr)
        ? MultichestDelegateGuardResult::ClearedInvalidDelegate
        : MultichestDelegateGuardResult::ClearFailed;
}

void* ForwardMultichestShowWithGuard(
    MultichestShowFn original, uintptr_t moduleBegin, uintptr_t moduleEnd,
    void* provider, void* controller, void* adsManager, int32_t firstChestRewardCount,
    void* onClose, void* beforeHideAction, void* methodInfo,
    MultichestDelegateGuardResult* guardResult, void** observedInvokeImpl) {
    if (guardResult) *guardResult = MultichestDelegateGuardResult::InvalidInput;
    if (observedInvokeImpl) *observedInvokeImpl = nullptr;
    if (!original) return nullptr;

    void* forwardedBeforeHideAction = beforeHideAction;
    const MultichestDelegateGuardResult result = SanitizeMultichestBeforeHideAction(
        &forwardedBeforeHideAction, 0, moduleBegin, moduleEnd, nullptr, observedInvokeImpl);
    if (result == MultichestDelegateGuardResult::InvalidInput ||
        result == MultichestDelegateGuardResult::FieldUnreadable ||
        result == MultichestDelegateGuardResult::ClearFailed) {
        forwardedBeforeHideAction = nullptr;
    }
    if (guardResult) *guardResult = result;

    return original(provider, controller, adsManager, firstChestRewardCount,
        onClose, forwardedBeforeHideAction, methodInfo);
}
