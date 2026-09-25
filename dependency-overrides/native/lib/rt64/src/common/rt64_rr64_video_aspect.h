#pragma once
#include <cmath>
#include <cstdint>

namespace RT64::RR64Video {
inline float sourceAspect(bool combined, std::uint32_t width, std::uint32_t height) {
    // The VI rescales High Res's non-square console pixels onto a 4:3 TV.
    return combined || height == 0u ? 4.0f / 3.0f : float(width) / float(height);
}
inline constexpr float combinedTarget = 16.0f / 9.0f;

inline bool adjustPairAspect(bool liveRace, bool ownedMenu, float scissorWidth,
    float scissorHeight, float physicalAspect, std::uint32_t framebufferWidth,
    std::uint32_t viWidth, std::uint32_t viHeight) {
    if (liveRace) return true;
    if (scissorWidth <= 0 || scissorHeight <= 0 || physicalAspect <= 0) return false;
    // The pair's scissor is in framebuffer pixels, not television coordinates.
    // High Res pixels are non-square: comparing 512x240 to physical 4:3 rejects
    // the correction and stretches menu labels behind the centered menu clip.
    // Use the submitted VI's pixel ratio only for its own menu output width.
    // Offscreen targets and the legacy presentation path retain the heuristic.
    const float reference = ownedMenu && viWidth && viHeight && framebufferWidth == viWidth
        ? float(viWidth) / float(viHeight) : physicalAspect;
    return std::abs(scissorWidth / scissorHeight / reference - 1.0f) < 0.1f;
}
}
