#include <array>
#include <cstdint>
#include <cstdlib>
#include <iostream>

#include "contrib/plume/plume_vulkan_swapchain_policy.h"

namespace {
uint32_t checks = 0;

void require(bool condition, const char *message) {
    ++checks;
    if (!condition) {
        std::cerr << "Vulkan swapchain check failed: " << message << '\n';
        std::exit(EXIT_FAILURE);
    }
}
}

int main() {
    using namespace plume::VulkanSwapChainPolicy;

    require(imageCount(3, 2, 0) == 3,
        "an unbounded two-image-minimum surface preserves requested triple buffering");
    require(imageCount(2, 2, 0) == 2,
        "explicit double buffering is retained");
    require(imageCount(3, 2, 2) == 2,
        "a real two-image maximum remains binding");
    require(imageCount(2, 3, 0) == 3,
        "the surface minimum remains binding when there is no maximum");
    require(imageCount(2, 3, 5) == 3,
        "the surface minimum remains binding with a finite maximum");
    require(imageCount(5, 1, 4) == 4,
        "a finite maximum clamps a larger request");

    // Property sweep over valid Vulkan capability ranges. The desired count
    // must be unchanged exactly when it is within the advertised bounds.
    for (uint32_t minimum = 1; minimum <= 4; ++minimum) {
        for (uint32_t maximum = 0; maximum <= 6; ++maximum) {
            if (maximum != 0 && maximum < minimum) {
                continue;
            }
            for (uint32_t requested = 1; requested <= 8; ++requested) {
                const uint32_t selected = imageCount(requested, minimum, maximum);
                require(selected >= minimum, "selected count must satisfy the surface minimum");
                require(maximum == 0 || selected <= maximum,
                    "selected count must satisfy a nonzero surface maximum");
                const bool inRange = requested >= minimum &&
                    (maximum == 0 || requested <= maximum);
                require((selected == requested) == inRange,
                    "buffer count changes only when the request is outside valid bounds");
            }
        }
    }

    // Model the queue IDs actually belonging to each swapchain independently
    // from the helper. Every nonzero wait must refer to a submitted image on
    // that same chain, including recreation after a long running race.
    PresentHistory history;
    constexpr std::array<uint32_t, 5> latencyLimits = {0, 1, 2, 3, 8};
    for (uint32_t generation = 0; generation < 3; ++generation) {
        history.reset();
        for (uint32_t latency : latencyLimits) {
            require(history.waitTarget(latency) == 0,
                "recreation cannot wait for any retired presentation");
        }
        uint64_t newestSubmitted = 0;
        for (uint32_t frame = 0; frame < 512; ++frame) {
            for (uint32_t latency : latencyLimits) {
                const uint64_t target = history.waitTarget(latency);
                require(target <= newestSubmitted,
                    "a wait cannot refer to an unsubmitted image");
                if (target != 0) {
                    const uint64_t queueDepth = newestSubmitted - target + 1;
                    require(queueDepth == (latency == 0 ? 1 : latency),
                        "waiting preserves the requested queue-depth allowance");
                }
            }
            const uint64_t submittedId = history.next();
            require(submittedId == newestSubmitted + 1,
                "IDs start at one and increase monotonically on each new chain");
            newestSubmitted = submittedId;
        }
    }

    std::cout << "RR64 Vulkan swapchain: " << checks << " checks passed\n";
}
