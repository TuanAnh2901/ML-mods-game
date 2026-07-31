#include "mascot_trace.h"

#include "../config_registry.h"
#include "../framework.h"
#include "../hook_registry.h"
#include "../il2cpp_resolve.h"
#include "../../minhook/include/MinHook.h"
#include "imgui.h"

#include <cstdint>
#include <cstring>
#include <string>
#include <unordered_set>
#include <vector>

namespace {
using GetCollectionFn = void*(__fastcall*)(void*, void*);
using GetInstanceFn = void*(__fastcall*)(void*);
using InitButtonsFn = void*(__fastcall*)(void*, void*);
using OnHiddenFn = void(__fastcall*)(void*, void*);

static GetCollectionFn s_getCollection = nullptr;
static GetInstanceFn s_getInstance = nullptr;
static InitButtonsFn s_originalInit = nullptr;
static OnHiddenFn s_originalHidden = nullptr;
static MascotTraceFeature* s_owner = nullptr;

static bool ReadPtr(const void* object, std::size_t offset, void** value) {
    if (!object || !value) return false;
    __try {
        *value = *reinterpret_cast<void* const*>(reinterpret_cast<const std::uint8_t*>(object) + offset);
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        *value = nullptr;
        return false;
    }
}

static bool ReadI32(const void* object, std::size_t offset, std::int32_t* value) {
    if (!object || !value) return false;
    __try {
        *value = *reinterpret_cast<const std::int32_t*>(reinterpret_cast<const std::uint8_t*>(object) + offset);
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        *value = 0;
        return false;
    }
}

static bool WritePtr(void* object, std::size_t offset, void* value) {
    if (!object) return false;
    __try {
        *reinterpret_cast<void**>(reinterpret_cast<std::uint8_t*>(object) + offset) = value;
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}

static bool WriteI32(void* object, std::size_t offset, std::int32_t value) {
    if (!object) return false;
    __try {
        *reinterpret_cast<std::int32_t*>(reinterpret_cast<std::uint8_t*>(object) + offset) = value;
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}

static bool WriteU8(void* object, std::size_t offset, std::uint8_t value) {
    if (!object) return false;
    __try {
        *reinterpret_cast<std::uint8_t*>(reinterpret_cast<std::uint8_t*>(object) + offset) = value;
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}

static bool ReadStringRaw(void* value, wchar_t* buffer, std::int32_t capacity, std::int32_t* lengthOut) {
    if (!value || !buffer || capacity <= 1 || !lengthOut) return false;
    __try {
        const auto* base = reinterpret_cast<const std::uint8_t*>(value);
        const std::int32_t length = *reinterpret_cast<const std::int32_t*>(base + 0x10);
        if (length <= 0 || length >= capacity) return false;
        const auto* chars = reinterpret_cast<const wchar_t*>(base + 0x14);
        for (std::int32_t i = 0; i < length; ++i) {
            buffer[i] = chars[i];
        }
        buffer[length] = L'\0';
        *lengthOut = length;
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        *lengthOut = 0;
        buffer[0] = L'\0';
        return false;
    }
}

static std::string ReadString(void* value) {
    wchar_t buffer[257] = {};
    std::int32_t length = 0;
    if (!ReadStringRaw(value, buffer, 257, &length)) return {};
    std::string result;
    result.reserve(static_cast<std::size_t>(length));
    for (std::int32_t i = 0; i < length; ++i)
        result.push_back(buffer[i] >= 0x20 && buffer[i] < 0x7f ? static_cast<char>(buffer[i]) : '?');
    return result;
}

static void* CallCollection(void* instance, void* getter) {
    if (!instance || !getter) return nullptr;
    __try { return reinterpret_cast<GetCollectionFn>(getter)(instance, nullptr); }
    __except (EXCEPTION_EXECUTE_HANDLER) { return nullptr; }
}

static void* CallInstance(void* getter) {
    if (!getter) return nullptr;
    __try { return reinterpret_cast<GetInstanceFn>(getter)(nullptr); }
    __except (EXCEPTION_EXECUTE_HANDLER) { return nullptr; }
}

static bool ListInfo(void* list, void** items, std::int32_t* size) {
    if (!list || !items || !size) return false;
    if (!ReadPtr(list, 0x10, items) || !ReadI32(list, 0x18, size)) return false;
    return *items != nullptr && *size >= 0 && *size <= 4096;
}

static std::vector<void*> MascotKeys(void* presenter) {
    std::vector<void*> keys;
    void* helper = nullptr;
    void* dictionary = nullptr;
    void* entries = nullptr;
    std::int32_t count = 0;
    if (!ReadPtr(presenter, 0x20, &helper) || !ReadPtr(helper, 0x28, &dictionary) ||
        !ReadPtr(dictionary, 0x18, &entries) || !ReadI32(dictionary, 0x20, &count) ||
        !entries || count < 0 || count > 4096) return keys;

    // Dictionary entries are a managed array. The extracted companion uses
    // the same +0x20 entry base and +0x18 key slot for this game build.
    for (std::int32_t i = 0; i < count + 16 && i < 8192; ++i) {
        auto* entry = reinterpret_cast<std::uint8_t*>(entries) + 0x20 + static_cast<std::size_t>(i) * 24;
        std::int32_t hash = 0;
        void* key = nullptr;
        if (!ReadI32(entry, 0, &hash) || hash < 0 || !ReadPtr(entry, 0x08, &key) || !key) continue;
        if (!ReadString(key).empty()) keys.push_back(key);
    }
    return keys;
}

static void __fastcall InitButtonsHook(void* self, void* mi) {
    if (s_owner) s_owner->OnMascotButtonsInit(self);
    if (s_originalInit) s_originalInit(self, mi);
}

static void __fastcall OnHiddenHook(void* self, void* mi) {
    if (s_owner) s_owner->OnMascotWindowHidden();
    if (s_originalHidden) s_originalHidden(self, mi);
}

template <typename T>
static bool Install(const char* owner, void* address, T hook, T* original) {
    if (!address || !GlobalHookRegistry().Claim(reinterpret_cast<std::uintptr_t>(address), owner)) return false;
    GlobalHookRegistry().MarkResolved(reinterpret_cast<std::uintptr_t>(address));
    if (MH_CreateHook(address, reinterpret_cast<LPVOID>(hook), reinterpret_cast<LPVOID*>(original)) != MH_OK ||
        MH_EnableHook(address) != MH_OK) {
        GlobalHookRegistry().MarkUnavailable(reinterpret_cast<std::uintptr_t>(address));
        return false;
    }
    GlobalHookRegistry().MarkHooked(reinterpret_cast<std::uintptr_t>(address));
    return true;
}
}

MascotTraceFeature::MascotTraceFeature() { name = "Mascot Trace"; enabled = false; s_owner = this; }

void MascotTraceFeature::Init() {
    GlobalConfigRegistry().RegisterBool("mascot.trace", &m_trace);
    GlobalConfigRegistry().RegisterBool("mascot.unlock", &m_unlock);
    GlobalConfigRegistry().RegisterBool("mascot.experimental", &m_experimental);
    m_getInstance = ResolveMethodOrFallback("Assembly-CSharp",
        "AutoChess.DataClasses.UserData", "ItemModule", "get_Instance", 0);
    m_getCollection = ResolveMethodOrFallback("Assembly-CSharp",
        "AutoChess.DataClasses.UserData", "ItemModule", "get_MascotCollection", 0);
    // The companion clears these two transient request guards after the
    // mascot endpoint returns 409. Resolve both methods/fields from metadata
    // so no stale offsets are baked into the feature.
    m_getPnkInstance = ResolveMethodOrFallback("Assembly-CSharp", "AutoChess",
        "PnkClient", "get_instance", 0);
    m_getSuhInstance = ResolveMethodOrFallback("Assembly-CSharp", "PnkClient",
        "ServerUpdateHandler", "get_Instance", 0);
    m_blockRequestsOffset = ResolveFieldOffset("Assembly-CSharp", "AutoChess",
        "PnkClient", "blockRequests");
    m_hasRequestSentOffset = ResolveFieldOffset("Assembly-CSharp", "PnkClient",
        "ServerUpdateHandler", "_hasRequestSent");
    m_initMascotButtons = ResolveMethodOrFallback("Assembly-CSharp",
        "AutoChess.MascotModule", "MascotWindowPresenter", "InitMascotButtons", 0);
    m_onHidden = ResolveMethodOrFallback("Assembly-CSharp",
        "AutoChess.MascotModule", "MascotWindow", "OnHidden", 0);
    s_getInstance = reinterpret_cast<GetInstanceFn>(m_getInstance);
    s_getCollection = reinterpret_cast<GetCollectionFn>(m_getCollection);
    const bool initHooked = Install("mascot.unlock.init", m_initMascotButtons,
        InitButtonsFn(&InitButtonsHook), &s_originalInit);
    const bool hiddenHooked = Install("mascot.unlock.restore", m_onHidden,
        OnHiddenFn(&OnHiddenHook), &s_originalHidden);
    LOG("[MASCOT] instance=%p collection=%p init=%p hidden=%p hooks=%d/%d",
        m_getInstance, m_getCollection, m_initMascotButtons, m_onHidden,
        initHooked ? 1 : 0, hiddenHooked ? 1 : 0);
    LOG("[MASCOT] request guards pnkGetter=%p suhGetter=%p offsets=0x%X/0x%X",
        m_getPnkInstance, m_getSuhInstance, m_blockRequestsOffset,
        m_hasRequestSentOffset);
}

void MascotTraceFeature::OnUpdate() {
    if (enabled && m_unlock) {
        if (!m_pnkInstance && m_getPnkInstance)
            m_pnkInstance = CallInstance(m_getPnkInstance);
        if (!m_suhInstance && m_getSuhInstance)
            m_suhInstance = CallInstance(m_getSuhInstance);
        // A failed mascot lookup can set these transient guards and block
        // later profile/quest requests. Clear only when the user explicitly
        // enabled the session unlock and only at metadata-resolved fields.
        if (m_pnkInstance && m_blockRequestsOffset >= 0)
            WritePtr(m_pnkInstance, static_cast<std::size_t>(m_blockRequestsOffset), nullptr);
        if (m_suhInstance && m_hasRequestSentOffset >= 0)
            WriteU8(m_suhInstance, static_cast<std::size_t>(m_hasRequestSentOffset), 0);
    }
    if (!enabled && m_injected) OnMascotWindowHidden();
}

void MascotTraceFeature::OnMascotButtonsInit(void* presenter) {
    m_lastPresenter = presenter;
    if (!enabled || !m_unlock || !s_getInstance || !s_getCollection) return;

    void* instance = CallInstance(m_getInstance);
    void* collection = CallCollection(instance, m_getCollection);
    if (!collection) { strncpy_s(m_status, "collection getter returned null", _TRUNCATE); return; }

    void* oldItems = nullptr;
    std::int32_t oldSize = 0;
    if (!ListInfo(collection, &oldItems, &oldSize)) {
        strncpy_s(m_status, "collection layout unresolved", _TRUNCATE);
        return;
    }
    m_lastCount = oldSize;
    if (m_injectedList == collection && m_injected) return;

    const std::vector<void*> keys = MascotKeys(presenter);
    if (keys.empty()) {
        strncpy_s(m_status, "mascot dictionary unavailable", _TRUNCATE);
        return;
    }

    std::unordered_set<std::string> owned;
    for (std::int32_t i = 0; i < oldSize; ++i) {
        void* item = nullptr;
        if (ReadPtr(reinterpret_cast<std::uint8_t*>(oldItems) + 0x20 + static_cast<std::size_t>(i) * 8, 0, &item))
            owned.insert(ReadString(item));
    }

    std::vector<void*> additions;
    std::unordered_set<std::string> seen = owned;
    for (void* key : keys) {
        const std::string id = ReadString(key);
        if (!id.empty() && seen.insert(id).second) additions.push_back(key);
    }
    if (additions.empty()) {
        strncpy_s(m_status, "all mascots already listed", _TRUNCATE);
        return;
    }

    void* stringClass = ResolveRuntimeClass(nullptr, "System", "String");
    void* array = AllocateIl2CppArray(stringClass, static_cast<std::size_t>(oldSize + additions.size()));
    if (!array) {
        strncpy_s(m_status, "managed string array allocation unavailable", _TRUNCATE);
        return;
    }
    bool written = true;
    for (std::int32_t i = 0; i < oldSize; ++i) {
        void* item = nullptr;
        if (!ReadPtr(reinterpret_cast<std::uint8_t*>(oldItems) + 0x20 + static_cast<std::size_t>(i) * 8, 0, &item) ||
            !WritePtr(reinterpret_cast<std::uint8_t*>(array) + 0x20 + static_cast<std::size_t>(i) * 8, 0, item)) {
            written = false; break;
        }
    }
    for (std::size_t i = 0; written && i < additions.size(); ++i)
        written = WritePtr(reinterpret_cast<std::uint8_t*>(array) + 0x20 + static_cast<std::size_t>(oldSize + i) * 8, 0, additions[i]);
    if (!written || !WritePtr(collection, 0x10, array) || !WriteI32(collection, 0x18, oldSize + static_cast<std::int32_t>(additions.size()))) {
        strncpy_s(m_status, "collection write rejected", _TRUNCATE);
        return;
    }
    m_injectedList = collection;
    m_originalItems = oldItems;
    m_originalSize = oldSize;
    m_injected = true;
    m_lastCount = oldSize + static_cast<std::int32_t>(additions.size());
    LOG("[MASCOT] session collection expanded %d -> %d", oldSize, m_lastCount);
    strncpy_s(m_status, "all mascots listed for this session", _TRUNCATE);
}

void MascotTraceFeature::OnMascotWindowHidden() {
    if (!m_injected || !m_injectedList) return;
    WritePtr(m_injectedList, 0x10, m_originalItems);
    WriteI32(m_injectedList, 0x18, m_originalSize);
    LOG("[MASCOT] collection restored size=%d", m_originalSize);
    m_injected = false;
    m_injectedList = nullptr;
    m_originalItems = nullptr;
    m_originalSize = 0;
}

void MascotTraceFeature::OnMenu() {
    if (!enabled) return;
    ImGui::Checkbox("Trace mascot collection", &m_trace);
    if (ImGui::Checkbox("Unlock all mascots (session)", &m_unlock)) ConfigMarkDirty();
    ImGui::Text("Getter: instance=%s collection=%s", m_getInstance ? "resolved" : "unresolved",
        m_getCollection ? "resolved" : "unresolved");
    ImGui::Text("Hooks: InitMascotButtons=%s OnHidden=%s", m_initMascotButtons ? "resolved" : "unresolved",
        m_onHidden ? "resolved" : "unresolved");
    ImGui::Text("Request guard clear: %s", (m_blockRequestsOffset >= 0 || m_hasRequestSentOffset >= 0)
        ? "metadata-resolved" : "not resolved");
    ImGui::TextWrapped("Enable the session unlock, close/reopen the mascot window, then choose the added mascot normally. The collection is restored when the window closes; no account inventory is written.");
    if (ImGui::Button("Apply to open mascot window") && m_lastPresenter) OnMascotButtonsInit(m_lastPresenter);
    ImGui::SameLine();
    if (ImGui::Button("Restore collection")) OnMascotWindowHidden();
    ImGui::Text("%s count=%d", m_status, m_lastCount);
}

static MascotTraceFeature g_mascotTrace;
static int g_mascotTraceRegistered = (RegisterFeature(&g_mascotTrace), 0);
