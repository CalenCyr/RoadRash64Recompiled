#pragma once

#include <atomic>
#include <cstdlib>

namespace rr64::diagnostics {

// Keep the two existing option conventions distinct: the general recorder
// requires exactly "1", while older runtime/autotest switches accept any
// nonempty value that does not begin with "0".
inline bool exact_enabled_value(const char *value) noexcept {
    return value && value[0] == '1' && value[1] == '\0';
}

inline bool enabled_value(const char *value) noexcept {
    return value && value[0] != '\0' && value[0] != '0';
}

// Options are fixed before worker startup; hot paths never query the
// environment or allocate after these function-local values initialize.
inline bool detailed_enabled() {
    static const bool enabled = exact_enabled_value(std::getenv("RR64_DIAGNOSTICS"));
    return enabled;
}

inline bool runtime_trace_enabled() {
    static const bool enabled = enabled_value(std::getenv("RR64_RUNTIME_TRACE")) ||
                                enabled_value(std::getenv("RR64_AUTOTEST"));
    return enabled;
}

inline bool routine_enabled() {
    return detailed_enabled() || runtime_trace_enabled();
}

inline bool network_detail_enabled() {
    // Existing network reports use presence, even when the path is empty.
    static const bool sync_requested = std::getenv("RR64_SYNC_LOG") != nullptr;
    return detailed_enabled() || sync_requested;
}

// A relaxed read is sufficient: this flag only deduplicates a report and
// publishes no gameplay state. Avoid a locked read/modify/write on every
// scheduler call after the first report, and all atomic work when disabled.
inline bool claim_once(std::atomic_bool &reported, bool enabled) noexcept {
    if (!enabled || reported.load(std::memory_order_relaxed)) {
        return false;
    }
    bool expected = false;
    return reported.compare_exchange_strong(expected, true, std::memory_order_relaxed);
}

} // namespace rr64::diagnostics
