#pragma once
#include <chrono>

namespace rr64::highlight_test {
// The generated test copy substitutes only its clock alias. No production
// runtime entry point, timing behavior, or feature flag is added for testing.
struct Clock {
    using time_point = std::chrono::steady_clock::time_point;
    inline static time_point current{};
    static time_point now() noexcept { return current; }
    static void advance(std::chrono::microseconds elapsed) noexcept { current += elapsed; }
};
}
