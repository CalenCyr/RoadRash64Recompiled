#include <cstdlib>
#include <iostream>

#include "hle/rt64_rr64_interpolation_pressure.h"

namespace {
    void require(bool condition, const char *message) {
        if (!condition) {
            std::cerr << "RR64 interpolation pressure failure: " << message << '\n';
            std::exit(EXIT_FAILURE);
        }
    }
}

int main() {
    using namespace RT64::RR64FramePacing;
    const QueuedAuthoredWorkload current{10u, 7u, {2u, true}};
    const QueuedAuthoredWorkload superseding{11u, 7u, {2u, true}};

    require(renderSupersededBatchNatively(true, true, true, current, superseding),
        "a newer writer before the same VI avoids optional interpolation");
    require(!renderSupersededBatchNatively(false, true, true, current, superseding),
        "unsupported retention, ray tracing, pauses and non-race paths keep their policy");
    require(!renderSupersededBatchNatively(true, false, true, current, superseding),
        "unknown target does not authorize a retained native image");
    require(!renderSupersededBatchNatively(true, true, false, current, superseding),
        "native-rate rendering has no optional work to suppress");

    for (const auto invalid : {
        QueuedAuthoredWorkload{},
        QueuedAuthoredWorkload{9u, 7u, {2u, true}},
        QueuedAuthoredWorkload{10u, 7u, {2u, true}},
        QueuedAuthoredWorkload{11u, 8u, {2u, true}},
        QueuedAuthoredWorkload{11u, 6u, {2u, true}},
        QueuedAuthoredWorkload{11u, 7u, {3u, true}},
        QueuedAuthoredWorkload{11u, 7u, {2u, false}}}) {
        require(!renderSupersededBatchNatively(true, true, true, current, invalid),
            "empty, stale, different-interval or different-scene work is not supersession");
    }
    require(!renderSupersededBatchNatively(true, true, true,
        {0u, 7u, {2u, true}}, superseding), "current writer must be valid");
    require(!renderSupersededBatchNatively(true, true, true,
        {10u, 7u, {0u, true}}, {11u, 7u, {0u, true}}), "scene must be initialized");

    // Exercise the actual presentation count helper: under pressure an owned
    // native image stays one image even with a legitimate slower source. Once
    // the queued writer belongs to the next VI, normal fractional output is
    // permitted again immediately; no sticky rate or cooldown state exists.
    for (const uint32_t source : {15u, 30u, 60u}) {
        for (const uint32_t target : {60u, 90u, 120u, 144u, 240u}) {
            const bool optional = target > source;
            const bool reduce = renderSupersededBatchNatively(true, true,
                optional, current, superseding);
            require(reduce == optional, "only optional pictures may be reduced");
            require(presentationFrameCount(true, 1u, target, source) == 1u,
                "owned native endpoint cannot expand according to a slow source ratio");
            const QueuedAuthoredWorkload nextInterval{12u, 8u, {2u, true}};
            require(!renderSupersededBatchNatively(true, true, optional,
                superseding, nextInterval), "ordinary interpolation returns at next interval");
            const InterpolationBatchMetadata certificate{current.writer,
                current.scene, {0x100000u, 320u, 2u}, source, target};
            require(certificate.matches(current.writer, current.scene,
                certificate.target, source, target),
                "native reduction preserves the writer and actual source-rate certificate");
        }
    }
    std::cout << "RR64 interpolation pressure smoke tests passed\n";
}
