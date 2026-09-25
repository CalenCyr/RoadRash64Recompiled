//
// plume
//
// Copyright (c) 2024 renderbag and contributors. All rights reserved.
// Licensed under the MIT license. See LICENSE file for details.
//

#pragma once

#include <algorithm>
#include <cstdint>

namespace plume::VulkanSwapChainPolicy {
    // VkSurfaceCapabilitiesKHR::maxImageCount == 0 means unbounded, not
    // "use the minimum". In particular, keep triple buffering when the
    // surface reports a minimum of two images and no maximum.
    constexpr uint32_t imageCount(uint32_t requested, uint32_t minimum, uint32_t maximum) {
        const uint32_t count = std::max(requested, minimum);
        return maximum == 0 ? count : std::min(count, maximum);
    }

    // Present IDs are scoped to one VkSwapchainKHR. A recreated swapchain
    // starts at zero even if its window and application object are unchanged.
    // Waiting for an ID inherited from the retired chain can stall until the
    // driver's timeout because that image was never queued on the new chain.
    class PresentHistory {
    public:
        constexpr void reset() {
            current = 0;
        }

        constexpr uint64_t next() {
            return ++current;
        }

        constexpr uint64_t waitTarget(uint32_t maximumLatency) const {
            // Zero is not a usable queue-depth limit. Treat it as the
            // smallest limit rather than underflowing the target ID.
            const uint32_t latency = std::max(maximumLatency, uint32_t(1));
            return current >= latency ? current - (latency - 1) : 0;
        }

    private:
        uint64_t current = 0;
    };
}
