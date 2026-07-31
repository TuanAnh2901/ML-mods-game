#include "automation.h"

#include <algorithm>

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
