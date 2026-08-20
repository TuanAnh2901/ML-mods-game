#include "resource_dump.h"
#include "resource_dump_core.h"
#include "../framework.h"
#include "../il2cpp_resolve.h"
#include "../feature.h"
#include "../../third_party/imgui/imgui.h"
#include <cstdio>
#include <cstring>
#include <vector>

typedef void* (__fastcall* GetAllResourceTypes_t)(void* methodInfo);
typedef void* (__fastcall* GetResourceTypeName_t)(int32_t type, void* methodInfo);

enum class ResourceDumpResolveSource {
    None,
    RuntimeList,
    ProbeFallback,
};

static const char* ResourceDumpResolveSourceLabel(ResourceDumpResolveSource source) {
    switch (source) {
    case ResourceDumpResolveSource::RuntimeList: return "runtime-list";
    case ResourceDumpResolveSource::ProbeFallback: return "probe-fallback";
    default: return "unresolved";
    }
}

static bool IsMappedResourceTypeName(int32_t resType, const char* name) {
    if (!name || !name[0]) return false;

    char selfName[16];
    snprintf(selfName, sizeof(selfName), "%d", resType);
    if (strcmp(name, selfName) == 0) return false;
    if (resType != 0 && strcmp(name, "no") == 0) return false;
    return true;
}

static void* CallGetAllResourceTypesSafely(void* method, bool* faulted) {
    *faulted = false;
    void* result = nullptr;
    __try {
        result = ((GetAllResourceTypes_t)method)(nullptr);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        *faulted = true;
    }
    return result;
}

static void* CallGetResourceTypeNameSafely(GetResourceTypeName_t resolver, int32_t id, bool* faulted) {
    *faulted = false;
    void* result = nullptr;
    __try {
        result = resolver(id, nullptr);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        *faulted = true;
    }
    return result;
}

static const int IL2CPP_STRING_LEN_OFFSET = 0x10;
static const int IL2CPP_STRING_CHARS_OFFSET = 0x14;

static int ReadIl2CppString(void* str, char* buf, int bufSize) {
    if (!str || !buf || bufSize <= 0) return 0;
    int32_t len = *(int32_t*)((uintptr_t)str + IL2CPP_STRING_LEN_OFFSET);
    if (len <= 0 || len > 4096) { buf[0] = '\0'; return 0; }
    const wchar_t* chars = (const wchar_t*)((uintptr_t)str + IL2CPP_STRING_CHARS_OFFSET);
    int result = WideCharToMultiByte(CP_UTF8, 0, chars, len, buf, bufSize - 1, NULL, NULL);
    buf[result > 0 ? result : 0] = '\0';
    return result;
}

static const char* DUMP_FILE = "el_native_resources.txt";
static const int32_t RES_TYPE_PROBE_MAX = 1024;

ResourceTypeDumpFeature::ResourceTypeDumpFeature() {
    name = "ResourceDump";
    enabled = false;
}

void ResourceTypeDumpFeature::Init() {
    LOG("[FEATURE] ResourceDump::Init() starting");

    m_getAllResourceTypes = ResolveMethodOrFallback(
        "Assembly-CSharp", "", "UserResourcesUtil", "GetAllResourceTypes", 0);
    m_getStringByResourceType = ResolveMethodOrFallback(
        "Assembly-CSharp", "", "UserResourcesUtil", "GetStringByResourceType", 1);
    m_getResourceName = ResolveMethodOrFallback(
        "Assembly-CSharp", "", "UserResourcesUtil", "GetResourceName", 1);
    m_resolveFailed = (m_getStringByResourceType == nullptr);

    LOG("[FEATURE] ResourceDump resolve: GetAllResourceTypes=%p GetStringByResourceType=%p GetResourceName=%p",
        m_getAllResourceTypes, m_getStringByResourceType, m_getResourceName);

    if (m_resolveFailed)
        LOG("[FEATURE] ResourceDump FATAL: GetStringByResourceType not resolved, dump disabled");
    else
        LOG("[FEATURE] ResourceDump ready — will dump on first frame");
}

