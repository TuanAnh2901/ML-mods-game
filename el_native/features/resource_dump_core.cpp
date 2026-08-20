#include "resource_dump_core.h"

#include <algorithm>
#include <cstddef>

namespace {
struct Il2CppListLayout {
    void* klass;
    void* monitor;
    void* items;
    int32_t size;
};
}

ResourceTypeListParseResult ParseResourceTypeList(void* listObject) {
    ResourceTypeListParseResult result;
    if (!listObject) {
        result.failureReason = "null-list";
        return result;
    }

    const Il2CppListLayout* list = static_cast<const Il2CppListLayout*>(listObject);
    if (list->size <= 0 || list->size > 4096 || !list->items) {
        result.failureReason = list->size <= 0 ? "empty-list" : "invalid-list";
        return result;
    }

    const unsigned char* array = static_cast<const unsigned char*>(list->items);
    const int32_t* values = reinterpret_cast<const int32_t*>(array + 0x20);
    result.ids.assign(values, values + list->size);
    NormalizeResourceTypeIds(result.ids);
    if (result.ids.empty()) result.failureReason = "no-valid-ids";
    return result;
}

void NormalizeResourceTypeIds(std::vector<int32_t>& ids) {
    ids.erase(std::remove_if(ids.begin(), ids.end(), [](int32_t id) { return id < 0 || id > 1024; }), ids.end());
    std::sort(ids.begin(), ids.end());
    ids.erase(std::unique(ids.begin(), ids.end()), ids.end());
}

const char* SelectDisplayResourceName(const char* rawKey, const char* displayName) {
    if (displayName && displayName[0]) return displayName;
    if (rawKey && rawKey[0]) return rawKey;
    return "-";
}
