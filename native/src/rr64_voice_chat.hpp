#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <span>

namespace rr64::voice_chat {

void set_enabled(bool enabled);
bool enabled();

// Called by the direct-connect service thread. Capture and decoding are kept
// away from the game's render and audio producer threads.
void update();
void shutdown();

// Adds decoded proximity speech to the game's interleaved stereo stream.
void mix(std::span<std::int16_t> stereo_samples, std::uint32_t output_rate);

// Public for deterministic ROM-free validation of the spatial policy.
inline float proximity_gain(float distance) {
    constexpr float full_volume_distance = 90.0f;
    constexpr float silent_distance = 1000.0f;
    if (!std::isfinite(distance) || distance >= silent_distance) {
        return 0.0f;
    }
    if (distance <= full_volume_distance) {
        return 1.0f;
    }
    const float t = std::clamp(
        (distance - full_volume_distance) / (silent_distance - full_volume_distance),
        0.0f,
        1.0f);
    const float smooth = t * t * (3.0f - 2.0f * t);
    return 1.0f - smooth;
}

} // namespace rr64::voice_chat
