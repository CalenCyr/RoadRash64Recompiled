#pragma once

#include <chrono>
#include <cstdint>
#include <cstdlib>

// Observation only: callbacks aggregate counters outside the scheduling path.
extern "C" void rr64_record_scheduler_stage(unsigned int stage, unsigned long long nanoseconds);
extern "C" void rr64_record_scheduler_event(unsigned int event, unsigned long long amount);
extern "C" void rr64_record_guest_queue_wait(unsigned int queue, unsigned long long nanoseconds);
extern "C" void rr64_record_external_queue_event(unsigned int queue, unsigned int outcome,
    unsigned long long nanoseconds);

namespace ultramodern::rr64_diagnostics {
inline bool enabled() {
    static const bool value = [] {
        const char *option = std::getenv("RR64_DIAGNOSTICS");
        return option && option[0] == '1' && option[1] == '\0';
    }();
    return value;
}

enum class Stage : unsigned int {
    ViWakeGap, ViWakeLate, ViDispatch, ViCallback,
    GfxTaskQueue, ScreenQueue, ScreenUpdate, SendDisplayList,
    SpComplete, DpComplete, ExternalDelivery, Count
};
inline constexpr const char* StageNames[] = {
    "vi-wake-gap", "vi-wake-late", "vi-dispatch", "vi-callback",
    "gfx-task-queue", "screen-queue", "screen-update", "send-display-list",
    "sp-complete", "dp-complete", "external-delivery"
};
enum class Event : unsigned int {
    ViSkipped, ViWakeOver22ms, ViWakeOver30ms,
    ExternalEnqueued, ExternalDelivered, ExternalRequeued, ExternalDropped,
    Count
};
inline constexpr const char* EventNames[] = {
    "vi-skipped", "vi-gap-over22ms", "vi-gap-over30ms",
    "external-enqueued", "external-delivered", "external-requeued", "external-dropped"
};
static_assert(sizeof(StageNames) / sizeof(StageNames[0]) == static_cast<unsigned int>(Stage::Count));
static_assert(sizeof(EventNames) / sizeof(EventNames[0]) == static_cast<unsigned int>(Event::Count));

// Every use of this clock belongs to optional scheduler telemetry. Actual VI
// deadlines and queue behavior use the runtime's separate clocks. Returning a
// zero timestamp when disabled also avoids reads in callers that collect
// enqueue/delivery times without a Scope.
struct Clock : std::chrono::steady_clock {
    static time_point now() noexcept {
        return enabled() ? std::chrono::steady_clock::now() : time_point{};
    }
};
inline void record(Stage stage, Clock::duration elapsed) {
    if (!enabled()) { return; }
    const auto ns = std::chrono::duration_cast<std::chrono::nanoseconds>(elapsed).count();
    if (ns >= 0) {
        rr64_record_scheduler_stage(static_cast<unsigned int>(stage), static_cast<unsigned long long>(ns));
    }
}
inline void count(Event event, std::uint64_t amount = 1) {
    if (!enabled()) { return; }
    rr64_record_scheduler_event(static_cast<unsigned int>(event), amount);
}
class Scope {
public:
    explicit Scope(Stage stage) : stage_(stage), start_(Clock::now()) { }
    ~Scope() {
        if (enabled()) { record(stage_, Clock::now() - start_); }
    }
    Scope(const Scope&) = delete;
    Scope& operator=(const Scope&) = delete;
private:
    Stage stage_;
    Clock::time_point start_;
};
}
