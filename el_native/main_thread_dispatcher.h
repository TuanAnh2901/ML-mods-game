#pragma once

#include <chrono>
#include <cstdint>
#include <functional>
#include <mutex>
#include <vector>

class MainThreadDispatcher {
public:
    using Clock = std::chrono::steady_clock;
    using Task = std::function<void()>;

    std::uint64_t PostAfter(std::chrono::milliseconds delay, Task task);
    std::size_t Tick(Clock::time_point now = Clock::now());
    void Clear();

private:
    struct Entry {
        std::uint64_t id;
        Clock::time_point due;
        Task task;
        bool cancelled;
    };
    std::mutex m_mutex;
    std::vector<Entry> m_entries;
    std::uint64_t m_nextId = 1;
};

MainThreadDispatcher& GlobalMainThreadDispatcher();
