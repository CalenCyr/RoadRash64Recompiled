#pragma once
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace recompui {
// Sparse process-start checkpoints only; disabled release capture reads no clock.
inline void startup_checkpoint(const char* phase) {
    static const bool enabled = [] {
        const char* value = std::getenv("RR64_DIAGNOSTICS");
        return value && std::strcmp(value, "1") == 0;
    }();
    if (!enabled) return;
    const auto ns = std::chrono::duration_cast<std::chrono::nanoseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
    std::fprintf(stderr, "[RR64-STARTUP] phase=%s monotonic-ns=%lld\n", phase,
        static_cast<long long>(ns));
}
}
