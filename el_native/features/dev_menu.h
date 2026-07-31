#pragma once

#include "../feature.h"
#include "../il2cpp_resolve.h"

struct DevMenuFeature : Feature {
    DevMenuFeature();
    void Init() override;
    void OnUpdate() override;
    void OnMenu() override;

private:
    enum class State { Idle, RunStaticCtor, LoadWindow, GetInstance, Show, Done, Error };
    void Step();
    bool Request();
    State m_state = State::Idle;
    bool m_requested = false;
    void* m_staticCtor = nullptr;
    void* m_loadWindow = nullptr;
    void* m_getInstance = nullptr;
    void* m_show = nullptr;
    void* m_staticCtorInfo = nullptr;
    void* m_loadWindowInfo = nullptr;
    void* m_getInstanceInfo = nullptr;
    void* m_showInfo = nullptr;
    void* m_instance = nullptr;
    char m_hotkey[16] = "F6";
    char m_status[96] = "not requested";
};
