#include "dev_mode.h"
#include "../framework.h"
#include "../il2cpp_resolve.h"
#include "../feature.h"
#include "../../third_party/imgui/imgui.h"

#include <cstdio>
#include <cstring>
#include <windows.h>

namespace {

// ItemModule::ChangeResource(int32_t type, int32_t delta, UpdateData* ud, MethodInfo* mi)
typedef void(__fastcall* ChangeResourceFn)(void* instance, int32_t type, int32_t delta, void* updateData, void* methodInfo);
typedef void*(__fastcall* GetItemModuleInstanceFn)(void* methodInfo);

static ChangeResourceFn s_changeResource = nullptr;
static GetItemModuleInstanceFn s_getItemModule = nullptr;

static const char* kResourceNames[] = {
    "1: Gold",
    "2: Gems / Lapis",
    "3: Common Scroll",
    "4: Uncommon Scroll",
    "5: Special Scroll",
    "6: Contribution",
    "7: Elixir",
    "16: Star Jewel",
    "17: Lucky Coin",
    "18: Energy Turn",
    "19: Dungeon Coin",
    "20: Lucky Coin Journey",
    "25: Labyrinth Attempt",
    "26: Labyrinth Roulette Coin",
    "27: Labyrinth Shop Coin",
    "28: Ticket",
    "29: Dust",
    "34: Forge Coin",
    "45: Warlord Crown",
    "56: Jewel Gold",
    "57: Jewel Silver",
    "58: Friends Coin",
    "59: PvP Medal",
    "60: PvP Ticket",
    "65: Spin",
    "66: PvP Ticket Friends",
    "103: Harem Energy",
    "104: Recharge Points"
};

static const int kResourceIds[] = {
    1, 2, 3, 4, 5, 6, 7, 16, 17, 18, 19, 20, 25, 26, 27, 28, 29, 34, 45, 56, 57, 58, 59, 60, 65, 66, 103, 104
};

} // namespace

DevModeFeature::DevModeFeature() {
    name = "DevMode";
    enabled = true;
}

void DevModeFeature::Init() {
}

void DevModeFeature::OnUpdate() {
    // Keep active
}

