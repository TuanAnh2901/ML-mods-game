#include "hook_registry.h"

namespace {
HookRecord* Find(std::vector<HookRecord>& records, std::uintptr_t address) {
    for (auto& record : records) {
        if (record.address == address) return &record;
    }
    return nullptr;
}

const HookRecord* Find(const std::vector<HookRecord>& records, std::uintptr_t address) {
    for (const auto& record : records) {
        if (record.address == address) return &record;
    }
    return nullptr;
}
}

bool HookRegistry::Claim(std::uintptr_t address, const std::string& owner) {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!address || owner.empty()) return false;
    HookRecord* record = Find(m_records, address);
    if (record) {
        if (record->owner == owner) return true;
        record->status = HookStatus::Conflict;
        return false;
    }
    HookRecord created;
    created.address = address;
    created.owner = owner;
    created.status = HookStatus::Unavailable;
    m_records.push_back(created);
    return true;
}

bool HookRegistry::MarkResolved(std::uintptr_t address) {
    std::lock_guard<std::mutex> lock(m_mutex);
    HookRecord* record = Find(m_records, address);
    if (!record || record->status == HookStatus::Conflict) {
        if (!record) return false;
        record->status = HookStatus::Resolved;
        return true;
    }
    record->status = HookStatus::Resolved;
    return true;
}

bool HookRegistry::MarkHooked(std::uintptr_t address) {
    std::lock_guard<std::mutex> lock(m_mutex);
    HookRecord* record = Find(m_records, address);
    if (!record || record->status == HookStatus::Conflict) return false;
    record->status = HookStatus::Hooked;
    return true;
}

bool HookRegistry::MarkUnavailable(std::uintptr_t address) {
    std::lock_guard<std::mutex> lock(m_mutex);
    HookRecord* record = Find(m_records, address);
    if (!record || record->status == HookStatus::Conflict) return false;
    record->status = HookStatus::Unavailable;
    return true;
}

HookStatus HookRegistry::Status(std::uintptr_t address) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    const HookRecord* record = Find(m_records, address);
    return record ? record->status : HookStatus::Unavailable;
}

std::string HookRegistry::Owner(std::uintptr_t address) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    const HookRecord* record = Find(m_records, address);
    return record ? record->owner : std::string();
}

std::vector<HookRecord> HookRegistry::Snapshot() const { std::lock_guard<std::mutex> lock(m_mutex); return m_records; }
std::size_t HookRegistry::Size() const { std::lock_guard<std::mutex> lock(m_mutex); return m_records.size(); }

HookRegistry& GlobalHookRegistry() {
    static HookRegistry registry;
    return registry;
}
