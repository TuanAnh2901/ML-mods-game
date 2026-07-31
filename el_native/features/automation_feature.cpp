#include "automation_feature.h"

#include "../config_registry.h"
#include "../framework.h"
#include "../hook_registry.h"
#include "../il2cpp_resolve.h"
#include "../../minhook/include/MinHook.h"
#include "imgui.h"
#include <cstdint>

namespace {
using ShowWindowFn = void(__fastcall*)(void*, void*, void*, void*, void*, void*);
using UpdateFn = void(__fastcall*)(void*, void*);
using PlayClickFn = void(__fastcall*)(void*, void*);
using ResultActionFn = void(__fastcall*)(void*, void*);
using ButtonFn = void(__fastcall*)(void*, void*);
using FocusFn = void(__fastcall*)(void*, void*, void*);
using DelegateInvokeFn = void(__fastcall*)(void*, void*);
static ShowWindowFn s_originalShow = nullptr;
static UpdateFn s_originalUpdate = nullptr;
static UpdateFn s_originalPlayUpdate = nullptr;
static PlayClickFn s_originalPlayClick = nullptr;
static ResultActionFn s_originalPlayNext = nullptr;
static ResultActionFn s_originalHide = nullptr;
static ResultActionFn s_originalContinue = nullptr;
static ButtonFn s_originalBattlefieldStart = nullptr;
static ButtonFn s_originalBattleSettingsUpdate = nullptr;
static ButtonFn s_originalConfirmShown = nullptr;
static ButtonFn s_originalButtonUpdate = nullptr;
static ButtonFn s_originalHandleClick = nullptr;
static ButtonFn s_originalLeagueShown = nullptr;
static ButtonFn s_originalLeagueClose = nullptr;
static ButtonFn s_originalLeagueUnlock = nullptr;
static UpdateFn s_originalLeagueScrollUpdate = nullptr;
static UpdateFn s_originalLeagueCounterUpdate = nullptr;
static ButtonFn s_originalMultichestShow = nullptr;
static ButtonFn s_originalMultichestOnEnable = nullptr;
static ButtonFn s_originalMultichestStart = nullptr;
static ButtonFn s_originalMultichestOpenAll = nullptr;
static ButtonFn s_originalMultichestCloseAction = nullptr;
static ButtonFn s_originalMultichestHidden = nullptr;
static FocusFn s_originalBundleFocus = nullptr;
static ButtonFn s_originalBundleClose = nullptr;
static AutomationFeature* s_owner = nullptr;

static void __fastcall ShowWindowHook(void* self, void* state, void* context,
                                      void* onHidden, void* onShown, void* mi) {
    if (s_originalShow) s_originalShow(self, state, context, onHidden, onShown, mi);
    if (s_owner) s_owner->OnResultWindowShown();
}

static void __fastcall UpdateHook(void* self, void* mi) {
    if (s_originalUpdate) s_originalUpdate(self, mi);
    if (s_owner) s_owner->OnResultWindowUpdate(self);
}

static void __fastcall PlayButtonUpdateHook(void* self, void* mi) {
    if (s_originalPlayUpdate) s_originalPlayUpdate(self, mi);
    if (s_owner) s_owner->OnPlayButtonUpdate(self);
}

static void __fastcall ContinueShowHook(void* self, void* mi) {
    if (s_originalContinue) s_originalContinue(self, mi);
    if (s_owner) s_owner->OnResultPhaseContinued();
}

static void __fastcall BattlefieldStartHook(void* self, void* mi) {
    if (s_originalBattlefieldStart) s_originalBattlefieldStart(self, mi);
    if (s_owner) s_owner->OnBattlefieldStart(self);
}

static void __fastcall BattleSettingsUpdateHook(void* self, void* mi) {
    if (s_originalBattleSettingsUpdate) s_originalBattleSettingsUpdate(self, mi);
    if (s_owner) s_owner->OnBattleSettingsUpdate(self);
}

static void __fastcall ConfirmShownHook(void* self, void* mi) {
    if (s_originalConfirmShown) s_originalConfirmShown(self, mi);
    if (s_owner) s_owner->OnConfirmShown(self);
}

static void __fastcall ButtonUpdateHook(void* self, void* mi) {
    if (s_originalButtonUpdate) s_originalButtonUpdate(self, mi);
    if (s_owner) s_owner->OnButtonUpdate(self);
}

static void __fastcall LeagueShownHook(void* self, void* mi) {
    if (s_originalLeagueShown) s_originalLeagueShown(self, mi);
    if (s_owner) s_owner->OnLeagueBarShown(self);
}

static void __fastcall LeagueCloseHook(void* self, void* mi) {
    if (s_originalLeagueClose) s_originalLeagueClose(self, mi);
}

static void __fastcall LeagueUnlockHook(void* self, void* mi) {
    if (s_originalLeagueUnlock) s_originalLeagueUnlock(self, mi);
    if (s_owner) s_owner->OnLeagueBarUnlock();
}

static void __fastcall LeagueScrollUpdateHook(void* self, void* mi) {
    if (s_originalLeagueScrollUpdate) s_originalLeagueScrollUpdate(self, mi);
    if (s_owner) s_owner->OnLeagueBarTick();
}

static void __fastcall LeagueCounterUpdateHook(void* self, void* mi) {
    if (s_originalLeagueCounterUpdate) s_originalLeagueCounterUpdate(self, mi);
    if (s_owner) {
        s_owner->OnLeagueBarTick();
        s_owner->OnMultichestTick();
    }
}

static void __fastcall MultichestShowHook(void* self, void* mi) {
    if (s_originalMultichestShow) s_originalMultichestShow(self, mi);
    if (s_owner) s_owner->OnMultichestShown(self);
}

static void __fastcall MultichestEnableHook(void* self, void* mi) {
    if (s_originalMultichestOnEnable) s_originalMultichestOnEnable(self, mi);
    if (s_owner) s_owner->OnMultichestShown(self);
}

static void __fastcall MultichestStartHook(void* self, void* mi) {
    if (s_originalMultichestStart) s_originalMultichestStart(self, mi);
    if (s_owner) s_owner->OnMultichestShown(self);
}

static void __fastcall MultichestHiddenHook(void* self, void* mi) {
    if (s_originalMultichestHidden) s_originalMultichestHidden(self, mi);
    if (s_owner) s_owner->OnMultichestHidden();
}

static void __fastcall BundleFocusHook(void* self, void* state, void* mi) {
    if (s_originalBundleFocus) s_originalBundleFocus(self, state, mi);
    if (s_owner) s_owner->OnBundleShown(self);
}

static bool ReadObjectPointer(void* object, int32_t offset, void** value) {
    if (!object || !value || offset < 0) return false;
    __try {
        *value = *reinterpret_cast<void**>(reinterpret_cast<std::uint8_t*>(object) + offset);
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        *value = nullptr;
        return false;
    }
}

static bool InvokeButtonAction(void* window, int32_t offset) {
    if (!window || offset < 0) return false;
    void* delegate = nullptr;
    if (!ReadObjectPointer(window, offset, &delegate) || !delegate) return false;
    void* method = nullptr;
    void* target = nullptr;
    void* methodInfo = nullptr;
    if (!ReadObjectPointer(delegate, 0x10, &method) ||
        !ReadObjectPointer(delegate, 0x20, &target) ||
        !ReadObjectPointer(delegate, 0x28, &methodInfo) || !method) return false;
    __try {
        reinterpret_cast<DelegateInvokeFn>(method)(target, methodInfo);
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}

template <typename T>
bool InstallObserver(const char* owner, void* address, T hook, T* original) {
    if (!address || !GlobalHookRegistry().Claim(reinterpret_cast<uintptr_t>(address), owner)) return false;
    GlobalHookRegistry().MarkResolved(reinterpret_cast<uintptr_t>(address));
    if (MH_CreateHook(address, reinterpret_cast<LPVOID>(hook), reinterpret_cast<LPVOID*>(original)) != MH_OK ||
        MH_EnableHook(address) != MH_OK) {
        GlobalHookRegistry().MarkUnavailable(reinterpret_cast<uintptr_t>(address));
        return false;
    }
    GlobalHookRegistry().MarkHooked(reinterpret_cast<uintptr_t>(address));
    return true;
}
}

AutomationFeature::AutomationFeature() { name = "Automation"; enabled = false; s_owner = this; }

void AutomationFeature::Init() {
    GlobalConfigRegistry().RegisterInteger("automation.mode", &m_mode);
    GlobalConfigRegistry().RegisterInteger("automation.delay_ms", &m_delayMs);
    GlobalConfigRegistry().RegisterInteger("automation.max_loops", &m_maxLoops);
    // Resolve only the stable result-window observer.  The old wide diagnostic
    // RVA list is intentionally not ported; unresolved observers stay trace-only.
    const char* ns = "AutoChess.UIScripts.WindowScripts.MatchCompleted";
    void* show = ResolveMethodOrFallback("Assembly-CSharp", ns, "MatchCompletedWindow", "ShowWindow", 4);
    void* update = ResolveMethodOrFallback("Assembly-CSharp", ns, "MatchCompletedWindow", "Update", 0);
    m_observerResolved = show != nullptr && update != nullptr;
    m_showHooked = InstallObserver("automation.result.show", show, &ShowWindowHook, &s_originalShow);
    m_updateHooked = InstallObserver("automation.result.update", update, &UpdateHook, &s_originalUpdate);
    void* playUpdate = ResolveMethodOrFallback("Assembly-CSharp",
        "AutoChess.UIScripts.WindowScripts", "MainWindowPlayButton", "Update", 0);
    void* playClick = ResolveMethodOrFallback("Assembly-CSharp",
        "AutoChess.UIScripts.WindowScripts", "MainWindowPlayButton", "OnPlayClick", 0);
    void* playNext = ResolveMethodOrFallback("Assembly-CSharp", ns,
        "MatchCompletedWindow", "HandlePlayNextButton", 0);
    void* hide = ResolveMethodOrFallback("Assembly-CSharp", ns,
        "MatchCompletedWindow", "HideWindow", 0);
    void* continueShow = ResolveMethodOrFallback("Assembly-CSharp", ns,
        "MatchCompletedWindow", "ContinueShow", 0);
    m_playHooked = InstallObserver("automation.play.update", playUpdate,
        &PlayButtonUpdateHook, &s_originalPlayUpdate);
    // These methods are invoked by the Update dispatcher and must not be
    // detoured themselves. Keep their resolved pointers as call targets.
    s_originalPlayClick = reinterpret_cast<PlayClickFn>(playClick);
    s_originalPlayNext = reinterpret_cast<ResultActionFn>(playNext);
    s_originalHide = reinterpret_cast<ResultActionFn>(hide);
    const bool continueHooked = InstallObserver("automation.result.continue", continueShow,
        ResultActionFn(&ContinueShowHook), &s_originalContinue);
    m_advanceHooked = playNext != nullptr || hide != nullptr;
    void* battlefieldStart = ResolveMethodOrFallback("Assembly-CSharp",
        "AutoChess.UIScripts.WindowScripts.BattlefieldWindow", "BattlefieldWindow", "Start", 0);
    void* battleSettingsUpdate = ResolveMethodOrFallback("Assembly-CSharp",
        "AutoChess.UIScripts.WindowScripts", "BattleSettingsWindow", "Update", 0);
    void* confirmShown = ResolveMethodOrFallback("Assembly-CSharp",
        "AutoChess.UIScripts.WindowScripts.TechnicalWindows", "TwoButtonWindow", "OnShown", 0);
    void* buttonUpdate = ResolveMethodOrFallback("Assembly-CSharp",
        "UGUIVisual", "UGUIButtonListener", "Update", 0);
    void* handleClick = ResolveMethodOrFallback("Assembly-CSharp",
        "UGUIVisual", "UGUIButtonListener", "HandleClick", 0);
    m_settingsOffset = ResolveFieldOffset("Assembly-CSharp",
        "AutoChess.UIScripts.WindowScripts.BattlefieldWindow", "BattlefieldWindow", "settingsButton");
    m_surrenderOffset = ResolveFieldOffset("Assembly-CSharp",
        "AutoChess.UIScripts.WindowScripts", "BattleSettingsWindow", "surrenderButton");
    m_leftButtonOffset = ResolveFieldOffset("Assembly-CSharp",
        "AutoChess.UIScripts.WindowScripts.TechnicalWindows", "TwoButtonWindow", "leftButton");
    m_onButtonActionOffset = ResolveFieldOffset("Assembly-CSharp",
        "AutoChess.UIScripts.WindowScripts.MatchCompleted", "MatchCompletedWindow", "_onButtonAction");
    m_advanceHooked = m_advanceHooked || m_onButtonActionOffset >= 0;
    const bool h1 = InstallObserver("automation.derank.battlefield", battlefieldStart,
        ButtonFn(&BattlefieldStartHook), &s_originalBattlefieldStart);
    const bool h2 = InstallObserver("automation.derank.settings", battleSettingsUpdate,
        ButtonFn(&BattleSettingsUpdateHook), &s_originalBattleSettingsUpdate);
    const bool h3 = InstallObserver("automation.derank.confirm", confirmShown,
        ButtonFn(&ConfirmShownHook), &s_originalConfirmShown);
    const bool h4 = InstallObserver("automation.derank.buttons", buttonUpdate,
        ButtonFn(&ButtonUpdateHook), &s_originalButtonUpdate);
    s_originalHandleClick = reinterpret_cast<ButtonFn>(handleClick);
    m_derankHooked = h1 && h2 && h3 && h4;
    m_derankReady = m_derankHooked && s_originalHandleClick &&
        m_settingsOffset >= 0 && m_surrenderOffset >= 0 && m_leftButtonOffset >= 0;

    void* leagueShown = ResolveMethodOrFallback("Assembly-CSharp",
        "AutoChess.LeagueFlow.Views", "LeagueBarWindowPresenter", "OnShown", 0);
    void* leagueClose = ResolveMethodOrFallback("Assembly-CSharp",
        "AutoChess.LeagueFlow.Views", "LeagueBarWindowPresenter", "OnClose", 0);
    void* leagueUnlock = ResolveMethodOrFallback("Assembly-CSharp",
        "AutoChess.LeagueFlow.Views", "LeagueBarWindow", "UnlockButtons", 0);
    void* leagueScroll = ResolveMethodOrFallback("Assembly-CSharp",
        "AutoChess.LeagueFlow.Views.BarViews", "LeagueBarScroll", "Update", 0);
    void* leagueCounter = ResolveMethodOrFallback("Assembly-CSharp",
        "AutoChess.UIScripts.WindowScripts.MultichestWindow", "CounterElement", "Update", 0);
    const bool lh1 = InstallObserver("automation.league.shown", leagueShown,
        ButtonFn(&LeagueShownHook), &s_originalLeagueShown);
    const bool lh2 = InstallObserver("automation.league.unlock", leagueUnlock,
        ButtonFn(&LeagueUnlockHook), &s_originalLeagueUnlock);
    const bool lh3 = InstallObserver("automation.league.scroll", leagueScroll,
        &LeagueScrollUpdateHook, &s_originalLeagueScrollUpdate);
    const bool lh4 = InstallObserver("automation.league.counter", leagueCounter,
        &LeagueCounterUpdateHook, &s_originalLeagueCounterUpdate);
    // OnClose is retained as a call target, not detoured: the tick hook invokes
    // it only after the presenter has finished unlocking its buttons.
    s_originalLeagueClose = reinterpret_cast<ButtonFn>(leagueClose);
    m_leagueHooked = lh1 && lh2 && s_originalLeagueClose != nullptr;
    m_leagueReady = m_leagueHooked && (lh3 || lh4);

    // Reward popup parity with auto_battle.js.  ShowMultichestWindow is the
    // primary entry point; OnEnable/Start are fallbacks for builds that route
    // the popup through a pooled WindowScript instance.
    void* multichestShow = ResolveMethodOrFallback("Assembly-CSharp",
        "UI_Scripts.WindowManager", "MultichestWindow", "ShowMultichestWindow", 0);
    void* multichestEnable = ResolveMethodOrFallback("Assembly-CSharp",
        "UI_Scripts.WindowManager", "MultichestWindow", "OnEnable", 0);
    void* multichestStart = ResolveMethodOrFallback("Assembly-CSharp",
        "UI_Scripts.WindowManager", "MultichestWindow", "Start", 0);
    void* multichestOpenAll = ResolveMethodOrFallback("Assembly-CSharp",
        "UI_Scripts.WindowManager", "MultichestWindow", "OnOpenAll", 0);
    void* multichestClose = ResolveMethodOrFallback("Assembly-CSharp",
        "UI_Scripts.WindowManager", "MultichestWindow", "OnCloseAction", 0);
    void* multichestHidden = ResolveMethodOrFallback("Assembly-CSharp",
        "UI_Scripts.WindowManager", "MultichestWindow", "WindowHidden", 0);
    const bool mh1 = InstallObserver("automation.multichest.show", multichestShow,
        ButtonFn(&MultichestShowHook), &s_originalMultichestShow);
    const bool mh2 = InstallObserver("automation.multichest.enable", multichestEnable,
        ButtonFn(&MultichestEnableHook), &s_originalMultichestOnEnable);
    const bool mh3 = InstallObserver("automation.multichest.start", multichestStart,
        ButtonFn(&MultichestStartHook), &s_originalMultichestStart);
    const bool mh4 = InstallObserver("automation.multichest.hidden", multichestHidden,
        ButtonFn(&MultichestHiddenHook), &s_originalMultichestHidden);
    s_originalMultichestOpenAll = reinterpret_cast<ButtonFn>(multichestOpenAll);
    s_originalMultichestCloseAction = reinterpret_cast<ButtonFn>(multichestClose);
    m_multichestReady = (mh1 || mh2 || mh3) && s_originalMultichestOpenAll &&
        s_originalMultichestCloseAction && mh4;

    void* bundleFocus = ResolveMethodOrFallback("Assembly-CSharp",
        "AutoChess.UIScripts.WindowScripts.Bundles", "BundleForceShowWindow", "OnFocus", 1);
    void* bundleClose = ResolveMethodOrFallback("Assembly-CSharp",
        "AutoChess.UIScripts.WindowScripts.Bundles", "BundleForceShowWindow", "OnClose", 0);
    const bool bh1 = InstallObserver("automation.bundle.focus", bundleFocus,
        &BundleFocusHook, &s_originalBundleFocus);
    s_originalBundleClose = reinterpret_cast<ButtonFn>(bundleClose);
    m_bundleReady = bh1 && s_originalBundleClose != nullptr;
    LOG("[AUTOMATION] result observers show=%p update=%p continue=%p playUpdate=%p playClick=%p next=%p hide=%p hooked=%d/%d/%d play=%d advance=%d",
        show, update, continueShow, playUpdate, playClick, playNext, hide,
        m_showHooked ? 1 : 0, m_updateHooked ? 1 : 0,
        continueHooked ? 1 : 0, m_playHooked ? 1 : 0, m_advanceHooked ? 1 : 0);
    LOG("[AUTOMATION] derank methods=%d/%d/%d/%d offsets=%d/%d/%d resultDelegate=%d ready=%d",
        h1 ? 1 : 0, h2 ? 1 : 0, h3 ? 1 : 0, h4 ? 1 : 0,
        m_settingsOffset, m_surrenderOffset, m_leftButtonOffset,
        m_onButtonActionOffset, m_derankReady ? 1 : 0);
    LOG("[AUTOMATION] league methods shown=%p close=%p unlock=%p scroll=%p counter=%p hooked=%d/%d/%d/%d ready=%d",
        leagueShown, leagueClose, leagueUnlock, leagueScroll, leagueCounter,
        lh1 ? 1 : 0, lh2 ? 1 : 0, lh3 ? 1 : 0, lh4 ? 1 : 0,
        m_leagueReady ? 1 : 0);
    LOG("[AUTOMATION] multichest methods show=%p enable=%p start=%p openAll=%p close=%p hidden=%p hooked=%d/%d/%d/%d ready=%d",
        multichestShow, multichestEnable, multichestStart, multichestOpenAll,
        multichestClose, multichestHidden, mh1 ? 1 : 0, mh2 ? 1 : 0,
        mh3 ? 1 : 0, mh4 ? 1 : 0, m_multichestReady ? 1 : 0);
    LOG("[AUTOMATION] bundle methods focus=%p close=%p hooked=%d ready=%d",
        bundleFocus, bundleClose, bh1 ? 1 : 0, m_bundleReady ? 1 : 0);
}

void AutomationFeature::OnUpdate() {
    if (!enabled) return;
    const AutomationState before = m_coordinator.State();
    const AutomationMode mode = m_mode == 2 ? AutomationMode::Derank :
        m_mode == 1 ? AutomationMode::AutoBattle : AutomationMode::Off;
    m_coordinator.Configure(mode, m_maxLoops);
    if (m_coordinator.State() == AutomationState::Cooldown &&
        !m_waitingLeagueBar && !m_waitingMultichest && !m_bundleWindow)
        m_coordinator.OnEvent(AutomationEvent::CooldownElapsed);
    OnMultichestTick();
    if (m_bundleWindow && m_bundleCloseAt != 0 && GetTickCount64() >= m_bundleCloseAt) {
        void* bundle = m_bundleWindow;
        m_bundleWindow = nullptr;
        m_bundleCloseAt = 0;
        if (s_originalBundleClose) s_originalBundleClose(bundle, nullptr);
        strncpy_s(m_status, "bundle popup dismissed", _TRUNCATE);
    }
    if (before == AutomationState::Cooldown && m_coordinator.State() == AutomationState::StartingBattle)
        m_nextActionAt = GetTickCount64() + static_cast<unsigned long long>(m_delayMs);
}

void AutomationFeature::OnMenu() {
    if (!enabled) return;
    const char* modes[] = {"Off", "AutoBattle", "Derank"};
    ImGui::Combo("Automation mode", &m_mode, modes, 3);
    ImGui::SliderInt("Delay (ms)", &m_delayMs, 250, 30000);
    ImGui::SliderInt("Max loops (0 = unlimited)", &m_maxLoops, 0, 100);
    ImGui::Text("Derank readiness: %s", m_derankReady ? "ready" : "field layout unresolved");
    ImGui::Text("LeagueBar: %s", m_leagueReady ? "hooked; auto-close" : "trace-only");
    ImGui::Text("Rewards: multichest=%s bundle=%s", m_multichestReady ? "auto-open/close" : "trace-only",
        m_bundleReady ? "auto-dismiss" : "trace-only");
    ImGui::Text("Fields: settings=%s surrender=%s confirm=%s result delegate=%s",
        m_settingsOffset >= 0 ? "ok" : "missing", m_surrenderOffset >= 0 ? "ok" : "missing",
        m_leftButtonOffset >= 0 ? "ok" : "missing", m_onButtonActionOffset >= 0 ? "ok" : "fallback");
    if (ImGui::Button("Start automation")) {
        const bool modeReady = m_mode != 2 || m_derankReady;
        if (modeReady && m_observerResolved && m_playHooked && m_advanceHooked && m_coordinator.Start()) {
            m_nextActionAt = GetTickCount64() + static_cast<unsigned long long>(m_delayMs);
            strncpy_s(m_status, "started; play is scheduled", _TRUNCATE);
        }
        else strncpy_s(m_status, m_mode == 2 ? "Derank field layout unresolved" : "observer unresolved or mode off", _TRUNCATE);
    }
    ImGui::SameLine();
    if (ImGui::Button("Stop")) { m_coordinator.Stop(); strncpy_s(m_status, "stopped", _TRUNCATE); }
    if (ImGui::Button("Mark reward collected")) {
        m_coordinator.OnEvent(AutomationEvent::RewardCollected);
        strncpy_s(m_status, "reward event accepted", _TRUNCATE);
    }
    ImGui::Text("State=%d loops=%d/%d observer=%s result=%d/%d play=%d advance=%d", static_cast<int>(m_coordinator.State()),
        m_coordinator.CompletedLoops(), m_maxLoops, m_observerResolved ? "ready" : "trace-only",
        m_showHooked ? 1 : 0, m_updateHooked ? 1 : 0,
        m_playHooked ? 1 : 0, m_advanceHooked ? 1 : 0);
    ImGui::TextWrapped("AutoBattle schedules Play, advances the result window, opens/closes Multichest rewards, dismisses bundle popups, and closes LeagueBar. Derank uses the same dispatcher. Max loops 0 keeps running until Stop.");
    ImGui::Text("%s", m_status);
}

void AutomationFeature::OnResultWindowShown() {
    if (!enabled || !m_observerResolved) return;
    if (m_coordinator.State() == AutomationState::StartingBattle)
        m_coordinator.OnEvent(AutomationEvent::BattleStarted);
    m_coordinator.OnEvent(AutomationEvent::BattleFinished);
    m_resultWindow = nullptr;
    m_derankPhase = DerankPhase::Idle;
    m_settingsButton = nullptr;
    m_surrenderButton = nullptr;
    m_confirmButton = nullptr;
    m_leaguePresenter = nullptr;
    m_waitingLeagueBar = false;
    m_leagueCloseAt = 0;
    m_multichestWindow = nullptr;
    m_waitingMultichest = false;
    m_multichestCloseAt = 0;
    m_bundleWindow = nullptr;
    m_bundleCloseAt = 0;
    m_nextActionAt = GetTickCount64() + static_cast<unsigned long long>(m_delayMs);
    strncpy_s(m_status, "result window observed; advance scheduled", _TRUNCATE);
}

void AutomationFeature::OnLeagueBarShown(void* self) {
    if (!enabled || !m_leagueReady || m_mode == 0 || !self) return;
    const AutomationState state = m_coordinator.State();
    if (state == AutomationState::Idle || state == AutomationState::Done ||
        state == AutomationState::Error) return;
    m_leaguePresenter = self;
    m_waitingLeagueBar = true;
    m_leagueCloseAt = GetTickCount64() + static_cast<unsigned long long>(m_delayMs);
    strncpy_s(m_status, "LeagueBar shown; close scheduled", _TRUNCATE);
}

void AutomationFeature::OnLeagueBarUnlock() {
    if (!enabled || !m_waitingLeagueBar) return;
    m_leagueCloseAt = GetTickCount64() + static_cast<unsigned long long>(m_delayMs);
}

void AutomationFeature::OnLeagueBarTick() {
    if (!enabled || !m_waitingLeagueBar || !s_originalLeagueClose ||
        GetTickCount64() < m_leagueCloseAt) return;
    void* presenter = m_leaguePresenter;
    // Clear before calling into Unity so a re-entrant update cannot close the
    // same window twice.
    m_leaguePresenter = nullptr;
    m_waitingLeagueBar = false;
    m_leagueCloseAt = 0;
    if (!presenter) return;
    s_originalLeagueClose(presenter, nullptr);
    m_nextActionAt = GetTickCount64() + static_cast<unsigned long long>(m_delayMs);
    strncpy_s(m_status, "LeagueBar closed; next loop armed", _TRUNCATE);
}

void AutomationFeature::OnMultichestShown(void* self) {
    if (!enabled || !m_multichestReady || m_mode == 0 || !self || m_multichestWindow) return;
    const AutomationState state = m_coordinator.State();
    if (state == AutomationState::Idle || state == AutomationState::Done ||
        state == AutomationState::Error) return;
    m_multichestWindow = self;
    m_waitingMultichest = true;
    if (s_originalMultichestOpenAll) {
        s_originalMultichestOpenAll(self, nullptr);
        strncpy_s(m_status, "multichest opened; Open All invoked", _TRUNCATE);
    }
    // Leave two extra seconds for the card animation before OnCloseAction.
    m_multichestCloseAt = GetTickCount64() +
        static_cast<unsigned long long>(m_delayMs) + 2000ULL;
}

void AutomationFeature::OnMultichestHidden() {
    m_multichestWindow = nullptr;
    m_multichestCloseAt = 0;
    m_waitingMultichest = false;
    if (enabled) strncpy_s(m_status, "multichest closed; next loop armed", _TRUNCATE);
}

void AutomationFeature::OnMultichestTick() {
    if (!enabled || !m_waitingMultichest || !m_multichestWindow ||
        !s_originalMultichestCloseAction || m_multichestCloseAt == 0 ||
        GetTickCount64() < m_multichestCloseAt) return;
    void* window = m_multichestWindow;
    m_multichestWindow = nullptr;
    m_multichestCloseAt = 0;
    s_originalMultichestCloseAction(window, nullptr);
    // WindowHidden normally clears this.  Keep the guard set until that hook
    // fires so the next fight cannot overlap the card animation.
    strncpy_s(m_status, "multichest close requested", _TRUNCATE);
}

void AutomationFeature::OnBundleShown(void* self) {
    if (!enabled || !m_bundleReady || m_mode == 0 || !self) return;
    const AutomationState state = m_coordinator.State();
    if (state == AutomationState::Idle || state == AutomationState::Done ||
        state == AutomationState::Error) return;
    m_bundleWindow = self;
    m_bundleCloseAt = GetTickCount64() + static_cast<unsigned long long>(m_delayMs);
    strncpy_s(m_status, "bundle popup shown; dismiss scheduled", _TRUNCATE);
}

void AutomationFeature::OnPlayButtonUpdate(void* self) {
    if (!enabled || m_mode == 0 || m_coordinator.State() != AutomationState::StartingBattle) return;
    if (GetTickCount64() < m_nextActionAt || !s_originalPlayClick) return;
    m_nextActionAt = 0;
    s_originalPlayClick(self, nullptr);
    m_coordinator.OnEvent(AutomationEvent::BattleStarted);
    strncpy_s(m_status, "Play invoked; battle running", _TRUNCATE);
}

void AutomationFeature::OnResultWindowUpdate(void* self) {
    if (!enabled || m_mode == 0 || m_coordinator.State() != AutomationState::AwaitingResult) return;
    m_resultWindow = self;
    if (GetTickCount64() < m_nextActionAt) return;
    m_nextActionAt = 0;
    const bool delegated = InvokeButtonAction(self, m_onButtonActionOffset);
    if (!delegated) {
        if (s_originalPlayNext) s_originalPlayNext(self, nullptr);
        else if (s_originalHide) s_originalHide(self, nullptr);
    }
    m_coordinator.OnEvent(AutomationEvent::RewardCollected);
    strncpy_s(m_status, "result advanced; next loop scheduled", _TRUNCATE);
}

void AutomationFeature::OnResultPhaseContinued() {
    if (!enabled || m_coordinator.State() != AutomationState::AwaitingResult) return;
    m_nextActionAt = GetTickCount64() + static_cast<unsigned long long>(m_delayMs);
    strncpy_s(m_status, "result phase continued; advance rescheduled", _TRUNCATE);
}

void AutomationFeature::OnBattlefieldStart(void* self) {
    if (!enabled || m_mode != 2 || !m_derankReady || m_coordinator.State() != AutomationState::InBattle) return;
    if (m_derankPhase != DerankPhase::Idle) return;
    ReadObjectPointer(self, m_settingsOffset, &m_settingsButton);
    if (!m_settingsButton) { strncpy_s(m_status, "settings button pointer unavailable", _TRUNCATE); return; }
    m_derankPhase = DerankPhase::Settings;
    m_nextActionAt = GetTickCount64() + static_cast<unsigned long long>(m_delayMs);
    strncpy_s(m_status, "battle loaded; settings click scheduled", _TRUNCATE);
}

void AutomationFeature::OnBattleSettingsUpdate(void* self) {
    if (!enabled || m_mode != 2 || m_derankPhase != DerankPhase::CaptureSurrender) return;
    ReadObjectPointer(self, m_surrenderOffset, &m_surrenderButton);
    if (!m_surrenderButton) return;
    m_derankPhase = DerankPhase::Surrender;
    m_nextActionAt = GetTickCount64() + 1000;
    strncpy_s(m_status, "surrender button captured", _TRUNCATE);
}

void AutomationFeature::OnConfirmShown(void* self) {
    if (!enabled || m_mode != 2 || m_derankPhase != DerankPhase::AwaitConfirm) return;
    ReadObjectPointer(self, m_leftButtonOffset, &m_confirmButton);
    if (!m_confirmButton) return;
    m_derankPhase = DerankPhase::Confirm;
    m_nextActionAt = GetTickCount64() + 1000;
    strncpy_s(m_status, "surrender confirmation captured", _TRUNCATE);
}

void AutomationFeature::OnButtonUpdate(void* self) {
    if (!enabled || m_mode != 2 || !m_derankReady || GetTickCount64() < m_nextActionAt) return;
    if (m_derankPhase == DerankPhase::Settings && self == m_settingsButton) {
        if (s_originalHandleClick) s_originalHandleClick(self, nullptr);
        m_derankPhase = DerankPhase::CaptureSurrender;
        m_nextActionAt = GetTickCount64() + static_cast<unsigned long long>(m_delayMs);
        strncpy_s(m_status, "settings clicked; waiting for surrender", _TRUNCATE);
    } else if (m_derankPhase == DerankPhase::Surrender && self == m_surrenderButton) {
        if (s_originalHandleClick) s_originalHandleClick(self, nullptr);
        m_derankPhase = DerankPhase::AwaitConfirm;
        m_nextActionAt = 0;
        strncpy_s(m_status, "surrender clicked; waiting for confirmation", _TRUNCATE);
    } else if (m_derankPhase == DerankPhase::Confirm && self == m_confirmButton) {
        if (s_originalHandleClick) s_originalHandleClick(self, nullptr);
        m_derankPhase = DerankPhase::Idle;
        m_settingsButton = nullptr;
        m_surrenderButton = nullptr;
        m_confirmButton = nullptr;
        m_nextActionAt = 0;
        strncpy_s(m_status, "surrender confirmed; waiting for result", _TRUNCATE);
    }
}

static AutomationFeature g_automation;
static int g_automationRegistered = (RegisterFeature(&g_automation), 0);
