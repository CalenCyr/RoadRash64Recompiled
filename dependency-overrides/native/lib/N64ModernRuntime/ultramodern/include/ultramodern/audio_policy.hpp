#pragma once

#include <cstdint>

namespace ultramodern {

// Road Rash produces one audio chunk per 60 Hz video interval, while common
// WASAPI devices request 1024 frames at a time. Runtime traces showed that a
// two-interval reserve could still land exactly at an empty queue when a host
// request followed a 1024-frame boundary. Three intervals (2205 frames at
// 44.1 kHz) keep one additional 736-frame source block in reserve without
// modifying the game's mixer or sample stream.
inline constexpr std::uint32_t kAudioReserveVideoIntervals = 3u;
inline constexpr std::uint32_t kAudioVideoRate = 60u;
inline constexpr std::uint32_t kAudioOutputChannels = 2u;

constexpr std::uint32_t audio_reserve_frames(std::uint32_t sample_rate) {
    return (sample_rate / kAudioVideoRate) * kAudioReserveVideoIntervals;
}

constexpr std::uint32_t audio_startup_prebuffer_frames(
    std::uint32_t sample_rate,
    std::uint32_t host_period_frames)
{
    const std::uint32_t reserve_frames = audio_reserve_frames(sample_rate);
    const std::uint32_t two_host_periods = host_period_frames * 2u;
    return reserve_frames > two_host_periods ? reserve_frames : two_host_periods;
}

constexpr std::uint32_t audio_remaining_bytes_for_game(
    std::uint32_t actual_buffered_frames,
    std::uint32_t sample_rate)
{
    const std::uint32_t reserve_frames = audio_reserve_frames(sample_rate);
    const std::uint32_t reportable_frames =
        actual_buffered_frames > reserve_frames ? actual_buffered_frames - reserve_frames : 0u;
    return reportable_frames * kAudioOutputChannels * sizeof(std::int16_t);
}

} // namespace ultramodern
