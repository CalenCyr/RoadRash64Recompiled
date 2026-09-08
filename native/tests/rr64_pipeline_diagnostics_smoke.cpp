#include "hle/rt64_rr64_pipeline_diagnostics.h"
#include <cstdio>
#include <cstring>

static unsigned calls = 0;
static unsigned invalid = 0;
extern "C" void rr64_record_pipeline_stage(unsigned stage, unsigned long long) {
    ++calls;
    if (stage >= static_cast<unsigned>(RT64::RR64PipelineDiagnostics::Stage::Count)) ++invalid;
}

int main(int argc, char** argv) {
    using namespace RT64::RR64PipelineDiagnostics;
    if (argc != 2) return 2;
    const bool expected = std::strcmp(argv[1], "enabled") == 0;
    if (enabled() != expected) return 3;
    {
        Scope outer(Stage::FullSync);
        { Scope inner(Stage::Matching); }
        if (calls != (expected ? 1u : 0u)) return 4;
        outer.finish();
        outer.finish(); // Explicit finish followed by destruction records once.
    }
    if (calls != (expected ? 2u : 0u) || invalid) return 5;
    std::printf("diagnostics_enabled=%u callbacks=%u nested_and_explicit_finish=passed\n", expected, calls);
    return 0;
}
