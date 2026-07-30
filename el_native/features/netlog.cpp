#include "netlog.h"
#include "../framework.h"
#include "../il2cpp_resolve.h"
#include "../../minhook/include/MinHook.h"
#include "imgui.h"
#include <windows.h>
#include <cstdint>
#include <cstdio>

// PnkHandler.SendRequest is an instance method, so `this` takes RCX and the
// remaining args follow MSVC x64 positional slots. IL2CPP appends MethodInfo*.
typedef void (__fastcall* SendRequest6_t)(void* self, void* url, void* onCompleted,
                                         void* sendElem, float delay,
                                         int32_t serializerType,
                                         void* webRequestMethod, void* methodInfo);
typedef void (__fastcall* SendRequest4_t)(void* self, void* url, void* onCompleted,
                                          int32_t serializerType,
                                          void* webRequestMethod, void* methodInfo);

typedef void* (*il2cpp_object_get_class_t)(void* obj);
typedef const char* (*il2cpp_class_get_name_t)(void* klass);

static SendRequest6_t Original_SendRequest6 = nullptr;
static SendRequest4_t Original_SendRequest4 = nullptr;
static il2cpp_object_get_class_t Resolved_ObjectGetClass = nullptr;
static il2cpp_class_get_name_t Resolved_ClassGetName = nullptr;

static bool s_active = false;
static int s_count = 0;
static char s_lastUrl[192] = {0};

// System.String layout: 0x10 object header, int32 length, then UTF-16 chars.
static void CopyIl2CppString(void* str, char* out, size_t outSize) {
    out[0] = '\0';
    if (!str) return;
    int32_t len = *(int32_t*)((uint8_t*)str + 0x10);
    if (len <= 0) return;
    if ((size_t)len >= outSize) len = (int32_t)outSize - 1;
    const wchar_t* chars = (const wchar_t*)((uint8_t*)str + 0x14);
    for (int32_t i = 0; i < len; ++i) {
        wchar_t c = chars[i];
        out[i] = (c >= 0x20 && c < 0x7F) ? (char)c : '?';
    }
    out[len] = '\0';
}

static const char* PayloadTypeName(void* obj) {
    if (!obj || !Resolved_ObjectGetClass || !Resolved_ClassGetName) return "?";
    void* klass = Resolved_ObjectGetClass(obj);
    if (!klass) return "?";
    const char* n = Resolved_ClassGetName(klass);
    return n ? n : "?";
}

static void LogSend(void* url, void* payload, int32_t serializerType,
                    void* webMethod) {
    char urlBuf[192];
    char methodBuf[16];
    CopyIl2CppString(url, urlBuf, sizeof(urlBuf));
    CopyIl2CppString(webMethod, methodBuf, sizeof(methodBuf));
    LOG("[NET] %s %s payload=%s serializer=%d",
        methodBuf[0] ? methodBuf : "?", urlBuf, PayloadTypeName(payload),
        serializerType);
    ++s_count;
    snprintf(s_lastUrl, sizeof(s_lastUrl), "%s %s",
             methodBuf[0] ? methodBuf : "?", urlBuf);
}

static void __fastcall SendRequest6Hook(void* self, void* url, void* onCompleted,
                                        void* sendElem, float delay,
                                        int32_t serializerType,
                                        void* webRequestMethod, void* methodInfo) {
    if (s_active) {
        __try {
            LogSend(url, sendElem, serializerType, webRequestMethod);
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            LOG("[NET] log CRASHED (SEH) on 6-arg SendRequest");
        }
    }
    Original_SendRequest6(self, url, onCompleted, sendElem, delay,
                          serializerType, webRequestMethod, methodInfo);
}

static void __fastcall SendRequest4Hook(void* self, void* url, void* onCompleted,
                                        int32_t serializerType,
                                        void* webRequestMethod, void* methodInfo) {
    if (s_active) {
        __try {
            LogSend(url, nullptr, serializerType, webRequestMethod);
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            LOG("[NET] log CRASHED (SEH) on 4-arg SendRequest");
        }
    }
    Original_SendRequest4(self, url, onCompleted, serializerType,
                          webRequestMethod, methodInfo);
}

NetLogFeature::NetLogFeature() {
    name = "NetLog";
    enabled = false;
}

void NetLogFeature::Init() {
    HMODULE hGA = GetModuleHandleA("GameAssembly.dll");
    if (hGA) {
        Resolved_ObjectGetClass =
            (il2cpp_object_get_class_t)GetProcAddress(hGA, "il2cpp_object_get_class");
        Resolved_ClassGetName =
            (il2cpp_class_get_name_t)GetProcAddress(hGA, "il2cpp_class_get_name");
    }

    void* send6 = ResolveMethodOrFallback("Assembly-CSharp", "PnkClient.Core",
                                          "PnkHandler", "SendRequest", 6);
    void* send4 = ResolveMethodOrFallback("Assembly-CSharp", "PnkClient.Core",
                                          "PnkHandler", "SendRequest", 4);
    LOG("[FEATURE] NetLog: SendRequest6=%p SendRequest4=%p objGetClass=%p",
        send6, send4, Resolved_ObjectGetClass);

    if (send6 &&
        MH_CreateHook(send6, &SendRequest6Hook, (LPVOID*)&Original_SendRequest6) == MH_OK &&
        MH_EnableHook(send6) == MH_OK) {
        LOG("[FEATURE] NetLog: hooked SendRequest(6) @ %p", send6);
    } else {
        Original_SendRequest6 = nullptr;
        LOG("[FEATURE] NetLog: SendRequest(6) hook failed");
    }

    if (send4 &&
        MH_CreateHook(send4, &SendRequest4Hook, (LPVOID*)&Original_SendRequest4) == MH_OK &&
        MH_EnableHook(send4) == MH_OK) {
        LOG("[FEATURE] NetLog: hooked SendRequest(4) @ %p", send4);
    } else {
        Original_SendRequest4 = nullptr;
        LOG("[FEATURE] NetLog: SendRequest(4) hook failed");
    }
}

void NetLogFeature::OnUpdate() {
    s_active = enabled;
}

void NetLogFeature::OnMenu() {
    if (!enabled) return;
    ImGui::Text("captured: %d", s_count);
    ImGui::Text("last: %s", s_lastUrl[0] ? s_lastUrl : "(none)");
}

static NetLogFeature g_netLog;
static int g_netLogRegistered = (RegisterFeature(&g_netLog), 0);