void ResourceTypeDumpFeature::OnUpdate() {
    if (m_dumped || m_resolveFailed) return;
    m_dumped = true;

    std::vector<int32_t> ids;
    ResourceDumpResolveSource source = ResourceDumpResolveSource::None;
    const char* reason = "GetAllResourceTypes unavailable";

    if (m_getAllResourceTypes) {
        bool faulted = false;
        void* listObject = CallGetAllResourceTypesSafely(m_getAllResourceTypes, &faulted);
        if (faulted) {
            reason = "GetAllResourceTypes fault";
        } else {
            ResourceTypeListParseResult parsed = ParseResourceTypeList(listObject);
            if (!parsed.ids.empty()) {
                ids.swap(parsed.ids);
                source = ResourceDumpResolveSource::RuntimeList;
                reason = "runtime resource list";
            } else {
                reason = parsed.failureReason ? parsed.failureReason : "invalid runtime list";
            }
        }
    }

    GetResourceTypeName_t rawResolver = (GetResourceTypeName_t)m_getStringByResourceType;
    GetResourceTypeName_t displayResolver = (GetResourceTypeName_t)m_getResourceName;
    if (source == ResourceDumpResolveSource::None) {
        LOG("[FEATURE] ResourceDump: GetAllResourceTypes unavailable / faulted; skipping probe during startup to prevent engine fault");
        return;
    }
    NormalizeResourceTypeIds(ids);

    m_resolveSource = (int)source;
    snprintf(m_sourceReason, sizeof(m_sourceReason), "%s", reason);
    LOG("[FEATURE] ResourceDump::OnUpdate source=%s reason=%s candidates=%u",
        ResourceDumpResolveSourceLabel(source), m_sourceReason, (unsigned)ids.size());

    FILE* dump = fopen(DUMP_FILE, "w");
    if (dump) {
        fprintf(dump, "# ResourceType dump\n");
        fprintf(dump, "# Source: %s\n", ResourceDumpResolveSourceLabel(source));
        fprintf(dump, "# id\traw_key\tdisplay_name\n");
    } else {
        LOG("[FEATURE] ResourceDump: failed to open dump file %s", DUMP_FILE);
    }

    m_entryCount = 0;
    for (int32_t id : ids) {
        bool rawFaulted = false;
        void* rawObject = CallGetResourceTypeNameSafely(rawResolver, id, &rawFaulted);
        if (rawFaulted) {
            LOG("[FEATURE] ResourceDump: GetStringByResourceType(%d) fault", id);
            continue;
        }

        char rawKey[256] = {};
        if (ReadIl2CppString(rawObject, rawKey, sizeof(rawKey)) <= 0 || !IsMappedResourceTypeName(id, rawKey))
            continue;

        char displayName[256] = {};
        if (displayResolver) {
            bool displayFaulted = false;
            void* displayObject = CallGetResourceTypeNameSafely(displayResolver, id, &displayFaulted);
            if (!displayFaulted)
                ReadIl2CppString(displayObject, displayName, sizeof(displayName));
        }
        const char* selectedDisplay = SelectDisplayResourceName(nullptr, displayName);
        if (!displayName[0]) selectedDisplay = "-";

        ++m_entryCount;
        LOG("[FEATURE] ResourceType[%d] raw=\"%s\" display=\"%s\"", id, rawKey, selectedDisplay);
        if (dump) fprintf(dump, "%d\t%s\t%s\n", id, rawKey, selectedDisplay);
    }

    if (dump) {
        fclose(dump);
        LOG("[FEATURE] ResourceDump: wrote %d entries to %s", m_entryCount, DUMP_FILE);
    }
    LOG("[FEATURE] ResourceDump complete source=%s reason=%s", ResourceDumpResolveSourceLabel(source), m_sourceReason);
}

void ResourceTypeDumpFeature::OnMenu() {
    if (m_resolveFailed) {
        ImGui::TextColored(ImVec4(1,0,0,1), "ResourceDump: raw resolver failed");
        return;
    }
    if (m_dumped) {
        ImGui::Text("ResourceDump: %d entries -> %s", m_entryCount, DUMP_FILE);
        ImGui::Text("Source: %s (%s)", ResourceDumpResolveSourceLabel((ResourceDumpResolveSource)m_resolveSource), m_sourceReason);
    } else {
        ImGui::Text("ResourceDump: pending...");
    }
}

static ResourceTypeDumpFeature g_resourceDump;
static int g_dumpRegistered = (RegisterFeature(&g_resourceDump), 0);
