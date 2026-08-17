#include "automation.h"

#include <algorithm>

AutoBattleTriggerDecision DecideAutoBattleTrigger(
    bool hasController, bool hasCore, bool unlocked, bool active) {
    if (!hasController) return AutoBattleTriggerDecision::WaitForController;
    if (!hasCore || !unlocked) return AutoBattleTriggerDecision::WaitForInitialization;
    return active ? AutoBattleTriggerDecision::Complete : AutoBattleTriggerDecision::Invoke;
}

bool CanArmAutoBattleAtBattlefieldStart(AutomationState state) {
    return state == AutomationState::StartingBattle || state == AutomationState::InBattle;
}

bool IsAutoBattleRuntimeReady(
    bool getterResolved, bool clickResolved, bool activeResolved, bool offsetsResolved) {
    return getterResolved && clickResolved && activeResolved && offsetsResolved;
}

bool HasAutoBattleStartBarrier(
    bool startObserverHooked, bool startObserved, bool controllerResolvedAfterBattlefieldStart) {
    if (startObserverHooked) return startObserved;
    return controllerResolvedAfterBattlefieldStart;
}

RewardSettlementDecision DecideRewardSettlement(
    bool multichestVisible, bool bundleVisible, bool leagueVisible,
    bool claimGraceElapsed, bool deadlineElapsed) {
    if (multichestVisible || bundleVisible || leagueVisible)
        return RewardSettlementDecision::Wait;
    if (claimGraceElapsed)
        return RewardSettlementDecision::CompleteAfterConfirmedClaim;
    return deadlineElapsed ? RewardSettlementDecision::ManualClaimRequired
                           : RewardSettlementDecision::Wait;
}

PlayInvokeDecision DecidePlayInvoke(
    bool inStartingBattle, bool playButtonCached, bool clickMethodResolved, bool deadlineElapsed) {
    if (!inStartingBattle || !playButtonCached || !clickMethodResolved)
        return PlayInvokeDecision::WaitForHook;
    return deadlineElapsed ? PlayInvokeDecision::InvokeViaWatchdog
                           : PlayInvokeDecision::WaitForHook;
}

void MultichestRuntimeState::OnShown(void* window) {
    if (!window) return;
    if (m_window == window && m_phase != MultichestPhase::Hidden) return;
    m_window = window;
    m_phase = MultichestPhase::WaitingForInitialization;
    ++m_generation;
    m_openAllInvoked = false;
    m_hiddenNotified = false;
}

MultichestAction MultichestRuntimeState::Decide(const MultichestSnapshot& snapshot) const {
    if (m_phase == MultichestPhase::WaitingForInitialization ||
        m_phase == MultichestPhase::WaitingForOpenAll) {
        const bool initialized = snapshot.rewardCount > 0 && snapshot.actualCount > 0 &&
            snapshot.cardsAppearAnimDone && !snapshot.openAllLock &&
            snapshot.openCardsButtonPresent &&
            (snapshot.openAllActive || snapshot.buttonsActive);
        return initialized && !m_openAllInvoked
            ? MultichestAction::InvokeOpenAll : MultichestAction::Wait;
    }
    if (m_phase == MultichestPhase::OpenAllInvoked ||
        m_phase == MultichestPhase::WaitingForRewardSettlement) {
        const bool countSettled = snapshot.actualCount > 0 &&
            snapshot.currentCount >= snapshot.actualCount;
        const bool openAllSettled = snapshot.pressedOpenAll && !snapshot.openAllActive;
        // Closing while the card animation still holds _openAllLock crashed the
        // game (SEH 0xC0000005). Wait for the lock to clear before requesting
        // the close, even when the reward counts already settled.
        return (countSettled || openAllSettled) && !snapshot.openAllLock
            ? MultichestAction::RequestClose : MultichestAction::Wait;
    }
    return MultichestAction::Wait;
}

bool ShouldForceCloseMultichest(
    bool rewardsClaimObserved, bool openAllInvoked, bool openAllLock) {
    return rewardsClaimObserved && openAllInvoked && !openAllLock;
}

void MultichestRuntimeState::OnOpenAllReturned(
    const MultichestSnapshot& before, const MultichestSnapshot& after) {
    const bool transitioned = (!before.pressedOpenAll && after.pressedOpenAll) ||
        (before.openAllActive && !after.openAllActive) ||
        after.currentCount > before.currentCount;
    if (transitioned) {
        m_openAllInvoked = true;
        m_phase = MultichestPhase::WaitingForRewardSettlement;
    } else {
        m_phase = MultichestPhase::WaitingForOpenAll;
    }
}

void MultichestRuntimeState::OnCloseRequested() {
    m_phase = MultichestPhase::CloseRequested;
}

bool MultichestRuntimeState::OnHidden() {
    const bool notify = !m_hiddenNotified && m_generation != 0;
    m_hiddenNotified = true;
    m_window = nullptr;
    m_phase = MultichestPhase::Hidden;
    m_openAllInvoked = false;
    return notify;
}

void AutomationCoordinator::Configure(AutomationMode mode, int maxLoops) {
    m_mode = mode;
    m_maxLoops = std::max(0, maxLoops);
    if (m_state == AutomationState::Idle || m_state == AutomationState::Done ||
        m_state == AutomationState::Error) {
        m_completedLoops = 0;
    }
}

bool AutomationCoordinator::Start() {
    if (m_mode == AutomationMode::Off || m_state != AutomationState::Idle) return false;
    m_completedLoops = 0;
    m_state = AutomationState::StartingBattle;
    return true;
}

void AutomationCoordinator::Stop() {
    m_state = AutomationState::Idle;
    m_mode = AutomationMode::Off;
}

void AutomationCoordinator::OnEvent(AutomationEvent event) {
    if (event == AutomationEvent::Stop) { Stop(); return; }
    if (event == AutomationEvent::Failed) { m_state = AutomationState::Error; return; }
    switch (m_state) {
    case AutomationState::StartingBattle:
        if (event == AutomationEvent::BattleStarted) m_state = AutomationState::InBattle;
        break;
    case AutomationState::InBattle:
        if (event == AutomationEvent::BattleFinished) m_state = AutomationState::AwaitingResult;
        break;
    case AutomationState::AwaitingResult:
        if (event == AutomationEvent::RewardCollected) {
            m_state = AutomationState::CollectingReward;
        }
        break;
    case AutomationState::CollectingReward:
        if (event == AutomationEvent::RewardCollected) {
            ++m_completedLoops;
            m_state = (m_maxLoops > 0 && m_completedLoops >= m_maxLoops)
                ? AutomationState::Done : AutomationState::Cooldown;
        }
        break;
    case AutomationState::Cooldown:
        if (event == AutomationEvent::CooldownElapsed) m_state = AutomationState::StartingBattle;
        break;
    default:
        break;
    }
}
