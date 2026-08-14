#include "automation_feature.h"

#include "../action_trace.h"
#include "../config_registry.h"
#include "../framework.h"
#include "../hook_registry.h"
#include "../il2cpp_resolve.h"
#include "../multichest_delegate_guard.h"
#include "../../minhook/include/MinHook.h"
#include "imgui.h"
#include <cstdint>

namespace {
using ShowWindowFn = void(__fastcall*)(void*, void*, void*, void*, void*, void*);
using UpdateFn = void(__fastcall*)(void*, void*);
using PlayClickFn = void(__fastcall*)(void*, void*);
using ResultActionFn = void(__fastcall*)(void*, void*);
using ButtonFn = void(__fastcall*)(void*, void*);
using IdleChestShowFn = void(__fastcall*)(void*, void*, void*);
using ClaimRewardShowFn = void(__fastcall*)(void*, void*, void*, void*);
using FocusFn = void(__fastcall*)(void*, void*, void*);
using DelegateInvokeFn = void(__fastcall*)(void*, void*);
using AutoBattleGetterFn = void*(__fastcall*)(void*, void*);
using AutoBattleClickFn = void(__fastcall*)(void*, void*);
using AutoBattleActiveFn = bool(__fastcall*)(void*, void*);
using ItemModuleGetterFn = void*(__fastcall*)(void*);
using IsEnoughResourceFn = bool(__fastcall*)(void*, int32_t, int32_t, void*);
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
static MultichestShowFn s_originalMultichestShow = nullptr;
static ButtonFn s_originalMultichestOnEnable = nullptr;
static ButtonFn s_originalMultichestStart = nullptr;
static ButtonFn s_originalMultichestOpenAll = nullptr;
static ButtonFn s_originalMultichestButtonClick = nullptr;
static ButtonFn s_originalMultichestCloseWindow = nullptr;
static ButtonFn s_originalMultichestCloseAction = nullptr;
static ButtonFn s_originalMultichestHidden = nullptr;
static FocusFn s_originalBundleFocus = nullptr;
static ButtonFn s_originalBundleClose = nullptr;
static AutoBattleGetterFn s_originalAutoBattleGetter = nullptr;
static AutoBattleClickFn s_originalAutoBattleClick = nullptr;
static AutoBattleActiveFn s_originalAutoBattleActive = nullptr;
static ButtonFn s_originalAutoBattleStart = nullptr;
static ButtonFn s_originalAutoBattleInactive = nullptr;
static ItemModuleGetterFn s_originalItemModuleGetter = nullptr;
static IsEnoughResourceFn s_originalIsEnoughResource = nullptr;
static ButtonFn s_originalClaimRewards = nullptr;
static IdleChestShowFn s_originalIdleChestShow = nullptr;
static ButtonFn s_originalIdleChestPreclaim = nullptr;
static ShowWindowFn s_originalRewardClaimShow = nullptr;
static ButtonFn s_originalRewardClaimAction = nullptr;
static ButtonFn s_originalRewardClaimClose = nullptr;
static ClaimRewardShowFn s_originalClaimRewardShow = nullptr;
static ButtonFn s_originalClaimRewardClose = nullptr;
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

static void* __fastcall MultichestShowHook(
    void* provider, void* controller, void* adsManager, int32_t firstChestRewardCount,
    void* onClose, void* beforeHideAction, void* methodInfo) {
    uintptr_t moduleBegin = 0;
    uintptr_t moduleEnd = 0;
    GetLoadedModuleImageRange("GameAssembly.dll", &moduleBegin, &moduleEnd);
    MultichestDelegateGuardResult guardResult = MultichestDelegateGuardResult::InvalidInput;
    void* invokeImpl = nullptr;
    void* promise = ForwardMultichestShowWithGuard(s_originalMultichestShow,
        moduleBegin, moduleEnd, provider, controller, adsManager, firstChestRewardCount,
        onClose, beforeHideAction, methodInfo, &guardResult, &invokeImpl);
    LOG("[AUTOMATION] multichest static show guard=%s provider=%p beforeHide=%p invoke=%p count=%d",
        MultichestDelegateGuardResultName(guardResult), provider, beforeHideAction,
        invokeImpl, firstChestRewardCount);
    return promise;
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

static void __fastcall ClaimRewardsHook(void* self, void* mi) {
    if (s_originalClaimRewards) s_originalClaimRewards(self, mi);
    if (s_owner) s_owner->OnRewardsClaimed();
}

static void __fastcall IdleChestShowHook(void* self, void* onClose, void* mi) {
    if (s_originalIdleChestShow) s_originalIdleChestShow(self, onClose, mi);
    if (s_owner) s_owner->OnIdleChestShown(self);
}

static void __fastcall IdleChestPreclaimHook(void* self, void* mi) {
    if (s_originalIdleChestPreclaim) s_originalIdleChestPreclaim(self, mi);
    if (s_owner) s_owner->OnIdleChestPreclaim(self);
}

static void __fastcall RewardClaimShowHook(void* self, void* actions, void* showResourcesBar,
                                           void* onHide, void* onStartAnimating, void* mi) {
    if (s_originalRewardClaimShow) s_originalRewardClaimShow(self, actions, showResourcesBar, onHide, onStartAnimating, mi);
    if (s_owner) s_owner->OnRewardClaimShown(self);
}

static void __fastcall ClaimRewardShowHook(void* self, void* onAnimation, void* onHideAction, void* mi) {
    if (s_originalClaimRewardShow) s_originalClaimRewardShow(self, onAnimation, onHideAction, mi);
    if (s_owner) s_owner->OnClaimRewardShown(self);
}

static void __fastcall AutoBattleStartHook(void* self, void* mi) {
    if (s_originalAutoBattleStart) s_originalAutoBattleStart(self, mi);
    if (s_owner) s_owner->OnAutoBattleControllerStarted(self);
}

static void __fastcall AutoBattleInactiveHook(void* self, void* mi) {
    if (s_originalAutoBattleInactive) s_originalAutoBattleInactive(self, mi);
    if (s_owner) s_owner->OnAutoBattleInactive(self);
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

static bool ReadObjectBool(void* object, int32_t offset, bool* value) {
    if (!object || !value || offset < 0) return false;
    __try {
        *value = *reinterpret_cast<bool*>(reinterpret_cast<std::uint8_t*>(object) + offset);
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        *value = false;
        return false;
    }
}

static bool ReadObjectInt32(void* object, int32_t offset, int32_t* value) {
    if (!object || !value || offset < 0) return false;
    __try {
        *value = *reinterpret_cast<int32_t*>(reinterpret_cast<std::uint8_t*>(object) + offset);
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        *value = 0;
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

// MatchCompletedWindow's _onButtonAction field moves between game builds.
// battle_cheat.js therefore scans the managed object for a delegate instead of
// relying on one stale field offset.  Keep the same scan, but validate the
// delegate method against GameAssembly before invoking it.
static bool IsGameAssemblyAddress(void* address) {
    if (!address) return false;
    HMODULE module = GetModuleHandleA("GameAssembly.dll");
    if (!module) return false;
    __try {
        auto* dos = reinterpret_cast<IMAGE_DOS_HEADER*>(module);
        if (dos->e_magic != IMAGE_DOS_SIGNATURE) return false;
        auto* nt = reinterpret_cast<IMAGE_NT_HEADERS64*>(
            reinterpret_cast<std::uint8_t*>(module) + dos->e_lfanew);
        if (nt->Signature != IMAGE_NT_SIGNATURE) return false;
        const uintptr_t begin = reinterpret_cast<uintptr_t>(module);
        const uintptr_t end = begin + nt->OptionalHeader.SizeOfImage;
        const uintptr_t value = reinterpret_cast<uintptr_t>(address);
        return value >= begin && value < end;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}

static bool InvokeMovingButtonAction(void* window) {
    if (!window) return false;
    for (int32_t offset = 0x130; offset <= 0x180; offset += 8) {
        void* delegate = nullptr;
        if (!ReadObjectPointer(window, offset, &delegate) || !delegate) continue;
        void* method = nullptr;
        void* target = nullptr;
        void* methodInfo = nullptr;
        if (!ReadObjectPointer(delegate, 0x10, &method) ||
            !ReadObjectPointer(delegate, 0x20, &target) ||
            !ReadObjectPointer(delegate, 0x28, &methodInfo) ||
            !IsGameAssemblyAddress(method)) continue;
        __try {
            LOG("[AUTOMATION] result delegate candidate offset=0x%X method=%p", offset, method);
            reinterpret_cast<DelegateInvokeFn>(method)(target, methodInfo);
            LOG("[AUTOMATION] result delegate invoked offset=0x%X", offset);
            return true;
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            LOG("[AUTOMATION] result delegate failed offset=0x%X code=0x%08X", offset, GetExceptionCode());
        }
    }
    return false;
}

static bool CallResultActionSafe(ResultActionFn action, void* window, const char* label) {
    if (!action || !window) return false;
    __try {
        LOG("[AUTOMATION] result action begin=%s self=%p", label ? label : "?", window);
        action(window, nullptr);
        LOG("[AUTOMATION] result action end=%s", label ? label : "?");
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        LOG("[AUTOMATION] result action SEH=%s code=0x%08X", label ? label : "?", GetExceptionCode());
        return false;
    }
}

static bool HasLapisBalance(int resourceType, int minimum) {
    if (!s_originalItemModuleGetter || !s_originalIsEnoughResource) return false;
    void* itemModule = nullptr;
    __try { itemModule = s_originalItemModuleGetter(nullptr); } __except (EXCEPTION_EXECUTE_HANDLER) { itemModule = nullptr; }
    if (!itemModule) return false;
    __try { return s_originalIsEnoughResource(itemModule, resourceType, minimum, nullptr); }
    __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
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
    GlobalConfigRegistry().RegisterInteger("automation.lapis_resource_type", &m_lapisResourceType);
    GlobalConfigRegistry().RegisterInteger("automation.lapis_minimum", &m_lapisMinimum);
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
    void* autoBattleGetter = ResolveMethodOrFallback("Assembly-CSharp",
        "AutoChess.UIScripts.WindowScripts.BattlefieldWindow", "BattlefieldWindow", "get_AutoBattleController", 0);
    void* autoBattleClick = ResolveMethodOrFallback("Assembly-CSharp",
        "AutoChess.CoreGameplay.Fight.AutoBattle", "AutoBattleController", "OnAutoBattleClicked", 0);
    void* autoBattleActive = ResolveMethodOrFallback("Assembly-CSharp",
        "AutoChess.CoreGameplay.Fight.AutoBattle", "AutoBattleController", "IsAutoBattleActive", 0);
    void* autoBattleStart = ResolveMethodOrFallback("Assembly-CSharp",
        "AutoChess.CoreGameplay.Fight.AutoBattle", "AutoBattleController", "Start", 0);
    void* autoBattleInactive = ResolveMethodOrFallback("Assembly-CSharp",
        "AutoChess.CoreGameplay.Fight.AutoBattle", "AutoBattleController", "SetAutoBattleInactive", 0);
    s_originalAutoBattleGetter = reinterpret_cast<AutoBattleGetterFn>(autoBattleGetter);
    s_originalAutoBattleClick = reinterpret_cast<AutoBattleClickFn>(autoBattleClick);
    s_originalAutoBattleActive = reinterpret_cast<AutoBattleActiveFn>(autoBattleActive);
    m_autoBattleStartHooked = InstallObserver("automation.autobattle.start",
        autoBattleStart, ButtonFn(&AutoBattleStartHook), &s_originalAutoBattleStart);
    InstallObserver("automation.autobattle.inactive", autoBattleInactive,
        ButtonFn(&AutoBattleInactiveHook), &s_originalAutoBattleInactive);
    m_autoBattleCoreOffset = ResolveFieldOffset("Assembly-CSharp",
        "AutoChess.CoreGameplay.Fight.AutoBattle", "AutoBattleController", "_autoBattleCore");
    m_autoBattleUnlockedOffset = ResolveFieldOffset("Assembly-CSharp",
        "AutoChess.CoreGameplay.Fight.AutoBattle", "AutoBattleController", "_autoBattleUnlocked");
    if (m_autoBattleCoreOffset < 0) m_autoBattleCoreOffset = 0x40;
    if (m_autoBattleUnlockedOffset < 0) m_autoBattleUnlockedOffset = 0x48;
    m_autoBattleReady = IsAutoBattleRuntimeReady(
        s_originalAutoBattleGetter != nullptr,
        s_originalAutoBattleClick != nullptr,
        s_originalAutoBattleActive != nullptr,
        m_autoBattleCoreOffset >= 0 && m_autoBattleUnlockedOffset >= 0);
    void* itemGetter = ResolveMethodOrFallback("Assembly-CSharp",
        "AutoChess.DataClasses.UserData", "ItemModule", "get_Instance", 0);
    void* enoughResource = ResolveMethodOrFallback("Assembly-CSharp",
        "AutoChess.DataClasses.UserData", "ItemModule", "IsEnoughResource", 2);
    s_originalItemModuleGetter = reinterpret_cast<ItemModuleGetterFn>(itemGetter);
    s_originalIsEnoughResource = reinterpret_cast<IsEnoughResourceFn>(enoughResource);
    m_lapisGateReady = s_originalItemModuleGetter && s_originalIsEnoughResource;
    void* claimRewards = ResolveMethodOrFallback("Assembly-CSharp",
        "AutoChess.Journey.JourneyModuleABC.Service", "ExternalJourneyFighter", "ClaimRewardsOnWin", 0);
    s_originalClaimRewards = reinterpret_cast<ButtonFn>(claimRewards);
    if (claimRewards)
        InstallObserver("automation.rewards.claim", claimRewards, &ClaimRewardsHook, &s_originalClaimRewards);
    void* idleChestShow = ResolveMethodOrFallback("Assembly-CSharp",
        "JourneyModuleMP.IdleChest", "IdleChestPresenter", "ShowWindow", 1);
    void* idleChestPreclaim = ResolveMethodOrFallback("Assembly-CSharp",
        "JourneyModuleMP.IdleChest", "IdleChestPresenter", "OnPreclaimRewards", 0);
    s_originalIdleChestShow = reinterpret_cast<IdleChestShowFn>(idleChestShow);
    s_originalIdleChestPreclaim = reinterpret_cast<ButtonFn>(idleChestPreclaim);
    const bool idleChestShowHooked = idleChestShow &&
        InstallObserver("automation.idle-chest.show", idleChestShow,
            &IdleChestShowHook, &s_originalIdleChestShow);
    const bool idleChestPreclaimHooked = idleChestPreclaim &&
        InstallObserver("automation.idle-chest.preclaim", idleChestPreclaim,
            &IdleChestPreclaimHook, &s_originalIdleChestPreclaim);

    void* rewardClaimShow = ResolveMethodOrFallback("Assembly-CSharp",
        "JourneyModuleMP.RewardClaim", "RewardClaimPresenter", "ShowWindow", 4);
    void* rewardClaimAction = ResolveMethodOrFallback("Assembly-CSharp",
        "JourneyModuleMP.RewardClaim", "RewardClaimPresenter", "OnClaimAction", 0);
    void* rewardClaimClose = ResolveMethodOrFallback("Assembly-CSharp",
        "JourneyModuleMP.RewardClaim", "RewardClaimPresenter", "CloseWindow", 0);
    s_originalRewardClaimAction = reinterpret_cast<ButtonFn>(rewardClaimAction);
    s_originalRewardClaimClose = reinterpret_cast<ButtonFn>(rewardClaimClose);
    const bool rewardClaimShowHooked = rewardClaimShow &&
        InstallObserver("automation.reward-claim.show", rewardClaimShow,
            &RewardClaimShowHook, &s_originalRewardClaimShow);

    void* claimRewardShow = ResolveMethodOrFallback("Assembly-CSharp",
        "UI_Scripts.WindowManager", "ClaimRewardWindow", "Show", 2);
    void* claimRewardClose = ResolveMethodOrFallback("Assembly-CSharp",
        "UI_Scripts.WindowManager", "ClaimRewardWindow", "Close", 0);
    s_originalClaimRewardClose = reinterpret_cast<ButtonFn>(claimRewardClose);
    const bool claimRewardShowHooked = claimRewardShow &&
        InstallObserver("automation.claim-reward.show", claimRewardShow,
            &ClaimRewardShowHook, &s_originalClaimRewardShow);

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
    m_multichestButtonOffset = ResolveFieldOffset("Assembly-CSharp",
        "UI_Scripts.WindowManager", "MultichestWindow", "openCardsButton");
    m_multichestBeforeHideActionOffset = ResolveFieldOffset("Assembly-CSharp",
        "UI_Scripts.WindowManager", "MultichestWindow", "_beforeHideAction");
    m_multichestRewardCountOffset = ResolveFieldOffset("Assembly-CSharp",
        "UI_Scripts.WindowManager", "MultichestWindow", "_rewardCount");
    m_multichestFreeCountOffset = ResolveFieldOffset("Assembly-CSharp",
        "UI_Scripts.WindowManager", "MultichestWindow", "_freeCount");
    m_multichestActualCountOffset = ResolveFieldOffset("Assembly-CSharp",
        "UI_Scripts.WindowManager", "MultichestWindow", "_actualCount");
    m_multichestCurrentCountOffset = ResolveFieldOffset("Assembly-CSharp",
        "UI_Scripts.WindowManager", "MultichestWindow", "_currentCount");
    m_multichestOpenAllActiveOffset = ResolveFieldOffset("Assembly-CSharp",
        "UI_Scripts.WindowManager", "MultichestWindow", "_openAllActive");
    m_multichestPressedOpenAllOffset = ResolveFieldOffset("Assembly-CSharp",
        "UI_Scripts.WindowManager", "MultichestWindow", "_pressedOpenAll");
    m_multichestCardsAppearAnimDoneOffset = ResolveFieldOffset("Assembly-CSharp",
        "UI_Scripts.WindowManager", "MultichestWindow", "_cardsAppearAnimDone");
    m_multichestOpenAllLockOffset = ResolveFieldOffset("Assembly-CSharp",
        "UI_Scripts.WindowManager", "MultichestWindow", "_openAllLock");
    m_multichestButtonsActiveOffset = ResolveFieldOffset("Assembly-CSharp",
        "UI_Scripts.WindowManager", "MultichestWindow", "_buttonsActive");
    // IL2CPP exports are absent in the injected runtime, so metadata field
    // traversal returns -1. These offsets were verified live with REToolkit
    // Frida against the current GameAssembly build.
    if (m_settingsOffset < 0) m_settingsOffset = 0x70; // BattlefieldWindow.settingsButton
    if (m_surrenderOffset < 0) m_surrenderOffset = 0x88; // BattleSettingsWindow.surrenderButton
    if (m_leftButtonOffset < 0) m_leftButtonOffset = 0x90; // TwoButtonWindow.leftButton
    if (m_multichestButtonOffset < 0) m_multichestButtonOffset = 0xA8; // MultichestWindow.openCardsButton
    if (m_multichestBeforeHideActionOffset < 0) m_multichestBeforeHideActionOffset = 0x130;
    if (m_multichestRewardCountOffset < 0) m_multichestRewardCountOffset = 0x148;
    if (m_multichestFreeCountOffset < 0) m_multichestFreeCountOffset = 0x14C;
    if (m_multichestActualCountOffset < 0) m_multichestActualCountOffset = 0x150;
    if (m_multichestCurrentCountOffset < 0) m_multichestCurrentCountOffset = 0x154;
    if (m_multichestOpenAllActiveOffset < 0) m_multichestOpenAllActiveOffset = 0x168;
    if (m_multichestPressedOpenAllOffset < 0) m_multichestPressedOpenAllOffset = 0x169;
    if (m_multichestCardsAppearAnimDoneOffset < 0) m_multichestCardsAppearAnimDoneOffset = 0x16A;
    if (m_multichestOpenAllLockOffset < 0) m_multichestOpenAllLockOffset = 0x16B;
    if (m_multichestButtonsActiveOffset < 0) m_multichestButtonsActiveOffset = 0x188;
    LOG("[AUTOMATION] derank field fallback settings=0x%X surrender=0x%X confirm=0x%X",
        m_settingsOffset, m_surrenderOffset, m_leftButtonOffset);
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

    // ShowMultichestWindow is static and has six managed parameters. Its hook
    // must preserve every argument; OnEnable/Start provide the actual pooled
    // MultichestWindow instance used by the automation state machine.
    void* multichestShow = ResolveMethodOrFallback("Assembly-CSharp",
        "UI_Scripts.WindowManager", "MultichestWindow", "ShowMultichestWindow", 6);
    void* multichestEnable = ResolveMethodOrFallback("Assembly-CSharp",
        "UI_Scripts.WindowManager", "MultichestWindow", "OnEnable", 0);
    void* multichestStart = ResolveMethodOrFallback("Assembly-CSharp",
        "UI_Scripts.WindowManager", "MultichestWindow", "Start", 0);
    void* multichestOpenAll = ResolveMethodOrFallback("Assembly-CSharp",
        "UI_Scripts.WindowManager", "MultichestWindow", "OnOpenAll", 0);
    void* multichestButtonClick = ResolveMethodOrFallback("Assembly-CSharp", "", "ButtonListener", "OnClick", 0);
    void* multichestCloseWindow = ResolveMethodOrFallback("Assembly-CSharp",
        "UI_Scripts.WindowManager", "MultichestWindow", "CloseWindow", 0);
    void* multichestClose = ResolveMethodOrFallback("Assembly-CSharp",
        "UI_Scripts.WindowManager", "MultichestWindow", "OnCloseAction", 0);
    void* multichestHidden = ResolveMethodOrFallback("Assembly-CSharp",
        "UI_Scripts.WindowManager", "MultichestWindow", "WindowHidden", 0);
    const bool mh1 = InstallObserver("automation.multichest.show", multichestShow,
        &MultichestShowHook, &s_originalMultichestShow);
    const bool mh2 = InstallObserver("automation.multichest.enable", multichestEnable,
        ButtonFn(&MultichestEnableHook), &s_originalMultichestOnEnable);
    const bool mh3 = InstallObserver("automation.multichest.start", multichestStart,
        ButtonFn(&MultichestStartHook), &s_originalMultichestStart);
    const bool mh4 = InstallObserver("automation.multichest.hidden", multichestHidden,
        ButtonFn(&MultichestHiddenHook), &s_originalMultichestHidden);
    s_originalMultichestOpenAll = reinterpret_cast<ButtonFn>(multichestOpenAll);
    s_originalMultichestButtonClick = reinterpret_cast<ButtonFn>(multichestButtonClick);
    if (!s_originalMultichestButtonClick) {
        HMODULE gameAssembly = GetModuleHandleA("GameAssembly.dll");
        if (gameAssembly)
            s_originalMultichestButtonClick = reinterpret_cast<ButtonFn>(
                reinterpret_cast<uintptr_t>(gameAssembly) + 0x9403B0);
    }
    s_originalMultichestCloseWindow = reinterpret_cast<ButtonFn>(multichestCloseWindow);
    s_originalMultichestCloseAction = reinterpret_cast<ButtonFn>(multichestClose);
    m_multichestReady = (mh2 || mh3) && s_originalMultichestOpenAll &&
        (s_originalMultichestCloseWindow || s_originalMultichestCloseAction) &&
        m_multichestBeforeHideActionOffset >= 0 && mh4;

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
    LOG("[AUTOMATION] auto-rank controller=%p click=%p active=%p start=%p startHook=%d core=0x%X unlocked=0x%X ready=%d; lapis getter=%p enough=%p gate=%d type=%d min=%d claim=%p idleChest show=%p preclaim=%p hooked=%d/%d",
        autoBattleGetter, autoBattleClick, autoBattleActive, autoBattleStart,
        m_autoBattleStartHooked ? 1 : 0, m_autoBattleCoreOffset,
        m_autoBattleUnlockedOffset, m_autoBattleReady ? 1 : 0,
        itemGetter, enoughResource, m_lapisGateReady ? 1 : 0, m_lapisResourceType, m_lapisMinimum, claimRewards,
        idleChestShow, idleChestPreclaim, idleChestShowHooked ? 1 : 0, idleChestPreclaimHooked ? 1 : 0);
    LOG("[AUTOMATION] reward-claim show=%p action=%p close=%p hooked=%d; claim-reward show=%p close=%p hooked=%d",
        rewardClaimShow, rewardClaimAction, rewardClaimClose, rewardClaimShowHooked ? 1 : 0,
        claimRewardShow, claimRewardClose, claimRewardShowHooked ? 1 : 0);
    LOG("[AUTOMATION] derank methods=%d/%d/%d/%d offsets=%d/%d/%d resultDelegate=%d ready=%d",
        h1 ? 1 : 0, h2 ? 1 : 0, h3 ? 1 : 0, h4 ? 1 : 0,
        m_settingsOffset, m_surrenderOffset, m_leftButtonOffset,
        m_onButtonActionOffset, m_derankReady ? 1 : 0);
    LOG("[AUTOMATION] league methods shown=%p close=%p unlock=%p scroll=%p counter=%p hooked=%d/%d/%d/%d ready=%d",
        leagueShown, leagueClose, leagueUnlock, leagueScroll, leagueCounter,
        lh1 ? 1 : 0, lh2 ? 1 : 0, lh3 ? 1 : 0, lh4 ? 1 : 0,
        m_leagueReady ? 1 : 0);
    LOG("[AUTOMATION] multichest methods show=%p enable=%p start=%p openAll=%p button=0x%X beforeHide=0x%X closeWindow=%p closeAction=%p hidden=%p hooked=%d/%d/%d/%d ready=%d",
        multichestShow, multichestEnable, multichestStart, multichestOpenAll,
        m_multichestButtonOffset, m_multichestBeforeHideActionOffset, multichestCloseWindow, multichestClose, multichestHidden, mh1 ? 1 : 0, mh2 ? 1 : 0,
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
    if (mode == AutomationMode::AutoBattle && m_lapisGateReady &&
        m_coordinator.State() != AutomationState::Idle &&
        m_coordinator.State() != AutomationState::Done &&
        !HasLapisBalance(m_lapisResourceType, m_lapisMinimum)) {
        m_coordinator.Stop();
        m_autoBattleArmed = false;
        m_autoBattleInitWaitLogged = false;
        m_nextActionAt = 0;
        m_multichestOpenAt = 0;
        m_multichestCloseAt = 0;
        m_multichestWindow = nullptr;
        m_waitingMultichest = false;
        strncpy_s(m_status, "Auto Rank stopped: Lapis below minimum", _TRUNCATE);
        actiontrace::Push("err", "Auto Rank stopped: Lapis below minimum");
        return;
    }
    if (m_coordinator.State() == AutomationState::Cooldown &&
        !m_waitingLeagueBar && !m_waitingMultichest && !m_bundleWindow)
        m_coordinator.OnEvent(AutomationEvent::CooldownElapsed);
    if (mode == AutomationMode::AutoBattle && m_autoBattleArmed &&
        GetTickCount64() >= m_autoBattleAt) {
        if (!m_autoBattleController && m_autoBattleWindow && s_originalAutoBattleGetter)
            m_autoBattleController = s_originalAutoBattleGetter(m_autoBattleWindow, nullptr);
        const unsigned long long now = GetTickCount64();
        void* autoBattleCore = nullptr;
        bool unlocked = false;
        const bool hasController = m_autoBattleController != nullptr && m_autoBattleControllerStarted;
        const bool hasCore = hasController &&
            ReadObjectPointer(m_autoBattleController, m_autoBattleCoreOffset, &autoBattleCore) &&
            autoBattleCore != nullptr;
        const bool hasUnlockState = hasController &&
            ReadObjectBool(m_autoBattleController, m_autoBattleUnlockedOffset, &unlocked);
        bool active = false;
        if (hasCore && s_originalAutoBattleActive) {
            __try { active = s_originalAutoBattleActive(m_autoBattleController, nullptr); }
            __except (EXCEPTION_EXECUTE_HANDLER) { active = false; }
        }
        const AutoBattleTriggerDecision decision = DecideAutoBattleTrigger(
            hasController, hasCore, hasUnlockState && unlocked, active);
        if (decision == AutoBattleTriggerDecision::WaitForController ||
            decision == AutoBattleTriggerDecision::WaitForInitialization) {
            m_autoBattleAt = now + 250ULL;
            ++m_autoBattleRetryCount;
            if (!m_autoBattleInitWaitLogged) {
                LOG("[AUTOMATION] Auto Rank autoplay waiting controller=%p started=%d core=%p unlocked=%d retry=%d time=%llu method=%p",
                    m_autoBattleController, m_autoBattleControllerStarted ? 1 : 0, autoBattleCore,
                    unlocked ? 1 : 0, m_autoBattleRetryCount, now, s_originalAutoBattleClick);
                m_autoBattleInitWaitLogged = true;
            }
            strncpy_s(m_status, "Auto Rank waiting for autoplay initialization", _TRUNCATE);
        } else if (decision == AutoBattleTriggerDecision::Complete) {
            m_autoBattleArmed = false;
            m_autoBattleClickInvoked = false;
            m_autoBattleInitWaitLogged = false;
            LOG("[AUTOMATION] Auto Rank autoplay confirmed active controller=%p",
                m_autoBattleController);
            strncpy_s(m_status, "Auto Rank autoplay confirmed active", _TRUNCATE);
        } else if (s_originalAutoBattleClick && !m_autoBattleClickInvoked) {
            __try {
                LOG("[AUTOMATION] Auto Rank autoplay invoke controller=%p core=%p unlocked=1 activeBefore=0 retry=%d time=%llu method=%p",
                    m_autoBattleController, autoBattleCore, m_autoBattleRetryCount, now, s_originalAutoBattleClick);
                s_originalAutoBattleClick(m_autoBattleController, nullptr);
                m_autoBattleClickInvoked = true;
                m_autoBattleAt = now + 250ULL;
                m_autoBattleInitWaitLogged = false;
                strncpy_s(m_status, "Auto Rank autoplay trigger sent; awaiting confirmation", _TRUNCATE);
            } __except (EXCEPTION_EXECUTE_HANDLER) {
                m_autoBattleAt = now + 1000ULL;
                m_autoBattleClickInvoked = false;
                strncpy_s(m_status, "Auto Rank autoplay call failed; retry scheduled", _TRUNCATE);
            }
        } else if (m_autoBattleClickInvoked) {
            m_autoBattleAt = now + 250ULL;
            ++m_autoBattleRetryCount;
        }
    }
    OnMultichestTick();
    if (m_coordinator.State() == AutomationState::CollectingReward &&
        m_idleChestPresenter && !m_idleChestPreclaimInvoked && s_originalIdleChestPreclaim &&
        GetTickCount64() >= m_idleChestPreclaimAt) {
        void* presenter = m_idleChestPresenter;
        // Mark before entering Unity: the callback can synchronously re-enter
        // this update path while the chest begins its claim animation.
        m_idleChestPreclaimInvoked = true;
        __try {
            s_originalIdleChestPreclaim(presenter, nullptr);
            LOG("[AUTOMATION] idle chest preclaim dispatched presenter=%p method=%p", presenter,
                s_originalIdleChestPreclaim);
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            m_idleChestPreclaimInvoked = false;
            strncpy_s(m_status, "pre-reward chest claim failed; retry pending", _TRUNCATE);
        }
    }
    if (m_coordinator.State() == AutomationState::CollectingReward && m_rewardClaimPresenter) {
        const unsigned long long now = GetTickCount64();
        void* presenter = m_rewardClaimPresenter;
        if (!m_rewardClaimActionInvoked && s_originalRewardClaimAction &&
            now >= m_rewardClaimAt) {
            m_rewardClaimActionInvoked = true;
            if (CallResultActionSafe(reinterpret_cast<ResultActionFn>(s_originalRewardClaimAction),
                    presenter, "RewardClaimPresenter.OnClaimAction")) {
                m_rewardClaimCloseAt = now + 250ULL;
                strncpy_s(m_status, "reward claim action dispatched; close scheduled", _TRUNCATE);
            } else {
                m_rewardClaimActionInvoked = false;
                m_rewardClaimAt = now + 500ULL;
                strncpy_s(m_status, "reward claim action failed; retry pending", _TRUNCATE);
            }
        } else if (m_rewardClaimActionInvoked && s_originalRewardClaimClose &&
            now >= m_rewardClaimCloseAt) {
            m_rewardClaimPresenter = nullptr;
            m_rewardClaimActionInvoked = false;
            m_rewardClaimAt = 0;
            m_rewardClaimCloseAt = 0;
            CallResultActionSafe(reinterpret_cast<ResultActionFn>(s_originalRewardClaimClose),
                presenter, "RewardClaimPresenter.CloseWindow");
            strncpy_s(m_status, "reward claim section closed", _TRUNCATE);
            actiontrace::Push("automation", "reward claim section closed");
        }
    }
    if (m_coordinator.State() == AutomationState::CollectingReward && m_claimRewardWindow &&
        s_originalClaimRewardClose && GetTickCount64() >= m_claimRewardCloseAt) {
        void* window = m_claimRewardWindow;
        m_claimRewardWindow = nullptr;
        m_claimRewardCloseAt = 0;
        CallResultActionSafe(reinterpret_cast<ResultActionFn>(s_originalClaimRewardClose),
            window, "ClaimRewardWindow.Close");
        strncpy_s(m_status, "claim reward window closed", _TRUNCATE);
        actiontrace::Push("automation", "claim reward window closed");
    }
    if (m_coordinator.State() == AutomationState::CollectingReward) {
        const unsigned long long now = GetTickCount64();
        const bool claimGraceElapsed = m_rewardsClaimObserved && m_rewardsClaimAt != 0 &&
            now >= m_rewardsClaimAt + 5000ULL;
        const RewardSettlementDecision settlement = DecideRewardSettlement(
            m_waitingMultichest, m_bundleWindow != nullptr, m_waitingLeagueBar,
            claimGraceElapsed, m_rewardSettlementDeadline != 0 && now >= m_rewardSettlementDeadline);
        if (settlement == RewardSettlementDecision::CompleteAfterConfirmedClaim) {
            m_coordinator.OnEvent(AutomationEvent::RewardCollected);
            m_rewardsClaimObserved = false;
            m_rewardsClaimAt = 0;
            m_rewardSettlementDeadline = 0;
            m_nextActionAt = now + static_cast<unsigned long long>(m_delayMs);
            strncpy_s(m_status, "reward claim confirmed; next loop armed", _TRUNCATE);
        } else if (settlement == RewardSettlementDecision::ManualClaimRequired) {
            m_nextActionAt = 0;
            strncpy_s(m_status, "reward settlement pending; claim/window required", _TRUNCATE);
        }
    }
    if (m_bundleWindow && m_bundleCloseAt != 0 && GetTickCount64() >= m_bundleCloseAt) {
        void* bundle = m_bundleWindow;
        m_bundleWindow = nullptr;
        m_bundleCloseAt = 0;
        if (!CallResultActionSafe(reinterpret_cast<ResultActionFn>(s_originalBundleClose), bundle, "BundleForceShowWindow.OnClose"))
            m_status[0] = '\0';
        strncpy_s(m_status, "bundle popup dismissed", _TRUNCATE);
        actiontrace::Push("automation", "bundle popup dismissed");
    }
    if (before == AutomationState::Cooldown && m_coordinator.State() == AutomationState::StartingBattle)
        m_nextActionAt = GetTickCount64() + static_cast<unsigned long long>(m_delayMs);
    if (m_coordinator.State() == AutomationState::StartingBattle) {
        const unsigned long long now = GetTickCount64();
        const PlayInvokeDecision decision = DecidePlayInvoke(
            true, m_playButton != nullptr, s_originalPlayClick != nullptr,
            m_nextActionAt != 0 && now >= m_nextActionAt);
        if (decision == PlayInvokeDecision::InvokeViaWatchdog) {
            if (mode == AutomationMode::AutoBattle && m_lapisGateReady &&
                !HasLapisBalance(m_lapisResourceType, m_lapisMinimum)) {
                m_coordinator.Stop();
                strncpy_s(m_status, "Auto Rank stopped before Play: no Lapis", _TRUNCATE);
                return;
            }
            void* button = m_playButton;
            m_playButton = nullptr;
            m_nextActionAt = 0;
            __try {
                s_originalPlayClick(button, nullptr);
                m_coordinator.OnEvent(AutomationEvent::BattleStarted);
                strncpy_s(m_status, "Play invoked via watchdog; battle running", _TRUNCATE);
                actiontrace::Push("automation", "Play invoked via watchdog");
            } __except (EXCEPTION_EXECUTE_HANDLER) {
                m_nextActionAt = now + 500ULL;
                m_playButton = button;
                strncpy_s(m_status, "watchdog play call failed; retry scheduled", _TRUNCATE);
            }
        }
    }
}

void AutomationFeature::OnMenu() {
    if (!enabled) return;
    const char* modes[] = {"Off", "AutoBattle", "Derank"};
    ImGui::Combo("Automation mode", &m_mode, modes, 3);
    ImGui::SliderInt("Delay (ms)", &m_delayMs, 250, 30000);
    ImGui::SliderInt("Max loops (0 = unlimited)", &m_maxLoops, 0, 100);
    ImGui::Text("Derank readiness: %s", m_derankReady ? "ready" : "field layout unresolved");
    ImGui::Text("Auto Rank: %s (start=%s)  Lapis gate: %s (type=%d min=%d)",
        m_autoBattleReady ? "ready" : "runtime unresolved",
        m_autoBattleStartHooked ? "observer" : "battlefield fallback",
        m_lapisGateReady ? "ready" : "unresolved", m_lapisResourceType, m_lapisMinimum);
    ImGui::InputInt("Lapis resource type", &m_lapisResourceType);
    ImGui::InputInt("Minimum Lapis", &m_lapisMinimum);
    if (m_lapisResourceType < 0) m_lapisResourceType = 0;
    if (m_lapisMinimum < 0) m_lapisMinimum = 0;
    ImGui::Text("LeagueBar: %s", m_leagueReady ? "hooked; auto-close" : "trace-only");
    ImGui::Text("Rewards: multichest=%s bundle=%s", m_multichestReady ? "auto-open/close" : "trace-only",
        m_bundleReady ? "auto-dismiss" : "trace-only");
    ImGui::Text("Fields: settings=%s surrender=%s confirm=%s result delegate=%s",
        m_settingsOffset >= 0 ? "ok" : "missing", m_surrenderOffset >= 0 ? "ok" : "missing",
        m_leftButtonOffset >= 0 ? "ok" : "missing", m_onButtonActionOffset >= 0 ? "ok" : "fallback");
    if (ImGui::Button("Start automation")) {
        const bool modeReady = m_mode == 1 ? (m_autoBattleReady && m_lapisGateReady && m_multichestReady) :
            (m_mode == 2 && m_derankReady);
        if (modeReady && m_observerResolved && m_playHooked && m_advanceHooked && m_coordinator.Start()) {
            m_nextActionAt = GetTickCount64() + static_cast<unsigned long long>(m_delayMs);
            strncpy_s(m_status, "started; play is scheduled", _TRUNCATE);
            actiontrace::Push("automation", "started mode=%d delay=%d", m_mode, m_delayMs);
        }
        else if (m_mode == 0) strncpy_s(m_status, "select an automation mode", _TRUNCATE);
        else if (m_mode == 1 && !m_autoBattleReady) strncpy_s(m_status, "Auto Rank runtime methods unresolved", _TRUNCATE);
        else if (m_mode == 1 && !m_lapisGateReady) strncpy_s(m_status, "Auto Rank Lapis gate unresolved", _TRUNCATE);
        else if (m_mode == 1 && !m_multichestReady) strncpy_s(m_status, "Auto Rank reward automation unresolved", _TRUNCATE);
        else if (!m_observerResolved) strncpy_s(m_status, "result observer unresolved", _TRUNCATE);
        else if (!m_playHooked) strncpy_s(m_status, "play observer hook unavailable", _TRUNCATE);
        else if (!m_advanceHooked) strncpy_s(m_status, "result advance action unresolved", _TRUNCATE);
        else if (m_mode == 2 && !m_derankReady) strncpy_s(m_status, "Derank field layout unresolved", _TRUNCATE);
        else strncpy_s(m_status, "automation is already running", _TRUNCATE);
    }
    ImGui::SameLine();
    if (ImGui::Button("Stop")) {
        m_coordinator.Stop();
        m_autoBattleArmed = false;
        m_autoBattleInitWaitLogged = false;
        m_nextActionAt = 0;
        m_multichestOpenAt = 0;
        m_multichestCloseAt = 0;
        m_multichestWindow = nullptr;
        m_waitingMultichest = false;
        strncpy_s(m_status, "stopped", _TRUNCATE);
        actiontrace::Push("automation", "stopped by user");
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
    m_autoBattleController = nullptr;
    m_autoBattleArmed = false;
    m_autoBattleInitWaitLogged = false;
    m_rewardsClaimObserved = false;
    m_idleChestPresenter = nullptr;
    m_idleChestPreclaimInvoked = false;
    m_idleChestPreclaimAt = 0;
    m_rewardClaimPresenter = nullptr;
    m_claimRewardWindow = nullptr;
    m_rewardClaimActionInvoked = false;
    m_rewardClaimAt = 0;
    m_rewardClaimCloseAt = 0;
    m_claimRewardCloseAt = 0;
    m_rewardsClaimAt = 0;
    m_rewardSettlementDeadline = 0;
    m_nextActionAt = GetTickCount64() + static_cast<unsigned long long>(m_delayMs);
    strncpy_s(m_status, "result window observed; advance scheduled", _TRUNCATE);
    actiontrace::Push("automation", "result window shown; advance scheduled");
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
    actiontrace::Push("automation", "LeagueBar shown; close scheduled");
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
    if (!CallResultActionSafe(reinterpret_cast<ResultActionFn>(s_originalLeagueClose), presenter, "LeagueBarWindowPresenter.OnClose")) {
        strncpy_s(m_status, "LeagueBar close skipped after safety check", _TRUNCATE);
        return;
    }
    // LeagueBar is the final reward surface for both modes.  Complete the
    // collecting state here instead of waiting for a claim observer that is
    // absent on some battle-result variants; OnUpdate then advances Cooldown
    // to StartingBattle and the existing Play hook starts the next loop.
    if (m_coordinator.State() == AutomationState::CollectingReward &&
        !m_waitingMultichest && !m_bundleWindow) {
        m_coordinator.OnEvent(AutomationEvent::RewardCollected);
        m_rewardsClaimObserved = false;
        m_rewardsClaimAt = 0;
        m_rewardSettlementDeadline = 0;
    }
    m_nextActionAt = GetTickCount64() + static_cast<unsigned long long>(m_delayMs);
    strncpy_s(m_status, "LeagueBar closed; next loop armed", _TRUNCATE);
    actiontrace::Push("automation", "LeagueBar closed");
}

bool AutomationFeature::ReadMultichestSnapshot(MultichestSnapshot* snapshot) const {
    if (!snapshot || !m_multichestWindow) return false;
    void* openCardsButton = nullptr;
    bool ok = ReadObjectInt32(m_multichestWindow, m_multichestRewardCountOffset, &snapshot->rewardCount) &&
        ReadObjectInt32(m_multichestWindow, m_multichestFreeCountOffset, &snapshot->freeCount) &&
        ReadObjectInt32(m_multichestWindow, m_multichestActualCountOffset, &snapshot->actualCount) &&
        ReadObjectInt32(m_multichestWindow, m_multichestCurrentCountOffset, &snapshot->currentCount) &&
        ReadObjectBool(m_multichestWindow, m_multichestOpenAllActiveOffset, &snapshot->openAllActive) &&
        ReadObjectBool(m_multichestWindow, m_multichestPressedOpenAllOffset, &snapshot->pressedOpenAll) &&
        ReadObjectBool(m_multichestWindow, m_multichestCardsAppearAnimDoneOffset, &snapshot->cardsAppearAnimDone) &&
        ReadObjectBool(m_multichestWindow, m_multichestOpenAllLockOffset, &snapshot->openAllLock) &&
        ReadObjectBool(m_multichestWindow, m_multichestButtonsActiveOffset, &snapshot->buttonsActive) &&
        ReadObjectPointer(m_multichestWindow, m_multichestButtonOffset, &openCardsButton);
    snapshot->openCardsButtonPresent = openCardsButton != nullptr;
    snapshot->openCardsButton = openCardsButton;
    return ok;
}

void AutomationFeature::OnMultichestShown(void* self) {
    if (!enabled || !m_multichestReady || m_mode == 0 || !self) return;
    const AutomationState state = m_coordinator.State();
    if (state == AutomationState::Idle || state == AutomationState::Done ||
        state == AutomationState::Error) return;
    if (m_mode == 1 && m_lapisGateReady && !HasLapisBalance(m_lapisResourceType, m_lapisMinimum)) {
        m_coordinator.Stop();
        strncpy_s(m_status, "Auto Rank stopped: no Lapis for reward flow", _TRUNCATE);
        return;
    }
    if (m_multichestWindow == self && m_waitingMultichest) return;
    m_multichestWindow = self;
    m_multichestRuntime.OnShown(self);
    m_waitingMultichest = true;
    // Do not call OnOpenAll from OnEnable/Start: those callbacks can run
    // before the pooled window has finished binding its reward controller.
    // Defer one tick so all callbacks collapse to a single attempt.
    m_multichestOpenAt = GetTickCount64() + 250ULL;
    m_multichestOpenDeadline = GetTickCount64() + 15000ULL;
    m_multichestCloseAt = 0;
    m_multichestRetryCount = 0;
    m_multichestSeedClickInvoked = false;
    strncpy_s(m_status, "multichest shown; Open All scheduled", _TRUNCATE);
    actiontrace::Push("automation", "multichest shown; Open All scheduled");
}

void AutomationFeature::OnMultichestHidden() {
    const bool notifyReward = m_multichestRuntime.OnHidden();
    m_multichestWindow = nullptr;
    m_multichestOpenAt = 0;
    m_multichestOpenDeadline = 0;
    m_multichestCloseAt = 0;
    m_waitingMultichest = false;
    if (notifyReward && enabled && m_coordinator.State() == AutomationState::CollectingReward)
        m_coordinator.OnEvent(AutomationEvent::RewardCollected);
    if (enabled) strncpy_s(m_status, "multichest closed; next loop armed", _TRUNCATE);
    if (enabled) actiontrace::Push("automation", "multichest hidden; reward collected=%d", notifyReward ? 1 : 0);
}

void AutomationFeature::OnMultichestTick() {
    if (!enabled || !m_waitingMultichest || !m_multichestWindow) return;
    const unsigned long long now = GetTickCount64();
    if (m_multichestOpenAt == 0 || now < m_multichestOpenAt) return;
    MultichestSnapshot snapshot{};
    if (!ReadMultichestSnapshot(&snapshot)) {
        m_multichestOpenAt = now + 250ULL;
        ++m_multichestRetryCount;
        return;
    }
    // The pooled reward window needs one ordinary card click before it
    // exposes Open All.  Dispatch that transition once per window.
    if (!m_multichestSeedClickInvoked && snapshot.openCardsButtonPresent &&
        snapshot.openCardsButton && snapshot.rewardCount > 0 &&
        snapshot.actualCount > 0 && snapshot.cardsAppearAnimDone &&
        !snapshot.openAllActive && s_originalMultichestButtonClick) {
        __try {
            s_originalMultichestButtonClick(snapshot.openCardsButton, nullptr);
            m_multichestSeedClickInvoked = true;
            m_multichestOpenAt = now + 350ULL;
            strncpy_s(m_status, "multichest first reward opened; Open All scheduled", _TRUNCATE);
            actiontrace::Push("automation", "multichest first reward click dispatched");
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            m_multichestOpenAt = now + 500ULL;
        }
        return;
    }
    MultichestAction action = m_multichestRuntime.Decide(snapshot);
    LOG("[AUTOMATION] multichest tick window=%p phase=%d reward=%d free=%d actual=%d current=%d openActive=%d pressed=%d anim=%d lock=%d button=%d buttons=%d retry=%d time=%llu",
        m_multichestWindow, static_cast<int>(m_multichestRuntime.Phase()), snapshot.rewardCount,
        snapshot.freeCount, snapshot.actualCount, snapshot.currentCount, snapshot.openAllActive ? 1 : 0,
        snapshot.pressedOpenAll ? 1 : 0, snapshot.cardsAppearAnimDone ? 1 : 0,
        snapshot.openAllLock ? 1 : 0, snapshot.openCardsButtonPresent ? 1 : 0,
        snapshot.buttonsActive ? 1 : 0, m_multichestRetryCount, now);
    if (action == MultichestAction::InvokeOpenAll) {
        const MultichestSnapshot before = snapshot;
        CallResultActionSafe(reinterpret_cast<ResultActionFn>(s_originalMultichestOpenAll),
            m_multichestWindow, "MultichestWindow.OnOpenAll");
        MultichestSnapshot after{};
        if (!ReadMultichestSnapshot(&after)) after = before;
        m_multichestRuntime.OnOpenAllReturned(before, after);
        ++m_multichestRetryCount;
        m_multichestOpenAt = now + 250ULL;
        LOG("[AUTOMATION] multichest OpenAll method=%p before(current=%d active=%d pressed=%d) after(current=%d active=%d pressed=%d) confirmed=%d",
            s_originalMultichestOpenAll, before.currentCount, before.openAllActive ? 1 : 0,
            before.pressedOpenAll ? 1 : 0, after.currentCount, after.openAllActive ? 1 : 0,
            after.pressedOpenAll ? 1 : 0, m_multichestRuntime.OpenAllInvoked() ? 1 : 0);
        strncpy_s(m_status, m_multichestRuntime.OpenAllInvoked()
            ? "multichest Open All confirmed; settling" : "multichest Open All unconfirmed; retrying", _TRUNCATE);
        actiontrace::Push("automation", "OpenAll invoked retry=%d confirmed=%d",
            m_multichestRetryCount, m_multichestRuntime.OpenAllInvoked() ? 1 : 0);
        return;
    }
    if (m_rewardsClaimObserved && m_multichestRuntime.OpenAllInvoked())
        action = MultichestAction::RequestClose;
    if (action != MultichestAction::RequestClose) {
        if (now >= m_multichestOpenDeadline) {
            m_waitingMultichest = false;
            m_multichestOpenAt = 0;
            strncpy_s(m_status, "multichest state timeout; manual reward close required", _TRUNCATE);
            actiontrace::Push("err", "multichest timeout; manual reward close required");
            return;
        }
        m_multichestOpenAt = now + 250ULL;
        ++m_multichestRetryCount;
        return;
    }
    void* window = m_multichestWindow;
    uintptr_t moduleBegin = 0;
    uintptr_t moduleEnd = 0;
    void* delegate = nullptr;
    void* invokeImpl = nullptr;
    MultichestDelegateGuardResult guardResult = MultichestDelegateGuardResult::InvalidInput;
    if (GetLoadedModuleImageRange("GameAssembly.dll", &moduleBegin, &moduleEnd)) {
        guardResult = SanitizeMultichestBeforeHideAction(window,
            m_multichestBeforeHideActionOffset, moduleBegin, moduleEnd, &delegate, &invokeImpl);
    }
    LOG("[AUTOMATION] multichest beforeHide guard=%s field=0x%X delegate=%p invoke=%p module=%p-%p",
        MultichestDelegateGuardResultName(guardResult), m_multichestBeforeHideActionOffset,
        delegate, invokeImpl, reinterpret_cast<void*>(moduleBegin), reinterpret_cast<void*>(moduleEnd));
    if (guardResult == MultichestDelegateGuardResult::InvalidInput ||
        guardResult == MultichestDelegateGuardResult::FieldUnreadable ||
        guardResult == MultichestDelegateGuardResult::ClearFailed) {
        m_multichestOpenAt = 0;
        m_waitingMultichest = false;
        strncpy_s(m_status, "multichest callback unsafe; manual reward close required", _TRUNCATE);
        return;
    }
    m_multichestOpenAt = 0;
    ButtonFn close = s_originalMultichestCloseWindow ? s_originalMultichestCloseWindow : s_originalMultichestCloseAction;
    const char* closeLabel = s_originalMultichestCloseWindow ? "MultichestWindow.CloseWindow" : "MultichestWindow.OnCloseAction";
    if (!CallResultActionSafe(reinterpret_cast<ResultActionFn>(close), window, closeLabel)) {
        m_waitingMultichest = false;
        strncpy_s(m_status, "multichest close skipped after safety check", _TRUNCATE);
        return;
    }
    m_multichestRuntime.OnCloseRequested();
    // WindowHidden normally clears this.  Keep the guard set until that hook
    // fires so the next fight cannot overlap the card animation.
    strncpy_s(m_status, "multichest close requested", _TRUNCATE);
    actiontrace::Push("automation", "multichest close requested");
}

void AutomationFeature::OnBundleShown(void* self) {
    if (!enabled || !m_bundleReady || m_mode == 0 || !self) return;
    const AutomationState state = m_coordinator.State();
    if (state == AutomationState::Idle || state == AutomationState::Done ||
        state == AutomationState::Error) return;
    m_bundleWindow = self;
    m_bundleCloseAt = GetTickCount64() + static_cast<unsigned long long>(m_delayMs);
    strncpy_s(m_status, "bundle popup shown; dismiss scheduled", _TRUNCATE);
    actiontrace::Push("automation", "bundle popup shown; dismiss scheduled");
}

void AutomationFeature::OnPlayButtonUpdate(void* self) {
    if (!enabled || m_mode == 0 || !self) return;
    m_playButton = self;
    if (m_coordinator.State() != AutomationState::StartingBattle) return;
    if (GetTickCount64() < m_nextActionAt || !s_originalPlayClick) return;
    if (m_mode == 1 && m_lapisGateReady && !HasLapisBalance(m_lapisResourceType, m_lapisMinimum)) {
        m_coordinator.Stop();
        strncpy_s(m_status, "Auto Rank stopped before Play: no Lapis", _TRUNCATE);
        return;
    }
    m_nextActionAt = 0;
    s_originalPlayClick(self, nullptr);
    m_coordinator.OnEvent(AutomationEvent::BattleStarted);
    strncpy_s(m_status, "Play invoked; battle running", _TRUNCATE);
    actiontrace::Push("automation", "Play invoked");
}

void AutomationFeature::OnResultWindowUpdate(void* self) {
    if (!enabled || m_mode == 0 || m_coordinator.State() != AutomationState::AwaitingResult) return;
    m_resultWindow = self;
    if (GetTickCount64() < m_nextActionAt) return;
    m_nextActionAt = 0;
    // Match battle_cheat's order: invoke the moving _onButtonAction delegate,
    // then use HandlePlayNextButton.  HideWindow is intentionally not called
    // from Update; it can destroy the result object while Unity is still
    // processing its banner and was the observed crash window.
    bool advanced = false;
    if (m_mode == 1 || m_mode == 2) advanced = InvokeMovingButtonAction(self);
    if (!advanced && s_originalPlayNext)
        advanced = CallResultActionSafe(s_originalPlayNext, self, "MatchCompletedWindow.HandlePlayNextButton");
    if (!advanced && m_mode != 2 && s_originalHide)
        advanced = CallResultActionSafe(s_originalHide, self, "MatchCompletedWindow.HideWindow");
    if (!advanced) {
        m_nextActionAt = GetTickCount64() + 1000ULL;
        strncpy_s(m_status, "result action deferred; no safe action", _TRUNCATE);
        return;
    }
    strncpy_s(m_status, "result advanced; waiting for banner phase", _TRUNCATE);
    m_coordinator.OnEvent(AutomationEvent::RewardCollected);
    m_nextActionAt = 0;
    m_rewardSettlementDeadline = GetTickCount64() + 30000ULL;
    strncpy_s(m_status, "result advanced; waiting for reward settlement", _TRUNCATE);
    actiontrace::Push("automation", "result advanced; waiting reward settlement");
}

void AutomationFeature::OnResultPhaseContinued() {
    if (!enabled || m_coordinator.State() != AutomationState::AwaitingResult) return;
    m_nextActionAt = GetTickCount64() + static_cast<unsigned long long>(m_delayMs);
    strncpy_s(m_status, "result phase continued; advance rescheduled", _TRUNCATE);
}

void AutomationFeature::OnAutoBattleControllerResolved(void* controller) {
    if (controller) m_autoBattleController = controller;
}

void AutomationFeature::OnAutoBattleControllerStarted(void* controller) {
    if (!controller) return;
    m_autoBattleController = controller;
    m_autoBattleControllerStarted = true;
    if (enabled && m_mode == 1 && CanArmAutoBattleAtBattlefieldStart(m_coordinator.State())) {
        m_autoBattleArmed = m_autoBattleReady;
        m_autoBattleAt = GetTickCount64();
    }
    LOG("[AUTOMATION] AutoBattleController.Start controller=%p time=%llu", controller, GetTickCount64());
}

void AutomationFeature::OnAutoBattleInactive(void* controller) {
    if (controller && controller != m_autoBattleController) return;
    m_autoBattleControllerStarted = false;
    m_autoBattleClickInvoked = false;
    m_autoBattleArmed = false;
    LOG("[AUTOMATION] AutoBattleController.SetAutoBattleInactive controller=%p time=%llu", controller, GetTickCount64());
}

void AutomationFeature::OnRewardsClaimed() {
    LOG("[AUTOMATION] claim rewards observer fired");
    m_rewardsClaimObserved = true;
    m_rewardsClaimAt = GetTickCount64();
    if (enabled && m_mode == 1)
        strncpy_s(m_status, "rewards claimed; waiting for Multichest/LeagueBar", _TRUNCATE);
    if (enabled) actiontrace::Push("automation", "rewards claimed (ClaimRewardsOnWin)");
}

void AutomationFeature::OnIdleChestShown(void* self) {
    if (!enabled || m_mode != 1 || !self ||
        m_coordinator.State() != AutomationState::CollectingReward) return;
    m_idleChestPresenter = self;
    m_idleChestPreclaimInvoked = false;
    m_idleChestPreclaimAt = GetTickCount64() + 250ULL;
    strncpy_s(m_status, "pre-reward chest shown; claim scheduled", _TRUNCATE);
    actiontrace::Push("automation", "idle chest shown; preclaim scheduled");
}

void AutomationFeature::OnIdleChestPreclaim(void* self) {
    if (!enabled || m_mode != 1 || !self ||
        m_coordinator.State() != AutomationState::CollectingReward) return;
    m_idleChestPresenter = self;
    m_idleChestPreclaimInvoked = true;
    m_rewardsClaimObserved = true;
    m_rewardsClaimAt = GetTickCount64();
    LOG("[AUTOMATION] idle chest preclaim invoked presenter=%p time=%llu", self, m_rewardsClaimAt);
    strncpy_s(m_status, "pre-reward chest claimed; waiting for reward window", _TRUNCATE);
    actiontrace::Push("automation", "idle chest preclaim invoked");
}

void AutomationFeature::OnRewardClaimShown(void* self) {
    if (!enabled || m_mode != 1 || !self ||
        m_coordinator.State() != AutomationState::CollectingReward) return;
    m_rewardClaimPresenter = self;
    m_rewardClaimActionInvoked = false;
    m_rewardClaimAt = GetTickCount64() + 250ULL;
    m_rewardClaimCloseAt = 0;
    m_rewardsClaimObserved = true;
    m_rewardsClaimAt = GetTickCount64();
    strncpy_s(m_status, "reward claim section shown; claim scheduled", _TRUNCATE);
    actiontrace::Push("automation", "reward claim section shown");
}

void AutomationFeature::OnClaimRewardShown(void* self) {
    if (!enabled || m_mode != 1 || !self ||
        m_coordinator.State() != AutomationState::CollectingReward) return;
    m_claimRewardWindow = self;
    m_claimRewardCloseAt = GetTickCount64() + 250ULL;
    m_rewardsClaimObserved = true;
    m_rewardsClaimAt = GetTickCount64();
    strncpy_s(m_status, "claim reward window shown; close scheduled", _TRUNCATE);
    actiontrace::Push("automation", "claim reward window shown");
}

void AutomationFeature::OnBattlefieldStart(void* self) {
    if (!enabled || m_mode == 0 || !CanArmAutoBattleAtBattlefieldStart(m_coordinator.State())) return;
    if (m_mode == 1) {
        m_autoBattleWindow = self;
        void* controller = s_originalAutoBattleGetter ? s_originalAutoBattleGetter(self, nullptr) : nullptr;
        const bool startObserved = m_autoBattleControllerStarted &&
            m_autoBattleController != nullptr && m_autoBattleController == controller;
        m_autoBattleController = controller;
        m_autoBattleArmed = m_autoBattleReady;
        m_autoBattleControllerStarted = HasAutoBattleStartBarrier(
            m_autoBattleStartHooked, startObserved, controller != nullptr);
        m_autoBattleClickInvoked = false;
        m_autoBattleRetryCount = 0;
        m_autoBattleInitWaitLogged = false;
        m_autoBattleAt = GetTickCount64() + static_cast<unsigned long long>(m_delayMs);
        strncpy_s(m_status, m_autoBattleController ? "battle loaded; autoplay scheduled" : "battle loaded; awaiting autoplay controller", _TRUNCATE);
        return;
    }
    if (!m_derankReady) return;
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


