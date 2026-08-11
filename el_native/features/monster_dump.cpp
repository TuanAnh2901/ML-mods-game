#include "monster_dump.h"
#include "../framework.h"
#include "../il2cpp_resolve.h"
#include "../feature.h"
#include "imgui.h"
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <windows.h>

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
static void* GetListItems(void* list) {
    if (!list) return nullptr;
    __try { return *(void**)((uint8_t*)list + 0x10); }
    __except (EXCEPTION_EXECUTE_HANDLER) { return nullptr; }
}
static int32_t GetListSize(void* list) {
    if (!list) return 0;
    __try { return *(int32_t*)((uint8_t*)list + 0x18); }
    __except (EXCEPTION_EXECUTE_HANDLER) { return 0; }
}
static void* GetArrayElement(void* array, int32_t i) {
    if (!array || i < 0) return nullptr;
    __try { return *(void**)((uint8_t*)array + 0x20 + i * 8); }
    __except (EXCEPTION_EXECUTE_HANDLER) { return nullptr; }
}

// MonsterStaticData backing fields (verified from Cpp2IL MonsterStaticData.cs):
//   monsterId      int32 @ +0x10
//   testUtilsName  string @ +0x18
//   rarity         int32 @ +0xCC   (Rarity enum)
//   race           int32 @ +0x100  (Race enum)
// Fields are read directly because the getters (get_monsterId has 461 bytes of
// IL, GetOwnName is a real method) resolved via stale fallback RVAs and
// crashed the render thread with 0xC0000005.
static int32_t GetFieldI32(void* obj, uint32_t off) {
    if (!obj) return -1;
    __try { return *(int32_t*)((uint8_t*)obj + off); }
    __except (EXCEPTION_EXECUTE_HANDLER) { return -1; }
}
static void* GetFieldPtr(void* obj, uint32_t off) {
    if (!obj) return nullptr;
    __try { return *(void**)((uint8_t*)obj + off); }
    __except (EXCEPTION_EXECUTE_HANDLER) { return nullptr; }
}

// ── Feature ──
MonsterDumpFeature::MonsterDumpFeature() { name = "MonsterDump"; enabled = false; }

void MonsterDumpFeature::Init() {
    // get_monstersIndexedList is private and does not resolve consistently in
    // this build (no fallback entry), so only the public static MonstersList
    // is used. It contains one MonsterStaticData per (id, star grade) row.
    m_getList = (GetStaticList_t)ResolveMethodOrFallback(
        "Assembly-CSharp", "NewAssets.Scripts.Data_Helpers",
        "MonsterDataHelper", "get_MonstersList", 0);
    m_ready = (m_getList != nullptr);
    LOG("[FEATURE] MonsterDump: %s list=%p", m_ready ? "READY" : "FAIL", m_getList);
}

void MonsterDumpFeature::OnUpdate() {}

void MonsterDumpFeature::OnMenu() {
    if (!enabled) return;
    if (!m_ready) {
        ImGui::TextColored(ImVec4(1,0,0,1), "API unresolved"); return;
    }

    if (ImGui::Button("Dump monster ID -> name")) {
        char modulePath[MAX_PATH] = {};
        GetModuleFileNameA(nullptr, modulePath, MAX_PATH);
        char outPath[MAX_PATH] = "el_native_monsters.txt";
        char* slash = strrchr(modulePath, '\\');
        if (!slash) slash = strrchr(modulePath, '/');
        if (slash) {
            *slash = '\0';
            sprintf_s(outPath, "%s\\el_native_monsters.txt", modulePath);
        }
        m_monsterCount = 0;
        FILE* f = nullptr;
        fopen_s(&f, outPath, "w");
        if (!f) { LOG("[FEATURE] MonsterDump: can not open %s", outPath); return; }
        fprintf(f, "Monster ID -> Name dump\n%-6s  %-32s  %-8s  %-8s\n------  --------------------------------  --------  --------\n", "ID", "Name", "Rarity", "Race");
        int32_t* ids = nullptr;
        char* names = nullptr;
        int32_t* rarities = nullptr;
        int32_t* races = nullptr;
        __try {
            // Keep the large dump buffers on the heap.  The render/UI thread
            // has a small stack; the old 2.6 MB local names array caused
            // 0xC00000FD in OnMenu and then poisoned subsequent frames.
            ids = (int32_t*)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY,
                sizeof(int32_t) * 10000);
            names = (char*)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY,
                256 * 10000);
            rarities = (int32_t*)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY,
                sizeof(int32_t) * 10000);
            races = (int32_t*)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY,
                sizeof(int32_t) * 10000);
            if (!ids || !names || !rarities || !races) {
                if (ids) HeapFree(GetProcessHeap(), 0, ids);
                if (names) HeapFree(GetProcessHeap(), 0, names);
                if (rarities) HeapFree(GetProcessHeap(), 0, rarities);
                if (races) HeapFree(GetProcessHeap(), 0, races);
                fclose(f);
                LOG("[FEATURE] MonsterDump: heap allocation failed");
                return;
            }
            int32_t unique = 0;
            void* list = m_getList(nullptr);
            void* items = GetListItems(list);
            int32_t size = GetListSize(list);
            LOG("[FEATURE] MonsterDump: list=%p items=%p size=%d", list, items, size);
            for (int32_t i = 0; items && i < size && i < 10000; ++i) {
                void* msd = GetArrayElement(items, i);
                if (!msd) continue;
                int32_t id = GetFieldI32(msd, 0x10);
                if (id < 0) continue;
                int32_t found = -1;
                for (int32_t j = 0; j < unique; ++j) if (ids[j] == id) { found = j; break; }
                if (found >= 0) continue;
                const char* name = ReadIL2CPPString(GetFieldPtr(msd, 0x18));
                ids[unique] = id;
                strncpy_s(names + (unique * 256), 256, name, _TRUNCATE);
                rarities[unique] = GetFieldI32(msd, 0xCC);
                races[unique] = GetFieldI32(msd, 0x100);
                ++unique;
            }
            for (int32_t i = 0; i < unique; ++i)
                fprintf(f, "%-6d  %-32s  %-8d  %-8d\n", ids[i], names + (i * 256), rarities[i], races[i]);
            m_monsterCount = unique;
            HeapFree(GetProcessHeap(), 0, ids);
            HeapFree(GetProcessHeap(), 0, names);
            HeapFree(GetProcessHeap(), 0, rarities);
            HeapFree(GetProcessHeap(), 0, races);
            ids = nullptr;
            names = nullptr;
            rarities = nullptr;
            races = nullptr;
            fclose(f);
            LOG("[FEATURE] MonsterDump: wrote %d monsters to %s", m_monsterCount, outPath);
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            // The pointers are plain heap allocations so they can be cleaned
            // up from the SEH path without invoking C++ object unwinding.
            // (The declarations remain in scope for MSVC's __try block.)
            HeapFree(GetProcessHeap(), 0, ids);
            HeapFree(GetProcessHeap(), 0, names);
            HeapFree(GetProcessHeap(), 0, rarities);
            HeapFree(GetProcessHeap(), 0, races);
            fclose(f);
            LOG("[FEATURE] MonsterDump: SEH");
        }
    }

    if (m_monsterCount > 0)
        ImGui::TextColored(ImVec4(0,1,0,1), "Dumped %d -> el_native_monsters.txt", m_monsterCount);
    ImGui::Text("File is beside the injector/module executable");
}

static MonsterDumpFeature g_md;
static int g_mdReg = (RegisterFeature(&g_md), 0);