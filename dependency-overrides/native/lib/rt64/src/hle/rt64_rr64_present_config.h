#pragma once

#include <atomic>

namespace RT64::RR64FramePacing {
    // Settings requests must not wait for a presenter that is pacing or inside
    // the driver. Only that presenter consumes and applies swapchain changes,
    // before resize/acquisition. Repeated settings edits coalesce to the latest.
    class PendingVsyncChange {
    public:
        void request(bool enabled) {
            pending.store(enabled ? 1 : 0, std::memory_order_relaxed);
        }

        bool consume(bool &enabled) {
            const int requested = pending.exchange(NoRequest, std::memory_order_relaxed);
            if (requested == NoRequest) return false;
            enabled = requested != 0;
            return true;
        }

        void reset() {
            pending.store(NoRequest, std::memory_order_relaxed);
        }

    private:
        static constexpr int NoRequest = -1;
        // The value is the entire message; no adjacent data needs publishing.
        std::atomic<int> pending{NoRequest};
    };
}
