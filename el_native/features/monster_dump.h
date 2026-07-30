#pragma once
#include "../feature.h"

// Reads MonsterDataHelper.get_MonstersList() to dump all monster IDs + names.
// IL2CPP string layout: +0x00 klass, +0x08 monitor, +0x10 length(int32), +0x14 chars
struct MonsterDumpFeature : Feature {
    MonsterDumpFeature();
    void Init() override;
    void OnUpdate() override;
    void OnMenu() override;

    typedef void* (__fastcall* GetHelper_t)(void* methodInfo);
    typedef void* (__fastcall* GetList_t)(void* self, void* methodInfo);
    typedef int32_t (__fastcall* GetId_t)(void* self, void* methodInfo);
    typedef void* (__fastcall* GetName_t)(void* self, void* methodInfo);

    GetHelper_t m_getHelper = nullptr;
    GetList_t m_getList = nullptr;
    GetId_t m_getId = nullptr;
    GetName_t m_getName = nullptr;

    bool m_ready = false;
    bool m_dumpDone = false;
    int m_monsterCount = 0;
};
