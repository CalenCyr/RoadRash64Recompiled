// Presentation cadence measured at the parsed authored-workload boundary.
// This never changes guest simulation time or infers duplicate visual content.
#pragma once

#include <cstdint>

namespace RT64::RR64FramePacing {
    struct AuthoredCadenceObservation {
        // Zero is deliberately unproven: callers render one native image.
        uint32_t sourceRate = 0;
        // Parsed arrival cadence, not proof of distinct poses:
        // 0: warm-up/ambiguous; 1: observed60; 2: observed30; 3: discontinuity.
        uint32_t reason = 0;
        uint64_t intervalNs = 0;
    };

    class AuthoredCadenceTracker {
    public:
        static constexpr uint64_t MillisecondNs = 1'000'000u;
        static constexpr uint64_t Native60MinimumNs = 13u * MillisecondNs;
        static constexpr uint64_t Native60MaximumNs = 21u * MillisecondNs;
        static constexpr uint64_t Interpolate30MinimumNs = 28u * MillisecondNs;
        static constexpr uint64_t Interpolate30MaximumNs = 38u * MillisecondNs;
        static constexpr uint64_t MaximumContinuousIntervalNs = 100u * MillisecondNs;
        static constexpr uint32_t RequiredConsecutiveIntervals = 3u;

        constexpr void reset() {
            previousTimestampNs = 0;
            previousSceneEpoch = 0;
            consecutive60 = 0;
            consecutive30 = 0;
            hasPrevious = false;
        }

        constexpr AuthoredCadenceObservation observe(uint64_t timestampNs,
            uint64_t sceneEpoch, bool raceActive)
        {
            if (!raceActive || (sceneEpoch == 0u)) {
                reset();
                return {0u, 3u, 0u};
            }

            if (!hasPrevious) {
                begin(timestampNs, sceneEpoch);
                return {};
            }

            if ((sceneEpoch != previousSceneEpoch) ||
                (timestampNs <= previousTimestampNs)) {
                begin(timestampNs, sceneEpoch);
                return {0u, 3u, 0u};
            }

            // Subtract only after the ordering check, so backward clocks cannot
            // wrap into an apparently valid interval.
            const uint64_t intervalNs = timestampNs - previousTimestampNs;
            previousTimestampNs = timestampNs;
            if (intervalNs > MaximumContinuousIntervalNs) {
                begin(timestampNs, sceneEpoch);
                return {0u, 3u, intervalNs};
            }

            if ((intervalNs >= Native60MinimumNs) &&
                (intervalNs <= Native60MaximumNs)) {
                // The first fast interval immediately revokes a previous30Hz
                // classification, even before the new60Hz run is confirmed.
                consecutive30 = 0;
                if (consecutive60 < RequiredConsecutiveIntervals) {
                    consecutive60++;
                }
                return consecutive60 == RequiredConsecutiveIntervals ?
                    AuthoredCadenceObservation{60u, 1u, intervalNs} :
                    AuthoredCadenceObservation{0u, 0u, intervalNs};
            }

            if ((intervalNs >= Interpolate30MinimumNs) &&
                (intervalNs <= Interpolate30MaximumNs)) {
                consecutive60 = 0;
                if (consecutive30 < RequiredConsecutiveIntervals) {
                    consecutive30++;
                }
                return consecutive30 == RequiredConsecutiveIntervals ?
                    AuthoredCadenceObservation{30u, 2u, intervalNs} :
                    AuthoredCadenceObservation{0u, 0u, intervalNs};
            }

            // Short bursts, uncertain rates, and isolated stalls never retain
            // permission to expand a workload into multiple presentation slots.
            consecutive60 = 0;
            consecutive30 = 0;
            return {0u, 0u, intervalNs};
        }

    private:
        constexpr void begin(uint64_t timestampNs, uint64_t sceneEpoch) {
            previousTimestampNs = timestampNs;
            previousSceneEpoch = sceneEpoch;
            consecutive60 = 0;
            consecutive30 = 0;
            hasPrevious = true;
        }

        uint64_t previousTimestampNs = 0;
        uint64_t previousSceneEpoch = 0;
        uint32_t consecutive60 = 0;
        uint32_t consecutive30 = 0;
        bool hasPrevious = false;
    };
}
