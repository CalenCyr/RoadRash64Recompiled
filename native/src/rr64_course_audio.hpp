#pragma once
#include <cstdint>
#include <span>

namespace rr64::course_audio {
enum class Effect : unsigned {
    ItemBreak, Roulette, ItemChosen, TrainWhistle, TrainDouble,
    CrossingBell, FerryWhistle, FerryDouble, WoodRoll, Impact, Penguin, Mole, BridgeRoll, Count
};
// Loading/selection only. The binary is an optional, identity-checked user-ROM
// asset in the course pack; ordinary builds carry no donor audio bytes.
void load_bank(std::span<const std::uint8_t> bytes);
void unload_bank() noexcept;
void set_wood_surfaces(std::span<const std::uint32_t> ids, bool bridge = false) noexcept;
void reset_runtime() noexcept;
// One game-thread producer, one audio-thread consumer. Updating a nonzero key
// refreshes an existing looping voice; zero-key requests are bounded one-shots.
bool request(Effect, float gain, std::uint32_t key = 0, float pitch = 1) noexcept;
void set_running(bool) noexcept;
void mix(std::span<std::int16_t> stereo, std::uint32_t sample_rate) noexcept;
struct Statistics {
    std::uint64_t submitted = 0, dropped = 0, rendered_frames = 0;
    unsigned active_voices = 0;
};
Statistics statistics() noexcept;
} // namespace rr64::course_audio

extern "C" void rr64_course_audio_step(unsigned char *memory);
