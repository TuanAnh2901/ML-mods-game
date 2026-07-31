#include "tracer.h"
#include "../framework.h"
#include "../il2cpp_resolve.h"
#include "../../minhook/include/MinHook.h"
#include "imgui.h"
#include <cstdio>
#include <cstring>

// Runtime StatType enum dumper — built-in Frida replacement.
// Hooks BattleUnit::GetCurrentValue(StatType) and GetStatValue(StatType)
// to capture every unique StatType → helps build the enum mapping.
//
// GetCurrentValue(StatType stat) → float at 0xE4D400 (unique)
// GetStatValue(StatType stat) → float at 0xE4D9A0 (unique)

typedef float(__fastcall* GetStatFloat_t)(void* self, int32_t stat, void* mi);
static GetStatFloat_t Original_GetCurrentValue = nullptr;
static GetStatFloat_t Original_GetStatValue = nullptr;

static bool s_dumpStats = false;

struct StatEntry {
    int32_t type;
    float minVal;
    float maxVal;
    float sumVal;
    int count;
    int lastSide;
};

static StatEntry s_stats[256];
static int s_statCount = 0;
static int s_newStatCount = 0; // updated when new stat discovered
static int s_lastSavedCount = -1;
static int s_frameCounter = 0;

// Save dump file (auto-called every ~5s + on new stat)
static void SaveStatDump() {
    FILE* f = fopen("el_native_stattypes.txt", "w");
    if (!f) return;
    fprintf(f, "# StatType dump — %d unique values (%d total calls)\n", s_statCount, s_newStatCount);
    fprintf(f, "# Type | Count | MinVal | MaxVal | AvgVal | LastSide\n");
    for (int i = 0; i < s_statCount; i++) {
        float avg = s_stats[i].sumVal / s_stats[i].count;
        fprintf(f, "%4d | %6d | %10.2f | %10.2f | %10.2f | side=%d\n",
            s_stats[i].type, s_stats[i].count,
            s_stats[i].minVal, s_stats[i].maxVal, avg,
            s_stats[i].lastSide);
    }
    fclose(f);
    s_lastSavedCount = s_newStatCount;
}

typedef int32_t(__fastcall* GetArmySide_t)(void* self, void* mi);
static GetArmySide_t Resolved_GetArmySide2 = nullptr;

static void RecordStat(int32_t statType, float value, int side) {
    for (int i = 0; i < s_statCount; i++) {
        if (s_stats[i].type == statType) {
            s_stats[i].count++;
            s_stats[i].sumVal += value;
            if (value < s_stats[i].minVal) s_stats[i].minVal = value;
            if (value > s_stats[i].maxVal) s_stats[i].maxVal = value;
            s_stats[i].lastSide = side;
            return;
        }
    }
    if (s_statCount < 256) {
        s_stats[s_statCount].type = statType;
        s_stats[s_statCount].minVal = value;
        s_stats[s_statCount].maxVal = value;
        s_stats[s_statCount].sumVal = value;
        s_stats[s_statCount].count = 1;
        s_stats[s_statCount].lastSide = side;
        s_statCount++;
        s_newStatCount++;
        SaveStatDump(); // immediate save on new stat type
    }
}

static float __fastcall GetCurrentValueHook(void* self, int32_t stat, void* mi) {
    float val = Original_GetCurrentValue(self, stat, mi);
    if (s_dumpStats && Resolved_GetArmySide2) {
        int side = Resolved_GetArmySide2(self, nullptr);
        RecordStat(stat, val, side);
    }
    return val;
}

static float __fastcall GetStatValueHook(void* self, int32_t stat, void* mi) {
    float val = Original_GetStatValue(self, stat, mi);
    if (s_dumpStats && Resolved_GetArmySide2) {
        int side = Resolved_GetArmySide2(self, nullptr);
        RecordStat(stat, val, side);
    }
    return val;
}

TracerFeature::TracerFeature() { name = "Tracer"; enabled = false; }

void TracerFeature::Init() {
    Resolved_GetArmySide2 = (GetArmySide_t)ResolveMethodOrFallback(
        "Assembly-CSharp", "AutoChess.CoreGameplay.Fight.Units",
        "UnitCore", "get_ArmySide", 0);

    void* sv = ResolveMethodOrFallback("Assembly-CSharp",
        "AutoChess.CoreGameplay.Fight.Units", "BattleUnit",
        "GetStatValue", 1);

    // GetCurrentValue is owned by EnergyAttackSpeed so it can report the
    // baseline/applied pair without competing MinHook installations.
    LOG("[FEATURE] Tracer: GetStatValue=%p get_ArmySide=%p", sv, Resolved_GetArmySide2);

    if (sv && MH_CreateHook(sv, &GetStatValueHook, (LPVOID*)&Original_GetStatValue) == MH_OK && MH_EnableHook(sv) == MH_OK)
        LOG("[FEATURE] Tracer: GetStatValue hooked @ %p", sv);
    else LOG("[FEATURE] Tracer: GetStatValue fail");
}

void TracerFeature::OnUpdate() {
    s_dumpStats = enabled && m_dumpStats && Original_GetStatValue;
    if (s_dumpStats) {
        s_frameCounter++;
        // Auto-save every ~5 seconds (300 frames at 60fps) or on new stat
        if (s_frameCounter >= 300 || s_lastSavedCount != s_newStatCount) {
            SaveStatDump();
            s_frameCounter = 0;
        }
    }
}

void TracerFeature::OnMenu() {
    if (!enabled) return;
    if (!Original_GetStatValue) {
        ImGui::TextColored(ImVec4(1, 0, 0, 1), "hook unavailable"); return;
    }
    ImGui::Checkbox("Dump StatType values", &m_dumpStats);
    ImGui::Text("Unique stats captured: %d (auto-saves to file)", s_statCount);
    if (s_statCount > 0) {
        ImGui::Separator();
        ImGui::Text("Type | Count | Value range | Side");
        for (int i = 0; i < s_statCount && i < 20; i++) {
            ImGui::Text("%4d | %5d | %.0f-%.0f | %d",
                s_stats[i].type, s_stats[i].count,
                s_stats[i].minVal, s_stats[i].maxVal,
                s_stats[i].lastSide);
        }
    }
    if (m_dumpStats)
        ImGui::TextColored(ImVec4(0, 1, 0, 1), "Auto-saving to el_native_stattypes.txt");
}

static TracerFeature g_tracer;
static int g_tracerReg = (RegisterFeature(&g_tracer), 0);
