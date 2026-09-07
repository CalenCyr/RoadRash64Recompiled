//
// Road Rash 64 frame-pacing policy.
//

#pragma once

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdlib>

namespace RT64::RR64FramePacing {
    constexpr uint32_t MaximumCadenceFrames = 8;
    constexpr uint32_t ConsoleRefreshRate = 60;

    constexpr bool requiresFrameMatching(bool stablePresentation,
        bool raytracingEnabled, uint32_t targetRate, uint32_t originalRate)
    {
        // Matching supplies fractional pictures (or ray-traced velocities).
        // Native output does not need it in races, results, or menus. An
        // unresolved source rate cannot authorize fractional pictures either.
        return raytracingEnabled || (stablePresentation ?
            ((originalRate > 0) && (targetRate > originalRate)) :
            (targetRate > 0));
    }

    // Process-start A/B switch. This changes host presentation only and never
    // changes the guest clock. Read once before the queues start using it.
    inline bool stablePresentationEnabled() {
        static const bool enabled = [] {
#ifdef _MSC_VER
            char *value = nullptr;
            std::size_t length = 0;
            _dupenv_s(&value, &length, "RR64_STABLE_PRESENTATION");
            const bool selected = (value == nullptr) || (value[0] != '0');
            std::free(value);
            return selected;
#else
            const char *value = std::getenv("RR64_STABLE_PRESENTATION");
            return (value == nullptr) || (value[0] != '0');
#endif
        }();
        return enabled;
    }

    constexpr bool usePresentWait(bool supported, bool d3d12,
        bool vsync, bool softwarePacing, bool stablePresentation)
    {
        // Present(1) owns D3D12 VSync. The software clock owns tearing output.
        // Neither path may acquire a second DXGI frame-latency gate.
        return supported && !(d3d12 &&
            (softwarePacing || (stablePresentation && vsync)));
    }

    enum class InterpolationTargetSelection : uint32_t {
        Unavailable = 0,
        StockFlag = 1,
        PresentationHistory = 2
    };

    struct PresentationTargetIdentity {
        uint32_t address = 0;
        uint16_t width = 0;
        uint8_t siz = 0;

        constexpr bool valid() const {
            return (address != 0) && (width != 0);
        }

        constexpr bool operator==(const PresentationTargetIdentity &rhs) const {
            return (address == rhs.address) && (width == rhs.width) &&
                (siz == rhs.siz);
        }
    };

    // The VI-selected buffer can differ from the next buffer being drawn.
    // Keep a bounded exact history of VI-selected targets so native and
    // generated images can retain their own pixels. This history cannot
    // authorize an arbitrary off-screen target or establish its source rate.
    class PresentationTargetHistory {
    public:
        static constexpr std::size_t Capacity = 8;

        constexpr void clear() {
            entries = {};
            entryCount = 0;
        }

        constexpr void record(PresentationTargetIdentity target) {
            if (!target.valid()) {
                return;
            }

            std::size_t existing = entryCount;
            for (std::size_t index = 0; index < entryCount; index++) {
                if (entries[index] == target) {
                    existing = index;
                    break;
                }
            }

            if (existing < entryCount) {
                for (std::size_t index = existing + 1; index < entryCount; index++) {
                    entries[index - 1] = entries[index];
                }
                entryCount--;
            }
            else if (entryCount == Capacity) {
                for (std::size_t index = 1; index < entryCount; index++) {
                    entries[index - 1] = entries[index];
                }
                entryCount--;
            }

            entries[entryCount++] = target;
        }

        constexpr void eraseAddress(uint32_t address) {
            std::size_t kept = 0;
            for (std::size_t index = 0; index < entryCount; index++) {
                if (entries[index].address != address) {
                    entries[kept++] = entries[index];
                }
            }
            entryCount = kept;
        }

        constexpr bool contains(PresentationTargetIdentity target) const {
            if (!target.valid()) {
                return false;
            }

            for (std::size_t index = 0; index < entryCount; index++) {
                if (entries[index] == target) {
                    return true;
                }
            }

            return false;
        }

