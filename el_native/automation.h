#pragma once

#include <cstddef>

enum class AutomationMode { Off, AutoBattle, Derank };
enum class AutomationState {
    Idle, StartingBattle, InBattle, AwaitingResult,
    CollectingReward, Cooldown, Done, Error
};
enum class AutomationEvent {
    BattleStarted, BattleFinished, RewardCollected,
    CooldownElapsed, Failed, Stop
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
