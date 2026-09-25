#pragma once
#include <cstdint>
#include <span>
#include <string_view>

namespace rr64::course_music {
// Optional private ROM-derived bank. Loading/validation allocates here, never
// in mix. Malformed input throws std::runtime_error; no partial bank publishes.
inline constexpr std::size_t kMaximumBankBytes = 128u * 1024u * 1024u;
void load_bank(std::span<const std::uint8_t> bytes);
void unload_bank() noexcept;
void select_course(std::string_view id) noexcept;
void set_enabled(bool enabled) noexcept;
void set_active(bool active) noexcept;
void reset_runtime() noexcept;
bool available() noexcept;
// Called before the stock/custom music mixer. This gain ducks music only;
// course effects, engine/crash sounds and voice chat retain their own volume.
float replacement_gain() noexcept;
void mix(std::span<std::int16_t> stereo, std::uint32_t sample_rate,
         float music_volume = 1.0f) noexcept;
struct Statistics {
    std::uint64_t frames = 0;
    std::uint64_t loops = 0;
    std::uint32_t song_id = 0;
    float gain = 0;
};
Statistics statistics() noexcept;
}
