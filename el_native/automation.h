#pragma once

#include <cstddef>
#include <cstdint>

enum class AutomationMode { Off, AutoBattle, Derank };
enum class AutomationState {
    Idle, StartingBattle, InBattle, AwaitingResult,
    CollectingReward, Cooldown, Done, Error
};
enum class AutomationEvent {
    BattleStarted, BattleFinished, RewardCollected,
    CooldownElapsed, Failed, Stop
};

enum class AutoBattleTriggerDecision {
    WaitForController,
    WaitForInitialization,
    Invoke,
    Complete
};

AutoBattleTriggerDecision DecideAutoBattleTrigger(
    bool hasController, bool hasCore, bool unlocked, bool active);

bool CanArmAutoBattleAtBattlefieldStart(AutomationState state);

bool IsAutoBattleRuntimeReady(
    bool getterResolved, bool clickResolved, bool activeResolved, bool offsetsResolved);

bool HasAutoBattleStartBarrier(
    bool startObserverHooked, bool startObserved, bool controllerResolvedAfterBattlefieldStart);

enum class RewardSettlementDecision { Wait, CompleteAfterConfirmedClaim, ManualClaimRequired };

RewardSettlementDecision DecideRewardSettlement(
    bool multichestVisible, bool bundleVisible, bool leagueVisible,
    bool claimGraceElapsed, bool deadlineElapsed);

// The CollectingReward state force-closes the multichest once the claim has
// been observed and OpenAll was invoked. Closing while the card animation
// still holds _openAllLock crashed the game (SEH 0xC0000005), so the forced
// close must wait for the lock to clear.
bool ShouldForceCloseMultichest(
    bool rewardsClaimObserved, bool openAllInvoked, bool openAllLock);

enum class PlayInvokeDecision { WaitForHook, InvokeViaWatchdog };

PlayInvokeDecision DecidePlayInvoke(
    bool inStartingBattle, bool playButtonCached, bool clickMethodResolved, bool deadlineElapsed);

enum class MultichestPhase {
    Hidden, Seen, WaitingForInitialization, WaitingForOpenAll,
    OpenAllInvoked, WaitingForRewardSettlement, CloseRequested
};

enum class MultichestAction { Wait, InvokeOpenAll, RequestClose };

struct MultichestSnapshot {
    int32_t rewardCount = 0;
    int32_t freeCount = 0;
    int32_t actualCount = 0;
    int32_t currentCount = 0;
    bool openAllActive = false;
    bool pressedOpenAll = false;
    bool cardsAppearAnimDone = false;
    bool openAllLock = false;
    bool openCardsButtonPresent = false;
    bool buttonsActive = false;
    void* openCardsButton = nullptr;
};

class MultichestRuntimeState {
public:
    void OnShown(void* window);
    MultichestAction Decide(const MultichestSnapshot& snapshot) const;
    void OnOpenAllReturned(const MultichestSnapshot& before, const MultichestSnapshot& after);
    void OnCloseRequested();
    bool OnHidden();
    MultichestPhase Phase() const { return m_phase; }
    unsigned Generation() const { return m_generation; }
    bool OpenAllInvoked() const { return m_openAllInvoked; }

private:
    void* m_window = nullptr;
    MultichestPhase m_phase = MultichestPhase::Hidden;
    unsigned m_generation = 0;
    bool m_openAllInvoked = false;
    bool m_hiddenNotified = false;
};

class AutomationCoordinator {
public:
    void Configure(AutomationMode mode, int maxLoops);
    bool Start();
    void Stop();
    void OnEvent(AutomationEvent event);
    AutomationMode Mode() const { return m_mode; }
    AutomationState State() const { return m_state; }
    int CompletedLoops() const { return m_completedLoops; }
    int MaxLoops() const { return m_maxLoops; }

private:
    AutomationMode m_mode = AutomationMode::Off;
    AutomationState m_state = AutomationState::Idle;
    // A value of zero follows the battle-cheat convention: keep running until
    // the user presses Stop.
    int m_maxLoops = 1;
    int m_completedLoops = 0;
};
