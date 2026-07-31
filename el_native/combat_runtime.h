#pragma once

#include <cstdint>
#include <string>
#include <mutex>
#include <map>
#include <vector>

enum class ArmySide { Unknown = 0, Player = 1, Enemy = 2 };

// The game enum uses 0 for NoSide, 1 for Left and 2 for Right.  A raw zero
// is intentionally kept Unknown; treating it as the enemy side makes every
// side-gated feature modify units before their army has been assigned.
ArmySide ArmySideFromRaw(int raw, int localRaw = 1);
int RuntimeLocalArmySide();
void SetRuntimeLocalArmySide(int raw);

struct UnitSnapshot {
    std::uintptr_t pointer = 0;
    std::uint32_t generation = 0;
    int monsterUid = 0;
    std::string name;
    int grade = 0;
    ArmySide side = ArmySide::Unknown;
    float hp = 0.0f;
    float mana = 0.0f;
    std::uintptr_t target = 0;
    bool alive = false;
    std::uint32_t callCount = 0;
};

struct StatComparison {
    std::string character;
    std::string stat;
    float baseline = 0.0f;
    float applied = 0.0f;
    std::uint32_t calls = 0;
    float Delta() const { return applied - baseline; }
};

class CombatRuntime {
public:
    void BeginSession(std::uint64_t sessionId);
    void EndSession();
    void ObserveUnit(const UnitSnapshot& unit);
    void RemoveUnit(std::uintptr_t pointer);
    void DisposeUnit(std::uintptr_t pointer);
    std::vector<UnitSnapshot> Snapshot() const;
    std::uint32_t GenerationFor(std::uintptr_t pointer) const;
    std::uint64_t SessionId() const { return m_sessionId; }
    bool Active() const { return m_active; }

private:
    std::uint64_t m_sessionId = 0;
    bool m_active = false;
    std::vector<UnitSnapshot> m_units;
    std::map<std::uintptr_t, std::uint32_t> m_generationHistory;
    mutable std::mutex m_mutex;
};

struct CombatConfig {
    float attackSpeedMultiplier = 1.0f;
    int multiHit = 1;
    bool freezeEnemies = false;
    bool dumbEnemies = false;
};

float ApplyTurnInterval(float originalInterval, ArmySide side, const CombatConfig& config);
int MultiHitCount(ArmySide side, const CombatConfig& config);
typedef void (*HitCallback)(void* context);
int ExecuteMultiHit(ArmySide side, const CombatConfig& config, HitCallback callback, void* context);
bool IsTimeFrozen(ArmySide side, bool originalValue, const CombatConfig& config);
bool AllowAttackActivation(ArmySide side, bool originalValue, const CombatConfig& config);

CombatRuntime& GlobalCombatRuntime();
