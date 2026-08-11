#pragma once
#include "../feature.h"

// Reads MonsterDataHelper.get_MonstersList() to dump all monster IDs + names.
// IL2CPP string layout: +0x00 klass, +0x08 monitor, +0x10 length(int32), +0x14 chars
struct MonsterDumpFeature : Feature {
    MonsterDumpFeature();
    void Init() override;
    void OnUpdate() override;
    void OnMenu() override;

    // Static property; only the hidden MethodInfo argument is passed.
    typedef void* (__fastcall* GetStaticList_t)(void* methodInfo);

    GetStaticList_t m_getList = nullptr;

    bool m_ready = false;
    bool m_dumpDone = false;
    int m_monsterCount = 0;
};