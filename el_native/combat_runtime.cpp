#include "combat_runtime.h"

#include <algorithm>

namespace {
bool SameIdentity(const UnitSnapshot& left, const UnitSnapshot& right) {
    return left.monsterUid == right.monsterUid && left.name == right.name &&
        left.grade == right.grade && left.side == right.side;
}

int g_runtimeLocalArmySide = 1;

}

ArmySide ArmySideFromRaw(int raw, int localRaw) {
    if (raw <= 0 || (localRaw != 1 && localRaw != 2)) return ArmySide::Unknown;
    if (raw == localRaw) return ArmySide::Player;
    if (raw == 1 || raw == 2) return ArmySide::Enemy;
    return ArmySide::Unknown;
}

int RuntimeLocalArmySide() { return g_runtimeLocalArmySide; }

void SetRuntimeLocalArmySide(int raw) {
    g_runtimeLocalArmySide = raw == 2 ? 2 : 1;
}

void CombatRuntime::BeginSession(std::uint64_t sessionId) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_sessionId = sessionId;
    m_active = true;
    m_units.clear();
    m_generationHistory.clear();
}

void CombatRuntime::EndSession() {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_active = false;
    m_units.clear();
    m_generationHistory.clear();
}

void CombatRuntime::ObserveUnit(const UnitSnapshot& unit) {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!unit.pointer) return;
    auto it = std::find_if(m_units.begin(), m_units.end(), [&](const UnitSnapshot& item) {
        return item.pointer == unit.pointer;
    });
    if (it == m_units.end()) {
        UnitSnapshot copy = unit;
        copy.generation = ++m_generationHistory[unit.pointer];
        m_units.push_back(copy);
        return;
    }

    UnitSnapshot copy = unit;
    if (SameIdentity(*it, unit)) {
        copy.generation = it->generation;
        copy.callCount = it->callCount + 1;
    } else {
        copy.generation = ++m_generationHistory[unit.pointer];
        copy.callCount = 1;
    }
    *it = copy;
}

void CombatRuntime::RemoveUnit(std::uintptr_t pointer) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_units.erase(std::remove_if(m_units.begin(), m_units.end(), [&](const UnitSnapshot& item) {
        return item.pointer == pointer;
    }), m_units.end());
}

void CombatRuntime::DisposeUnit(std::uintptr_t pointer) { RemoveUnit(pointer); }

std::vector<UnitSnapshot> CombatRuntime::Snapshot() const { std::lock_guard<std::mutex> lock(m_mutex); return m_units; }

std::uint32_t CombatRuntime::GenerationFor(std::uintptr_t pointer) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    for (const auto& unit : m_units) {
        if (unit.pointer == pointer) return unit.generation;
    }
    auto it = m_generationHistory.find(pointer);
    return it == m_generationHistory.end() ? 0 : it->second;
}

float ApplyTurnInterval(float originalInterval, ArmySide side, const CombatConfig& config) {
    if (side != ArmySide::Player || config.attackSpeedMultiplier <= 0.0f)
        return originalInterval;
    return originalInterval / config.attackSpeedMultiplier;
}

int MultiHitCount(ArmySide side, const CombatConfig& config) {
    if (side != ArmySide::Player) return 1;
    return std::max(1, std::min(5, config.multiHit));
}

int ExecuteMultiHit(ArmySide side, const CombatConfig& config, HitCallback callback, void* context) {
    if (!callback) return 0;
    const int hits = MultiHitCount(side, config);
    for (int i = 0; i < hits; ++i) callback(context);
    return hits;
}

bool IsTimeFrozen(ArmySide side, bool originalValue, const CombatConfig& config) {
    return side == ArmySide::Enemy && config.freezeEnemies ? true : originalValue;
}

bool AllowAttackActivation(ArmySide side, bool originalValue, const CombatConfig& config) {
    return side == ArmySide::Enemy && config.dumbEnemies ? false : originalValue;
}

CombatRuntime& GlobalCombatRuntime() {
    static CombatRuntime runtime;
    return runtime;
}
