// Authored framebuffer identity, independent of interpolated GPU replays.
#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

#include "rt64_rr64_frame_metadata.h"
#include "rt64_rr64_authored_cadence.h"

namespace RT64::RR64FramePacing {
    struct AuthoredTargetStamp {
        PresentationTargetIdentity target;
        uint64_t writerWorkloadId = 0;
        SceneSnapshot scene;
        uint32_t sourceRate = 0;
        uint64_t authoredTimestampNs = 0;
        uint64_t authoredIntervalNs = 0;

        constexpr bool valid() const {
            return target.valid() && (target.siz <= 3u) &&
                (writerWorkloadId != 0u) && (scene.epoch != 0u);
        }
    };

    // State owns this bounded ledger. Each authored write replaces the prior
    // writer even when it is only a partial update. Callers must also report
    // depth aliases and CPU writes; rendered interpolation must not update it.
    class AuthoredTargetTracker {
    public:
        static constexpr std::size_t MaximumEntries = 16;

        constexpr void clear() {
            for (Entry &entry : entries) {
                entry = {};
            }
            nextEviction = 0;
        }

        constexpr void invalidateRange(uint32_t address, uint64_t bytes) {
            if (bytes == 0u) {
                return;
            }
            const uint64_t begin = address;
            const uint64_t remaining = AddressLimit - begin;
            const uint64_t end = begin + ((bytes < remaining) ? bytes : remaining);
            for (Entry &entry : entries) {
                if (entry.stamp.valid() && (begin < entry.end) &&
                    (uint64_t(entry.stamp.target.address) < end)) {
                    entry = {};
                }
            }
        }

        constexpr void invalidateAddress(uint32_t address) {
            for (Entry &entry : entries) {
                if (entry.stamp.target.address == address) {
                    entry = {};
                }
            }
        }

        constexpr bool write(PresentationTargetIdentity target, uint32_t height,
            uint64_t writerWorkloadId, SceneSnapshot scene, uint32_t sourceRate = 0,
            uint64_t authoredTimestampNs = 0, uint64_t authoredIntervalNs = 0)
        {
            // An unrepresentable write has no safe bounded interval. Forget
            // existing certificates rather than retaining one it might overlap.
            if (!target.valid() || (target.siz > 3u) || (height == 0u)) {
                clear();
                return false;
            }
            const uint64_t rowBytes = ((uint64_t(target.width) << target.siz) + 1u) / 2u;
            const uint64_t bytes = rowBytes * uint64_t(height);
            if (bytes > (AddressLimit - uint64_t(target.address))) {
                clear();
                return false;
            }

            uint64_t end = uint64_t(target.address) + bytes;
            for (const Entry &entry : entries) {
                // A HUD-only update must not shrink the known image extent:
                // later writes into its retained rows still invalidate it.
                if (entry.stamp.valid() && (entry.stamp.target == target) &&
                    (entry.end > end)) {
                    end = entry.end;
                }
            }
            invalidateRange(target.address, end - uint64_t(target.address));

            const AuthoredTargetStamp stamp{target, writerWorkloadId, scene,
                sourceRate, authoredTimestampNs, authoredIntervalNs};
            if (!stamp.valid()) {
                return false;
            }
            for (Entry &entry : entries) {
                if (!entry.stamp.valid()) {
                    entry = {stamp, end};
                    return true;
                }
            }
            // Eviction only loses eligibility; it cannot invent an identity.
            entries[nextEviction] = {stamp, end};
            nextEviction = (nextEviction + 1u) % MaximumEntries;
            return true;
        }

        constexpr AuthoredTargetStamp find(PresentationTargetIdentity target,
            SceneSnapshot scene) const
        {
            for (const Entry &entry : entries) {
                if (entry.stamp.valid() && (entry.stamp.target == target) &&
                    (entry.stamp.scene == scene)) {
                    return entry.stamp;
                }
            }
            return {};
        }

    private:
        static constexpr uint64_t AddressLimit = uint64_t{1} << 32u;
        struct Entry {
            AuthoredTargetStamp stamp;
            uint64_t end = 0;
        };
        std::array<Entry, MaximumEntries> entries{};
        std::size_t nextEviction = 0;
    };

    // The present watermark says which submitted work has finished. It does
    // not identify the content of the selected buffer. That content must match
    // its captured authored writer exactly, even when another buffer was drawn
    // more recently. Composition, resource epochs, and GPU ownership are checked
    // by the batch owner separately.
    constexpr bool ownedBatchRequestMatches(const InterpolationBatchMetadata &batch,
        const AuthoredTargetStamp &request, uint64_t presentWorkloadWatermark,
        uint32_t sourceRate, uint32_t targetRate)
    {
        return request.valid() &&
            (request.writerWorkloadId <= presentWorkloadWatermark) &&
            (request.sourceRate == sourceRate) &&
            batch.matches(request.writerWorkloadId, request.scene,
                request.target, sourceRate, targetRate);
    }
}
