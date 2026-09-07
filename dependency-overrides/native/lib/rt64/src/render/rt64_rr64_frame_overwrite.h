// Conservative proof over the renderer's converted GPU command order.
#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace RT64::RR64FramePacing {
    struct OverwriteTarget {
        uintptr_t identity = 0;
        uint32_t width = 0;
        uint32_t height = 0;

        constexpr bool valid() const {
            return (identity != 0) && (width != 0) && (height != 0);
        }

        constexpr bool operator==(const OverwriteTarget &rhs) const {
            return (identity == rhs.identity) && (width == rhs.width) &&
                (height == rhs.height);
        }
    };

    constexpr bool exactFullOverwriteRect(OverwriteTarget target,
        int32_t left, int32_t top, int32_t right, int32_t bottom)
    {
        return target.valid() && (left == 0) && (top == 0) &&
            (right > 0) && (bottom > 0) &&
            (uint32_t(right) == target.width) &&
            (uint32_t(bottom) == target.height);
    }

    class FrameOverwriteProof {
    public:
        explicit FrameOverwriteProof(OverwriteTarget selected) : selected(selected) {
            accepted = selected.valid();
        }

        void reject() { accepted = false; }

        void clearColor(OverwriteTarget color, bool fullExtent) {
            if (!(color == selected) || (!colorInitialized && !fullExtent)) {
                reject();
            }
            if (fullExtent && (color == selected)) {
                colorInitialized = true;
            }
        }

        void clearDepth(OverwriteTarget depth, bool fullExtent) {
            if (!depth.valid() || (depth.identity == selected.identity)) {
                reject();
                return;
            }
            if (!fullExtent) {
                return;
            }
            for (size_t i = 0; i < depthCount; i++) {
                if (depthTargets[i].identity == depth.identity) {
                    depthTargets[i] = depth;
                    return;
                }
            }
            if (depthCount == depthTargets.size()) {
                reject();
                return;
            }
            depthTargets[depthCount++] = depth;
        }

        void drawColor(OverwriteTarget color, OverwriteTarget depth, bool usesDepth) {
            if (!(color == selected) || !colorInitialized) {
                reject();
            }
            if (usesDepth) {
                bool initialized = false;
                for (size_t i = 0; i < depthCount; i++) {
                    initialized = initialized || (depthTargets[i] == depth);
                }
                if (!depth.valid() || !initialized) {
                    reject();
                }
            }
        }

        bool complete() const { return accepted && colorInitialized; }

    private:
        OverwriteTarget selected;
        std::array<OverwriteTarget, 16> depthTargets{};
        size_t depthCount = 0;
        bool accepted = false;
        bool colorInitialized = false;
    };
}
