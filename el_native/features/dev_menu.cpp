#include "dev_menu.h"

#include "../framework.h"
#include "../main_thread_dispatcher.h"
#include "imgui.h"

namespace {
using StaticMethod = void(__fastcall*)(void*);
using GetInstanceMethod = void*(__fastcall*)(void*);
using ShowMethod = void(__fastcall*)(void*, void*, void*, void*);
}

DevMenuFeature::DevMenuFeature() { name = "DevMenu"; enabled = false; }

void DevMenuFeature::Init() {
    const auto cctor = ResolveMethodInfoOrFallback(
        "Assembly-CSharp", "AutoChess.Prefabs.CheatsWindow.Scripts",
        "CheatsWindow", ".cctor", 0);
    const auto load = ResolveMethodInfoOrFallback(
        "Assembly-CSharp", "AutoChess.Prefabs.CheatsWindow.Scripts",
        "CheatsWindow", "LoadWindow", 0);
    const auto instance = ResolveMethodInfoOrFallback(
        "Assembly-CSharp", "AutoChess.Prefabs.CheatsWindow.Scripts",
        "CheatsWindow", "get_instance", 0);
    const auto show = ResolveMethodInfoOrFallback(
        "Assembly-CSharp", "AutoChess.Prefabs.CheatsWindow.Scripts",
        "CheatsWindow", "Show", 2);
    m_staticCtor = cctor.pointer; m_staticCtorInfo = cctor.methodInfo;
    m_loadWindow = load.pointer; m_loadWindowInfo = load.methodInfo;
    m_getInstance = instance.pointer; m_getInstanceInfo = instance.methodInfo;
    m_show = show.pointer; m_showInfo = show.methodInfo;
    LOG("[DEV_MENU] cctor=%p load=%p instance=%p show=%p", m_staticCtor,
        m_loadWindow, m_getInstance, m_show);
}

bool DevMenuFeature::Request() {
    if (!m_getInstance || !m_show) {
        strncpy_s(m_status, "CheatsWindow methods unresolved", _TRUNCATE);
        m_state = State::Error;
        return false;
    }
    m_requested = true;
    m_state = m_staticCtor ? State::RunStaticCtor :
        m_loadWindow ? State::LoadWindow : State::GetInstance;
    strncpy_s(m_status, "queued on main thread", _TRUNCATE);
    return true;
}

void DevMenuFeature::Step() {
    switch (m_state) {
    case State::RunStaticCtor:
        reinterpret_cast<StaticMethod>(m_staticCtor)(m_staticCtorInfo);
        m_state = m_loadWindow ? State::LoadWindow : State::GetInstance;
        strncpy_s(m_status, "static constructor invoked", _TRUNCATE);
        break;
    case State::LoadWindow:
        reinterpret_cast<StaticMethod>(m_loadWindow)(m_loadWindowInfo);
        m_state = State::GetInstance;
        strncpy_s(m_status, "window load invoked", _TRUNCATE);
        break;
    case State::GetInstance:
        m_instance = reinterpret_cast<GetInstanceMethod>(m_getInstance)(m_getInstanceInfo);
        if (!m_instance) { m_state = State::Error; strncpy_s(m_status, "get_instance returned null", _TRUNCATE); }
        else { m_state = State::Show; strncpy_s(m_status, "instance acquired", _TRUNCATE); }
        break;
    case State::Show:
        reinterpret_cast<ShowMethod>(m_show)(m_instance, nullptr, nullptr, m_showInfo);
        m_state = State::Done;
        strncpy_s(m_status, "shown", _TRUNCATE);
        break;
    case State::Done:
    case State::Error:
    case State::Idle:
        m_requested = false;
        break;
    }
}

void DevMenuFeature::OnUpdate() {
    if (!enabled) { m_requested = false; m_state = State::Idle; return; }
    if (m_requested) Step();
}

void DevMenuFeature::OnMenu() {
    if (!enabled) return;
    ImGui::InputText("Dev menu hotkey", m_hotkey, sizeof(m_hotkey));
    if (ImGui::Button("Open CheatsWindow")) Request();
    ImGui::SameLine();
    ImGui::Text("%s", m_status);
}

static DevMenuFeature g_devMenu;
static int g_devMenuRegistered = (RegisterFeature(&g_devMenu), 0);
