#pragma once
#include <cstdint>

namespace RT64::RR64Video {
inline float sourceAspect(bool combined, std::uint32_t width, std::uint32_t height) {
    // The VI rescales High Res's non-square console pixels onto a 4:3 TV.
    return combined || height == 0u ? 4.0f / 3.0f : float(width) / float(height);
}
inline constexpr float combinedTarget = 16.0f / 9.0f;
}
