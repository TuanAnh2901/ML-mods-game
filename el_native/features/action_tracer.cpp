#include "action_tracer.h"
#include "../action_trace.h"
#include "../overlay_pos.h"
#include "../framework.h"
#include "../hook_registry.h"
#include "../il2cpp_resolve.h"
#include "../feature.h"
#include "../../minhook/include/MinHook.h"
#include "../../third_party/imgui/imgui.h"
#include <windows.h>
#include <cstdint>
#include <cstring>

namespace {
using ButtonFn = void(__fastcall*)(void*, void*);
using NameFn = void*(__fastcall*)(void*, void*);
static ButtonFn s_originalClick = nullptr;
static NameFn s_getName = nullptr;
static bool s_active = false;
static OverlayPos s_pos = { "actiontrace.window" };

static void ReadString(void* s, char* out, size_t cap) {
    if (!s || !out || !cap) return;
    out[0] = 0;
    __try {
        int32_t n = *(int32_t*)((uint8_t*)s + 0x10);
        if (n <= 0) return;
        if ((size_t)n >= cap) n = (int32_t)cap - 1;
        const wchar_t* chars = (const wchar_t*)((uint8_t*)s + 0x14);
        for (int32_t i = 0; i < n; i++) {
            wchar_t c = chars[i];
            out[i] = (c >= 0x20 && c < 0x7f) ? (char)c : '?';
        }
        out[n] = 0;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        out[0] = 0;
    }
}

// The resolver's imageHint filter requires an exact image-name match, and
// Unity image names differ between builds.  Try the known spellings; the
// metadata walk still fails for UnityEngine.CoreModule in this build (the
// resolver needs il2cpp_image_get_name which is absent), so fall back to the
// verified native RVA from method-pointer-map.json: UnityEngine.Object::get_name
// = 0x43CA3B0.  ReadClassName keeps clicks readable if even that is missing.
static void* ResolveGetName() {
    static void* cached = nullptr;
    static bool tried = false;
    if (tried) return cached;
    tried = true;
    const char* hints[] = { "UnityEngine.CoreModule", "UnityEngine.CoreModule.dll", "UnityEngine", "" };
    for (const char* hint : hints) {
        cached = ResolveMethodOrFallback(hint, "UnityEngine", "Object", "get_name", 0);
        if (cached) break;
    }
    if (!cached) {
        HMODULE ga = GetModuleHandleA("GameAssembly.dll");
        if (ga) cached = (void*)((uintptr_t)ga + 0x43CA3B0);
    }
    return cached;
}

static void ReadClassName(void* obj, char* out, size_t cap) {
    if (!obj || !out || !cap) return;
    out[0] = 0;
    __try {
        void* klass = *(void**)obj;
        if (!klass) return;
        const char* name = *(const char**)((uint8_t*)klass + 0x10);
        if (!name) return;
        strncpy_s(out, cap, name, _TRUNCATE);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        out[0] = 0;
    }
}

static void __fastcall ClickHook(void* self, void* mi) {
    if (s_originalClick) s_originalClick(self, mi);
    if (!s_active || !self) return;
    char name[96] = {};
    if (s_getName) {
        __try {
            void* str = s_getName(self, nullptr);
            ReadString(str, name, sizeof(name));
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            name[0] = 0;
        }
    }
    if (name[0]) {
        actiontrace::Push("click", "%s [%p]", name, self);
        return;
    }
    char className[96] = {};
    ReadClassName(self, className, sizeof(className));
    actiontrace::Push("click", "%s%s [%p] (name unresolved)",
        className[0] ? "<" : "", className[0] ? className : "?",
        self);
}
}

ActionTracerFeature::ActionTracerFeature() { name = "ActionTracer"; enabled = false; }
void ActionTracerFeature::Init() {
    s_pos.Register();
    void* click = ResolveMethodOrFallback("Assembly-CSharp", "UGUIVisual",
        "UGUIButtonListener", "HandleClick", 0);
    void* getName = ResolveGetName();
    s_getName = reinterpret_cast<NameFn>(getName);
    bool hooked = false;
    if (click && GlobalHookRegistry().Claim((uintptr_t)click, "actiontracer.click")) {
        GlobalHookRegistry().MarkResolved((uintptr_t)click);
        if (MH_CreateHook(click, (LPVOID)&ClickHook, (LPVOID*)&s_originalClick) == MH_OK &&
            MH_EnableHook(click) == MH_OK) {
            GlobalHookRegistry().MarkHooked((uintptr_t)click);
            hooked = true;
        } else {
            GlobalHookRegistry().MarkUnavailable((uintptr_t)click);
        }
    }
    LOG("[ACTIONTRACER] click=%p getName=%p hooked=%d", click, getName, hooked ? 1 : 0);
    actiontrace::Push("tracer", "ActionTracer init click=%p hooked=%d", click, hooked ? 1 : 0);
}
void ActionTracerFeature::OnUpdate() { s_active = enabled; }
void ActionTracerFeature::OnMenu() {
    ImGui::TextWrapped("Logs every UGUI button click (with GameObject name) and Automation state transitions into "
                       "el_native_action_trace.jsonl + the on-screen ring. Click name resolution needs Object.get_name.");
    ImGui::Separator();
    ActionTracerRender();
}
void ActionTracerRender() {
    static actiontrace::Event ev[actiontrace::kRing];
    int n = 0;
    actiontrace::Snapshot(ev, &n);
    ImGui::BeginChild("action_trace_view", ImVec2(0, 0), true);
    for (int i = 0; i < n; i++) {
        const auto& e = ev[i];
        if (strcmp(e.source, "click") == 0) {
            ImGui::TextColored(ImVec4(0.4f, 0.8f, 1, 1), "[%s] %s", e.source, e.text);
        } else if (strcmp(e.source, "automation") == 0) {
            ImGui::TextColored(ImVec4(0.3f, 1, 0.4f, 1), "[%s] %s", e.source, e.text);
        } else if (strcmp(e.source, "err") == 0) {
            ImGui::TextColored(ImVec4(1, 0.2f, 0.2f, 1), "[%s] %s", e.source, e.text);
        } else {
            ImGui::Text("[%s] %s", e.source, e.text);
        }
    }
    if (n == 0) ImGui::Text("(no events yet; enable and interact with the game)");
    ImGui::EndChild();
}
void ActionTracerFeature::OnOverlay() {
    s_pos.Apply();
    ImGui::Begin("Action Trace", nullptr, ImGuiWindowFlags_NoSavedSettings);
    ImGui::Text("Automation + UI clicks (last %d)", actiontrace::Count());
    ImGui::Separator();
    ActionTracerRender();
    ImGui::End();
    s_pos.Save();
}

static ActionTracerFeature g_feature;
static int g_registered = (RegisterFeature(&g_feature), 0);

