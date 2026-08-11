#pragma once

#include <cstdint>

enum class MultichestDelegateGuardResult {
    InvalidInput,
    NoDelegate,
    ValidDelegate,
    ClearedInvalidDelegate,
    FieldUnreadable,
    ClearFailed,
};

using MultichestShowFn = void*(__fastcall*)(
    void* provider, void* controller, void* adsManager, int32_t firstChestRewardCount,
    void* onClose, void* beforeHideAction, void* methodInfo);

const char* MultichestDelegateGuardResultName(MultichestDelegateGuardResult result);

bool GetLoadedModuleImageRange(const char* moduleName, uintptr_t* begin, uintptr_t* end);

MultichestDelegateGuardResult SanitizeMultichestBeforeHideAction(
    void* window, int32_t fieldOffset, uintptr_t moduleBegin, uintptr_t moduleEnd,
    void** observedDelegate = nullptr, void** observedInvokeImpl = nullptr);

void* ForwardMultichestShowWithGuard(
    MultichestShowFn original, uintptr_t moduleBegin, uintptr_t moduleEnd,
    void* provider, void* controller, void* adsManager, int32_t firstChestRewardCount,
    void* onClose, void* beforeHideAction, void* methodInfo,
    MultichestDelegateGuardResult* guardResult = nullptr,
    void** observedInvokeImpl = nullptr);
