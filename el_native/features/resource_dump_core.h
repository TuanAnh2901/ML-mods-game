#pragma once

#include <cstdint>
#include <vector>

struct ResourceTypeListParseResult {
    std::vector<int32_t> ids;
    const char* failureReason = nullptr;
};

ResourceTypeListParseResult ParseResourceTypeList(void* listObject);
void NormalizeResourceTypeIds(std::vector<int32_t>& ids);
const char* SelectDisplayResourceName(const char* rawKey, const char* displayName);
