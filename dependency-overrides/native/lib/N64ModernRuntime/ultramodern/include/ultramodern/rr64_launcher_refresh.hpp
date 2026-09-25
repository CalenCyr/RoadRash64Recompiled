#pragma once

#include <atomic>
#include <cassert>

namespace ultramodern::rr64 {
// Modal launcher operations may pause presentation while VI time keeps moving.
// Bound only synthetic launcher refreshes; guest tasks and VI events never use
// these credits. The action queue publishes the payload, not this counter.
class LauncherRefreshBudget {
    std::atomic<unsigned> pending_{0};

public:
    static constexpr unsigned capacity = 2;
    bool try_acquire() noexcept {
        unsigned pending = pending_.load(std::memory_order_relaxed);
        while (pending < capacity) {
            if (pending_.compare_exchange_weak(pending, pending + 1,
                                              std::memory_order_relaxed)) return true;
        }
        return false;
    }
    void release() noexcept {
        const unsigned previous = pending_.fetch_sub(1, std::memory_order_relaxed);
        assert(previous != 0);
        (void)previous;
    }
    unsigned pending() const noexcept { return pending_.load(std::memory_order_relaxed); }
    class Completion {
        LauncherRefreshBudget &budget_;
    public:
        explicit Completion(LauncherRefreshBudget &budget) noexcept : budget_(budget) {}
        Completion(const Completion &) = delete;
        Completion &operator=(const Completion &) = delete;
        ~Completion() { budget_.release(); }
    };
    Completion complete_on_exit() noexcept { return Completion{*this}; }
};
}
