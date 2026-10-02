#include "ultramodern/rr64_scheduler_diagnostics.hpp"

#include <cstdio>
#include <cstdlib>

namespace {
unsigned stages = 0, events = 0, invalid = 0;
#if defined(_MSC_VER)
__declspec(noinline)
#else
__attribute__((noinline))
#endif
void benchmark_scope() {
    using namespace ultramodern::rr64_diagnostics;
    Scope scope{Stage::SpComplete};
}
}

extern "C" void rr64_record_scheduler_stage(unsigned stage, unsigned long long) {
    ++stages;
    if (stage >= unsigned(ultramodern::rr64_diagnostics::Stage::Count)) ++invalid;
}
extern "C" void rr64_record_scheduler_event(unsigned event, unsigned long long amount) {
    events += static_cast<unsigned>(amount);
    if (event >= unsigned(ultramodern::rr64_diagnostics::Event::Count)) ++invalid;
}

int main(int argc, char **argv) {
    namespace probe = ultramodern::rr64_diagnostics;
    if (argc != 2) return 2;
    const bool expected = std::atoi(argv[1]) != 0;
    if (probe::enabled() != expected) return 3;
    const auto begin = probe::Clock::now();
    if ((begin != probe::Clock::time_point{}) != expected) return 4;
    {
        probe::Scope scope{probe::Stage::GfxTaskQueue};
        probe::count(probe::Event::ExternalEnqueued, 3);
        probe::record(probe::Stage::ExternalDelivery, std::chrono::nanoseconds(5));
        probe::record(probe::Stage::ExternalDelivery, std::chrono::nanoseconds(-1));
    }
    if (stages != (expected ? 2u : 0u) || events != (expected ? 3u : 0u) || invalid) return 5;
    const auto end = probe::Clock::now();
    if (end < begin || (!expected && end != probe::Clock::time_point{})) return 6;

    constexpr unsigned iterations = 1000000;
    const auto benchmark_start = std::chrono::steady_clock::now();
    for (unsigned i = 0; i < iterations; ++i) {
        benchmark_scope();
    }
    const auto elapsed = std::chrono::duration_cast<std::chrono::nanoseconds>(
        std::chrono::steady_clock::now() - benchmark_start).count();
    if (stages != (expected ? iterations + 2 : 0u)) return 7;
    std::printf("scheduler enabled=%u callbacks=%u events=%u disabled_clock_zero=%u iterations=%u ns=%lld ns_per_scope=%.3f PASS\n",
        expected, stages, events, !expected && end == probe::Clock::time_point{},
        iterations, static_cast<long long>(elapsed), double(elapsed) / iterations);
}
