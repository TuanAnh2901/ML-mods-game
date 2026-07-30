#include "monster_dump.h"
#include "../framework.h"
#include "../il2cpp_resolve.h"
#include "../feature.h"
#include "imgui.h"
#include <cstdint>
#include <cstdio>

// IL2CPP System.String layout (x64): +0x00 klass, +0x08 monitor, +0x10 length, +0x14 chars
static const char* ReadIL2CPPString(void* s) {
    if (!s) return "(null)";
    __try {
        int32_t len = *(int32_t*)((uint8_t*)s + 0x10);
        if (len <= 0 || len > 256) return "(bad)";
        static char buf[512];
        const wchar_t* wcs = (const wchar_t*)((uint8_t*)s + 0x14);
        int out = 0;
        for (int32_t i = 0; i < len && out < (int)sizeof(buf)-3; ++i) {
            if (wcs[i] < 128) buf[out++] = (char)wcs[i];
            else if (wcs[i] < 256) buf[out++] = '_';
        }
        buf[out] = '\0';
        return buf;
    } __except (EXCEPTION_EXECUTE_HANDLER) { return "(SEH)"; }
}

// IL2CPP List<T>: +0x10 _items, +0x18 _size
static void* GetListItems(void* list) { return list ? *(void**)((uint8_t*)list + 0x10) : nullptr; }
static int32_t GetListSize(void* list) { return list ? *(int32_t*)((uint8_t*)list + 0x18) : 0; }
static void* GetArrayElement(void* array, int32_t i) { return array ? *(void**)((uint8_t*)array + 0x20 + i * 8) : nullptr; }

// ── Feature ──
MonsterDumpFeature::MonsterDumpFeature() { name = "MonsterDump"; enabled = false; }

void MonsterDumpFeature::Init() {
    m_getHelper = (GetHelper_t)ResolveMethodOrFallback(
        "Assembly-CSharp", "", "MonsterDataUtils", "get_MonsterDataHelper", 0);
    m_getList = (GetList_t)ResolveMethodOrFallback(
        "Assembly-CSharp", "NewAssets.Scripts.Data_Helpers",
        "MonsterDataHelper", "get_MonstersList", 0);
    m_getId = (GetId_t)ResolveMethodOrFallback(
        "Assembly-CSharp", "NewAssets.Scripts.DataClasses",
        "MonsterStaticData", "get_monsterId", 0);
    m_getName = (GetName_t)ResolveMethodOrFallback(
        "Assembly-CSharp", "NewAssets.Scripts.DataClasses",
        "MonsterStaticData", "GetOwnName", 0);
    m_ready = m_getHelper && m_getList && m_getId && m_getName;
    LOG("[FEATURE] MonsterDump: %s", m_ready ? "READY" : "FAIL");
}

void MonsterDumpFeature::OnUpdate() {}

void MonsterDumpFeature::OnMenu() {
    if (!enabled) return;
    if (!m_ready) {
        ImGui::TextColored(ImVec4(1,0,0,1), "API unresolved"); return;
    }

    if (ImGui::Button("Dump monster ID -> name")) {
        const char* outPath = "el_native_monsters.txt";
        m_monsterCount = 0;
        __try {
            void* helper = m_getHelper(nullptr);
            if (!helper) { LOG("[FEATURE] MonsterDump: helper null"); return; }
            void* list = m_getList(helper, nullptr);
            if (!list) { LOG("[FEATURE] MonsterDump: list null"); return; }
            void* items = GetListItems(list);
            int32_t size = GetListSize(list);
            if (!items || size <= 0) return;

            FILE* f = nullptr;
            fopen_s(&f, outPath, "w");
            if (!f) { LOG("[FEATURE] MonsterDump: can't open %s", outPath); return; }

            fprintf(f, "Monster ID -> Name dump\n");
            fprintf(f, "%-6s  %s\n", "ID", "Name");
            fprintf(f, "------  --------------------------------\n");

            for (int32_t i = 0; i < size && i < 2000; ++i) {
                void* msd = GetArrayElement(items, i);
                if (!msd) continue;
                int32_t id = m_getId(msd, nullptr);
                void* nameStr = m_getName(msd, nullptr);
                const char* name = ReadIL2CPPString(nameStr);
                fprintf(f, "%-6d  %s\n", id, name);
                m_monsterCount = i + 1;
            }
            fclose(f);
            LOG("[FEATURE] MonsterDump: wrote %d monsters to %s", m_monsterCount, outPath);
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            LOG("[FEATURE] MonsterDump: SEH");
        }
    }

    if (m_monsterCount > 0)
        ImGui::TextColored(ImVec4(0,1,0,1), "Dumped %d -> el_native_monsters.txt", m_monsterCount);
    ImGui::Text("File in game folder next to el_native.log");
}

static MonsterDumpFeature g_md;
static int g_mdReg = (RegisterFeature(&g_md), 0);