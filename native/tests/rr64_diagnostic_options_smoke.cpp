#include "rr64_diagnostic_options.hpp"

#include <array>
#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <thread>

namespace {
unsigned failures = 0;
void expect(bool condition, const char *message) {
    if (!condition) {
        ++failures;
        std::fprintf(stderr, "FAIL: %s\n", message);
    }
}

// Each benchmark call has the same non-inlined boundary and reads a volatile
// mode. This prevents constant-folding the disabled loop into an empty timer.
#if defined(_MSC_VER)
#define RR64_NOINLINE __declspec(noinline)
#else
#define RR64_NOINLINE __attribute__((noinline))
#endif
RR64_NOINLINE bool old_once(std::atomic_bool &reported, volatile bool &enabled) {
    (void)enabled;
    bool expected = false;
    return reported.compare_exchange_strong(expected, true);
}
RR64_NOINLINE bool new_once(std::atomic_bool &reported, volatile bool &enabled) {
    return rr64::diagnostics::claim_once(reported, enabled);
}

void benchmark(const char *name, bool (*claim)(std::atomic_bool &, volatile bool &),
               bool enabled) {
    constexpr unsigned iterations = 4000000;
    std::atomic_bool reported{true};
    volatile bool mode = enabled;
    unsigned accepted = 0;
    const auto begin = std::chrono::steady_clock::now();
    for (unsigned i = 0; i < iterations; ++i) {
        accepted += claim(reported, mode);
    }
    const auto elapsed = std::chrono::duration_cast<std::chrono::nanoseconds>(
        std::chrono::steady_clock::now() - begin).count();
    expect(accepted == 0 && reported.load(), "benchmark must not change report identity");
    std::printf("benchmark=%s iterations=%u ns=%lld ns_per_call=%.3f accepted=%u\n",
                name, iterations, static_cast<long long>(elapsed),
                static_cast<double>(elapsed) / iterations, accepted);
}
} // namespace

int main(int argc, char **argv) {
    if (argc != 5 && argc != 6) {
        std::fprintf(stderr, "usage: smoke detailed runtime routine network [benchmark]\n");
        return 2;
    }
    using namespace rr64::diagnostics;
    expect(detailed_enabled() == (std::atoi(argv[1]) != 0), "general diagnostics mode");
    expect(runtime_trace_enabled() == (std::atoi(argv[2]) != 0), "runtime/autotest mode");
    expect(routine_enabled() == (std::atoi(argv[3]) != 0), "routine report mode");
    expect(network_detail_enabled() == (std::atoi(argv[4]) != 0), "network detail mode");

    expect(!exact_enabled_value(nullptr) && !exact_enabled_value("") &&
           !exact_enabled_value("0") && !exact_enabled_value("01") &&
           !exact_enabled_value("true") && !exact_enabled_value("10") &&
           exact_enabled_value("1"), "exact recorder option convention");
    expect(!enabled_value(nullptr) && !enabled_value("") && !enabled_value("0") &&
           !enabled_value("01") && enabled_value("1") && enabled_value("true") &&
           enabled_value("10"), "legacy runtime option convention");

    std::atomic_bool reported{false};
    for (unsigned i = 0; i < 100000; ++i) {
        if (claim_once(reported, false)) ++failures;
    }
    expect(!reported.load(), "disabled capture must not consume the first report");

    std::atomic<unsigned> reports{0};
    std::array<std::thread, 8> workers;
    for (auto &worker : workers) {
        worker = std::thread([&] {
            for (unsigned i = 0; i < 50000; ++i) {
                if (claim_once(reported, true)) {
                    reports.fetch_add(1, std::memory_order_relaxed);
                }
            }
        });
    }
    for (auto &worker : workers) worker.join();
    expect(reports.load() == 1, "concurrent calls must emit exactly once");
    expect(!claim_once(reported, false) && !claim_once(reported, true),
           "repeated calls remain silent in either mode");

    if (argc == 6) {
        benchmark("previous-locked-cas", old_once, true);
        benchmark("disabled-gate", new_once, false);
        benchmark("already-reported-gate", new_once, true);
    }
    std::printf("diagnostics detailed=%u runtime=%u routine=%u network=%u concurrent_reports=%u failures=%u\n",
                detailed_enabled(), runtime_trace_enabled(), routine_enabled(),
                network_detail_enabled(), reports.load(), failures);
    return failures ? 1 : 0;
}
