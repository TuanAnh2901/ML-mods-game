#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <mutex>
#include <vector>

enum class HookStatus {
    Unavailable,
    Resolved,
    Hooked,
    Conflict,
};

struct HookRecord {
    std::uintptr_t address = 0;
    std::string owner;
    HookStatus status = HookStatus::Unavailable;
};

class HookRegistry {
public:
    bool Claim(std::uintptr_t address, const std::string& owner);
    bool MarkResolved(std::uintptr_t address);
    bool MarkHooked(std::uintptr_t address);
    bool MarkUnavailable(std::uintptr_t address);
    HookStatus Status(std::uintptr_t address) const;
    std::string Owner(std::uintptr_t address) const;
    std::vector<HookRecord> Snapshot() const;
    std::size_t Size() const;

private:
    std::vector<HookRecord> m_records;
    mutable std::mutex m_mutex;
};

HookRegistry& GlobalHookRegistry();
