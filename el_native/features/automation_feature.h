#pragma once

#include "../automation.h"
#include "../feature.h"

struct AutomationFeature : Feature {
    AutomationFeature();
    void Init() override;
    void OnUpdate() override;
    void OnMenu() override;
    void OnResultWindowShown();
    void OnPlayButtonUpdate(void* self);
    void OnResultWindowUpdate(void* self);
    void OnResultPhaseContinued();
    void OnBattlefieldStart(void* self);
    void OnBattleSettingsUpdate(void* self);
    void OnConfirmShown(void* self);
    void OnButtonUpdate(void* self);
    void OnLeagueBarShown(void* self);
    void OnLeagueBarUnlock();
    void OnLeagueBarTick();
    void OnMultichestShown(void* self);
    void OnMultichestHidden();
    void OnMultichestTick();
    void OnBundleShown(void* self);

private:
    AutomationCoordinator m_coordinator;
    int m_mode = 0;
    int m_delayMs = 1500;
    int m_maxLoops = 1;
    bool m_observerResolved = false;
    bool m_showHooked = false;
    bool m_updateHooked = false;
    bool m_playHooked = false;
    bool m_advanceHooked = false;
    bool m_derankReady = false;
    bool m_derankHooked = false;
    bool m_leagueHooked = false;
    bool m_leagueReady = false;
    bool m_waitingLeagueBar = false;
    bool m_multichestReady = false;
    bool m_waitingMultichest = false;
    bool m_bundleReady = false;
    unsigned long long m_nextActionAt = 0;
    void* m_resultWindow = nullptr;
    void* m_settingsButton = nullptr;
    void* m_surrenderButton = nullptr;
    void* m_confirmButton = nullptr;
    void* m_leaguePresenter = nullptr;
    void* m_multichestWindow = nullptr;
    void* m_bundleWindow = nullptr;
    int32_t m_settingsOffset = -1;
    int32_t m_surrenderOffset = -1;
    int32_t m_leftButtonOffset = -1;
    int32_t m_onButtonActionOffset = -1;
    unsigned long long m_leagueCloseAt = 0;
    unsigned long long m_multichestCloseAt = 0;
    unsigned long long m_bundleCloseAt = 0;
    enum class DerankPhase { Idle, Settings, CaptureSurrender, Surrender, AwaitConfirm, Confirm };
    DerankPhase m_derankPhase = DerankPhase::Idle;
    char m_status[96] = "idle";
};
