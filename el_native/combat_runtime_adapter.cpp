#include "combat_runtime_adapter.h"

#include <windows.h>
#include <algorithm>
#include <cstdint>

namespace {
void* ReadPointer(const unsigned char* address) {
    if (!address) return nullptr;
    void* value = nullptr;
    __try {
        value = *reinterpret_cast<void* const*>(address);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return nullptr;
    }
    return value;
}

int ReadSize(const unsigned char* address) {
    if (!address) return 0;
    int value = 0;
    __try {
        value = *reinterpret_cast<const int*>(address);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return 0;
    }
    return value;
}
}

std::vector<void*> ExtractIl2CppListPointers(void* list, std::size_t maxCount) {
    std::vector<void*> result;
    if (!list || maxCount == 0) return result;
    const auto* base = reinterpret_cast<const unsigned char*>(list);
    const auto* array = reinterpret_cast<const unsigned char*>(ReadPointer(base + 0x10));
    const int size = ReadSize(base + 0x18);
    if (!array || size <= 0) return result;
    const std::size_t count = std::min<std::size_t>(static_cast<std::size_t>(size), maxCount);
    result.reserve(count);
    for (std::size_t i = 0; i < count; ++i) {
        void* value = ReadPointer(array + 0x20 + i * sizeof(void*));
        if (value) result.push_back(value);
    }
    return result;
}
