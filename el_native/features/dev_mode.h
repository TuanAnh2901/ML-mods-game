#pragma once
#include "../feature.h"
#include <cstdint>

struct DevModeFeature : Feature {
    DevModeFeature();
    void Init() override;
    void OnUpdate() override;
    void OnMenu() override;

    // --- Sub-panel states & inputs ---
    // 1. Resources & Economy
    int m_resourceType = 2;       // Default Lapis (2)
    int m_resourceAmount = 10000; // Default 10,000
    int m_customResId = 1;
    int m_customResAmount = 1000;

    // 2. Match
    int m_matchCoins = 100;
    int m_matchHealth = 100;
    int m_matchPlace = 1;
    int m_skipRounds = 1;
    int m_unitId = 1;
    bool m_disableSlotLimit = false;

    // 3. BattlePass
    int m_bpLevel = 50;
    bool m_bpPremium = true;

    // 4. Building & Storage
    int m_buildingLevel = 10;
    bool m_allBuildings = true;

    // 5. Dungeon & Labyrinth
    int m_dungeonFloor = 10;
    int m_artifactId = 1;
    int m_artifactCount = 1;

    // 6. Leagues & Fame
    int m_fameDelta = 500;
    int m_targetLeague = 5;

    // 7. Skips & Quality of Life
    bool m_skipTutorial = false;
    bool m_skipBpAnim = true;
    bool m_skipDungeonAnim = true;
    bool m_skipUnlockWindows = true;
    bool m_skipPushes = true;

    // Status message for UI feedback
    char m_statusMessage[128] = "Ready";

    // Resolved function pointers
    void* m_fnChangeResource = nullptr;
    void* m_fnGetItemModuleInstance = nullptr;
    void* m_fnConvertWinningSide = nullptr;
    void* m_fnApplyBattleResult = nullptr;
    void* m_fnBpSetLevel = nullptr;
    void* m_fnBpSetPremium = nullptr;
    void* m_fnSendBpProgress = nullptr;
};