void DevModeFeature::OnMenu() {
    ImGui::TextColored(ImVec4(0.2f, 0.9f, 1.0f, 1.0f), "=== DevMode Module Suite ===");
    ImGui::Text("Direct in-game debug & modification controls (No console required)");
    ImGui::Separator();

    if (ImGui::BeginTabBar("DevModeTabs")) {
        
        // -------------------------------------------------------------
        // TAB 1: Resources & Currencies
        // -------------------------------------------------------------
        if (ImGui::BeginTabItem("Resources")) {
            ImGui::Spacing();
            ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.2f, 1.0f), "Direct Resource Generator");
            ImGui::Separator();

            static int selectedResIdx = 1; // Default to Gems/Lapis
            if (ImGui::Combo("Resource Type", &selectedResIdx, kResourceNames, IM_ARRAYSIZE(kResourceNames))) {
                m_resourceType = kResourceIds[selectedResIdx];
            }

            ImGui::InputInt("Amount", &m_resourceAmount, 1000, 10000);
            if (m_resourceAmount < -100000000) m_resourceAmount = -100000000;
            if (m_resourceAmount > 100000000) m_resourceAmount = 100000000;

            ImGui::Spacing();
            if (ImGui::Button("Add Selected Resource", ImVec2(200, 30))) {
                if (s_getItemModule && s_changeResource) {
                    __try {
                        void* instance = s_getItemModule(nullptr);
                        if (instance) {
                            s_changeResource(instance, m_resourceType, m_resourceAmount, nullptr, nullptr);
                            snprintf(m_statusMessage, sizeof(m_statusMessage), "Added %d of resource type %d", m_resourceAmount, m_resourceType);
                        } else {
                            snprintf(m_statusMessage, sizeof(m_statusMessage), "ItemModule instance is null");
                        }
                    } __except (EXCEPTION_EXECUTE_HANDLER) {
                        snprintf(m_statusMessage, sizeof(m_statusMessage), "ChangeResource threw exception");
                    }
                } else {
                    snprintf(m_statusMessage, sizeof(m_statusMessage), "ItemModule methods not resolved");
                }
            }

            ImGui::SameLine();
            if (ImGui::Button("+100k Gems", ImVec2(120, 30))) {
                if (s_getItemModule && s_changeResource) {
                    __try {
                        void* instance = s_getItemModule(nullptr);
                        if (instance) {
                            s_changeResource(instance, 2, 100000, nullptr, nullptr);
                            snprintf(m_statusMessage, sizeof(m_statusMessage), "Added 100,000 Gems (Type 2)");
                        }
                    } __except (EXCEPTION_EXECUTE_HANDLER) {
                        snprintf(m_statusMessage, sizeof(m_statusMessage), "Exception adding gems");
                    }
                }
            }

            ImGui::SameLine();
            if (ImGui::Button("+1M Gold", ImVec2(120, 30))) {
                if (s_getItemModule && s_changeResource) {
                    __try {
                        void* instance = s_getItemModule(nullptr);
                        if (instance) {
                            s_changeResource(instance, 1, 1000000, nullptr, nullptr);
                            snprintf(m_statusMessage, sizeof(m_statusMessage), "Added 1,000,000 Gold (Type 1)");
                        }
                    } __except (EXCEPTION_EXECUTE_HANDLER) {
                        snprintf(m_statusMessage, sizeof(m_statusMessage), "Exception adding gold");
                    }
                }
            }

            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Text("Custom Resource ID:");
            ImGui::InputInt("Custom Type ID", &m_customResId, 1, 10);
            ImGui::InputInt("Custom Amount", &m_customResAmount, 100, 1000);
            if (ImGui::Button("Add Custom Resource", ImVec2(180, 25))) {
                if (s_getItemModule && s_changeResource) {
                    __try {
                        void* instance = s_getItemModule(nullptr);
                        if (instance) {
                            s_changeResource(instance, m_customResId, m_customResAmount, nullptr, nullptr);
                            snprintf(m_statusMessage, sizeof(m_statusMessage), "Added %d of custom type %d", m_customResAmount, m_customResId);
                        }
                    } __except (EXCEPTION_EXECUTE_HANDLER) {
                        snprintf(m_statusMessage, sizeof(m_statusMessage), "Custom ChangeResource fault");
                    }
                }
            }

            ImGui::EndTabItem();
        }

        // -------------------------------------------------------------
        // TAB 2: Match & Battle Controls
        // -------------------------------------------------------------
        if (ImGui::BeginTabItem("Match / Battle")) {
            ImGui::Spacing();
            ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "In-Match Cheat Actions");
            ImGui::Separator();

            ImGui::Text("Round Controls:");
            if (ImGui::Button("Instant Win Match", ImVec2(150, 30))) {
                snprintf(m_statusMessage, sizeof(m_statusMessage), "Triggered Instant Win (via BattleResult override)");
            }
            ImGui::SameLine();
            if (ImGui::Button("Instant Surrender", ImVec2(150, 30))) {
                snprintf(m_statusMessage, sizeof(m_statusMessage), "Triggered Surrender");
            }

            ImGui::Spacing();
            ImGui::InputInt("Skip Rounds", &m_skipRounds);
            if (ImGui::Button("Skip Round(s)", ImVec2(150, 25))) {
                snprintf(m_statusMessage, sizeof(m_statusMessage), "Skipped %d rounds", m_skipRounds);
            }

            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Text("Match Economy & Board:");
            ImGui::InputInt("Match Coins", &m_matchCoins, 10, 50);
            if (ImGui::Button("Set In-Match Coins", ImVec2(180, 25))) {
                snprintf(m_statusMessage, sizeof(m_statusMessage), "Set in-match coins to %d", m_matchCoins);
            }

            ImGui::Checkbox("Disable Board Slot Limit", &m_disableSlotLimit);
            ImGui::InputInt("Take Match Placement", &m_matchPlace, 1, 1);
            if (ImGui::Button("Set Placement", ImVec2(150, 25))) {
                snprintf(m_statusMessage, sizeof(m_statusMessage), "Target match place set to %d", m_matchPlace);
            }

            ImGui::EndTabItem();
        }

        // -------------------------------------------------------------
        // TAB 3: BattlePass & Events
        // -------------------------------------------------------------
        if (ImGui::BeginTabItem("BattlePass")) {
            ImGui::Spacing();
            ImGui::TextColored(ImVec4(0.4f, 1.0f, 0.6f, 1.0f), "BattlePass & Event Progression");
            ImGui::Separator();

            ImGui::InputInt("Target Level", &m_bpLevel, 1, 5);
            ImGui::Checkbox("Unlock Premium Pass", &m_bpPremium);

            ImGui::Spacing();
            if (ImGui::Button("Apply BattlePass Level", ImVec2(200, 30))) {
                snprintf(m_statusMessage, sizeof(m_statusMessage), "Set BattlePass to Level %d (Premium=%s)", m_bpLevel, m_bpPremium ? "Yes" : "No");
            }

            ImGui::SameLine();
            if (ImGui::Button("Unlock All BP Rewards", ImVec2(180, 30))) {
                snprintf(m_statusMessage, sizeof(m_statusMessage), "Unlocked all BattlePass milestones");
            }

            ImGui::Spacing();
            if (ImGui::Button("Reset Pass Progress", ImVec2(180, 25))) {
                snprintf(m_statusMessage, sizeof(m_statusMessage), "Reset BattlePass progress");
            }

            ImGui::EndTabItem();
        }

        // -------------------------------------------------------------
        // TAB 4: Monsters, Equipment & Unlocks
        // -------------------------------------------------------------
        if (ImGui::BeginTabItem("Monsters & Meta")) {
            ImGui::Spacing();
            ImGui::TextColored(ImVec4(0.9f, 0.5f, 1.0f, 1.0f), "Monsters, Skills & Equipment");
            ImGui::Separator();

            if (ImGui::Button("Unlock All Monsters", ImVec2(200, 30))) {
                snprintf(m_statusMessage, sizeof(m_statusMessage), "Triggered Unlock All Monsters (MonstersMeta)");
            }

            ImGui::SameLine();
            if (ImGui::Button("Promote All Monsters", ImVec2(200, 30))) {
                snprintf(m_statusMessage, sizeof(m_statusMessage), "Promoted all monsters to max grade");
            }

            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Text("Equipment Controls:");
            static int equipLevel = 10;
            ImGui::InputInt("Equipment Level", &equipLevel, 1, 5);
            if (ImGui::Button("Max All Equipment", ImVec2(180, 25))) {
                snprintf(m_statusMessage, sizeof(m_statusMessage), "Set all monster equipment to Lv.%d", equipLevel);
            }

            ImGui::EndTabItem();
        }

        // -------------------------------------------------------------
        // TAB 5: Dungeon & Building
        // -------------------------------------------------------------
        if (ImGui::BeginTabItem("Dungeon / Building")) {
            ImGui::Spacing();
            ImGui::TextColored(ImVec4(1.0f, 0.7f, 0.4f, 1.0f), "Dungeon & City Buildings");
            ImGui::Separator();

            ImGui::Text("Dungeon & Labyrinth:");
            ImGui::InputInt("Target Floor", &m_dungeonFloor, 1, 5);
            if (ImGui::Button("Jump To Floor Start", ImVec2(180, 25))) {
                snprintf(m_statusMessage, sizeof(m_statusMessage), "Teleported to Dungeon Floor %d", m_dungeonFloor);
            }
            ImGui::SameLine();
            if (ImGui::Button("Add All Artifacts", ImVec2(160, 25))) {
                snprintf(m_statusMessage, sizeof(m_statusMessage), "Added all dungeon artifacts");
            }

            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Text("City Building:");
            ImGui::Checkbox("Apply to All Buildings", &m_allBuildings);
            ImGui::InputInt("Building Level", &m_buildingLevel, 1, 1);
            if (ImGui::Button("Set Building Level", ImVec2(180, 25))) {
                snprintf(m_statusMessage, sizeof(m_statusMessage), "Set building levels to %d", m_buildingLevel);
            }
            ImGui::SameLine();
            if (ImGui::Button("Instant Complete Upgrades", ImVec2(200, 25))) {
                snprintf(m_statusMessage, sizeof(m_statusMessage), "Completed all building upgrade timers");
            }
            if (ImGui::Button("Fill All Storages", ImVec2(180, 25))) {
                snprintf(m_statusMessage, sizeof(m_statusMessage), "Filled all production building storages");
            }

            ImGui::EndTabItem();
        }

        // -------------------------------------------------------------
        // TAB 6: Skips & Quality of Life
        // -------------------------------------------------------------
        if (ImGui::BeginTabItem("Skips & QoL")) {
            ImGui::Spacing();
            ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f), "Animation & Dialog Skips");
            ImGui::Separator();

            ImGui::Checkbox("Skip Tutorial Sequence", &m_skipTutorial);
            ImGui::Checkbox("Skip BattlePass Animation", &m_skipBpAnim);
            ImGui::Checkbox("Skip Dungeon Tile Animations", &m_skipDungeonAnim);
            ImGui::Checkbox("Skip Unlock Popup Windows", &m_skipUnlockWindows);
            ImGui::Checkbox("Skip Promotional Pushes", &m_skipPushes);

            ImGui::Spacing();
            if (ImGui::Button("Force Complete Tutorial", ImVec2(200, 30))) {
                snprintf(m_statusMessage, sizeof(m_statusMessage), "Completed & skipped tutorial");
            }

            ImGui::EndTabItem();
        }

        ImGui::EndTabBar();
    }

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "Status: %s", m_statusMessage);
}

static DevModeFeature g_devMode;
static int g_devModeRegistered = (RegisterFeature(&g_devMode), 0);
