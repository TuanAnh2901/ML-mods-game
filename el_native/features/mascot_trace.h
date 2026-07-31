#pragma once

#include "../feature.h"

struct MascotTraceFeature : Feature {
    MascotTraceFeature();
    void Init() override;
    void OnUpdate() override;
    void OnMenu() override;
    void OnMascotButtonsInit(void* presenter);
    void OnMascotWindowHidden();

private:
    void* m_getInstance = nullptr;
    void* m_getCollection = nullptr;
    void* m_getPnkInstance = nullptr;
    void* m_getSuhInstance = nullptr;
    void* m_initMascotButtons = nullptr;
    void* m_onHidden = nullptr;
    void* m_lastPresenter = nullptr;
    void* m_injectedList = nullptr;
    void* m_originalItems = nullptr;
    int32_t m_originalSize = 0;
    int32_t m_blockRequestsOffset = -1;
    int32_t m_hasRequestSentOffset = -1;
    bool m_trace = true;
    bool m_unlock = false;
    bool m_injected = false;
    bool m_experimental = false;
    int m_lastCount = -1;
    void* m_pnkInstance = nullptr;
    void* m_suhInstance = nullptr;
    char m_status[96] = "not captured";
};
