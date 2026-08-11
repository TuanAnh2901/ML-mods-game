#include "main_thread_dispatcher.h"
#include "safe_call.h"

#include <algorithm>

std::uint64_t MainThreadDispatcher::PostAfter(std::chrono::milliseconds delay, Task task) {
    if (!task) return 0;
    std::lock_guard<std::mutex> lock(m_mutex);
    const std::uint64_t id = m_nextId++;
    m_entries.push_back({id, Clock::now() + delay, std::move(task), false});
    return id;
}

std::size_t MainThreadDispatcher::Tick(Clock::time_point now) {
    std::vector<Task> ready;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        auto it = m_entries.begin();
        while (it != m_entries.end()) {
            if (it->cancelled || it->due <= now) {
                if (!it->cancelled && it->task) ready.push_back(std::move(it->task));
                it = m_entries.erase(it);
            } else {
                ++it;
            }
        }
    }
    // A faulting task must never take down the game Update thread: isolate it,
    // log which dispatch faulted, and let the remaining tasks run.
    for (auto& task : ready) {
        ElGuard("dispatcher.task", [&] { task(); });
    }
    return ready.size();
}

void MainThreadDispatcher::Clear() {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_entries.clear();
}

MainThreadDispatcher& GlobalMainThreadDispatcher() {
    static MainThreadDispatcher dispatcher;
    return dispatcher;
}