        constexpr std::size_t size() const {
            return entryCount;
        }

    private:
        std::array<PresentationTargetIdentity, Capacity> entries{};
        std::size_t entryCount = 0;
    };

    struct DeadlineDecision {
        int64_t deadlineNanoseconds = 0;
        bool rebased = false;
    };

    // The old presenter derived every wait from whichever frame happened to
    // reach Present last. When its source-rate guess changed, pacing toggled
    // on and off and a missed interval could be followed by an uneven catch-up
    // interval. This coordinator owns one absolute deadline stream. A late
    // frame is rebased immediately, so timing debt is discarded instead of
    // accumulating or being replayed as a burst.
    class StableDeadlinePacer {
    public:
        constexpr void reset() {
            initialized = false;
            scheduledRate = 0;
            deadlineNanoseconds = 0;
        }

        constexpr DeadlineDecision schedule(
            int64_t nowNanoseconds,
            uint32_t targetRate)
        {
            if (targetRate == 0) {
                reset();
                return { nowNanoseconds, true };
            }

            const int64_t periodNanoseconds =
                1'000'000'000LL / static_cast<int64_t>(targetRate);
            if (!initialized || (scheduledRate != targetRate)) {
                initialized = true;
                scheduledRate = targetRate;
                deadlineNanoseconds = nowNanoseconds;
                return { deadlineNanoseconds, true };
            }

            const int64_t plannedDeadline =
                deadlineNanoseconds + periodNanoseconds;
            if (nowNanoseconds >= plannedDeadline) {
                deadlineNanoseconds = nowNanoseconds;
                return { deadlineNanoseconds, true };
            }

            deadlineNanoseconds = plannedDeadline;
            return { deadlineNanoseconds, false };
        }

        // schedule() cannot see a late timer wake or work between the wait and
        // submission. Anchor the next deadline to that actual submission if it
        // missed this deadline; otherwise the next frame can burst to catch up.
        constexpr void recordPresent(int64_t actualNanoseconds) {
            if (initialized && actualNanoseconds > deadlineNanoseconds) {
                deadlineNanoseconds = actualNanoseconds;
            }
        }

    private:
        bool initialized = false;
        uint32_t scheduledRate = 0;
        int64_t deadlineNanoseconds = 0;
    };

    // A known lower-rate source can repeat its selected completed image at an
    // integer output cadence when interpolation is unavailable. Equal or
    // unresolved source rates require only one native image.
    constexpr uint32_t exactCadenceFrameCount(
        uint32_t targetRate,
        uint32_t originalRate)
    {
        if ((targetRate == 0) || (originalRate == 0) ||
            (targetRate <= originalRate) || ((targetRate % originalRate) != 0))
        {
            return 1;
        }

        return std::clamp(
            targetRate / originalRate,
            uint32_t(1),
            MaximumCadenceFrames);
    }

    constexpr uint32_t presentationFrameCount(
        bool interpolationAvailable,
        uint32_t generatedFrameCount,
        uint32_t targetRate,
        uint32_t originalRate)
    {
        if (interpolationAvailable && (generatedFrameCount > 0)) {
            return std::min(generatedFrameCount, MaximumCadenceFrames);
        }

        return exactCadenceFrameCount(targetRate, originalRate);
    }

    // Legacy queue policy: clock predictions do not abandon generated frames.
    // The caller supplies whether another workload supersedes the current one.
    constexpr bool shouldAbandonRemainingGeneratedFrames(
        bool newerWorkloadPending)
    {
        return newerWorkloadPending;
    }

    constexpr bool allowPresentationHistoryTarget(
        bool raceActive,
        uint32_t targetRate,
        uint32_t /* originalRate */,
        bool wasActuallyPresented)
    {
        // Retaining an exact selected target is useful for native images too.
        // Source cadence controls image count separately from target admission.
        return raceActive && (targetRate > 0) && wasActuallyPresented;
    }
}
