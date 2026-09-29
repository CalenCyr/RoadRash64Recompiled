#pragma once
#include "rr64_mk64_item_state.hpp"
#include <span>
#include <string>

namespace rr64::mk64_items {
enum class Sound : unsigned {
    Shell,
    ShellMotion,
    Banana,
    Mushroom,
    Lightning,
    BooStart,
    BooLoop,
    StarLoop,
    StarDistant,
    Warning,
    LightningImpact,
    Hit,
    StarMusic,
    Count
};
struct AudioRider {
    Vec position{};
    std::uint32_t identity = 0;
    bool local = false;
};
struct AudioListener {
    Vec position{}, right{1, 0, 0};
    unsigned rider = 0;
};
bool install_audio_bank(std::span<const std::uint8_t>, std::string &error);
void clear_audio_bank() noexcept;
bool audio_available() noexcept;
void reset_audio() noexcept;
// One game-thread producer. A fresh identity establishes an event baseline;
// duplicate authority snapshots never replay one-shots. Pause freezes voices,
// while highlights terminate audio and never replay item events.
void present_audio(const Snapshot &, std::span<const AudioRider>, std::span<const AudioListener>,
                   bool running, bool highlights) noexcept;
// Called after the original channel-order conversion. This mixer has its own
// bounded voices and cannot claim or steal an original engine/crash voice.
void mix_audio(std::span<std::int16_t> stereo, unsigned rate, float sfx_volume,
               float music_volume) noexcept;
// Smooth music-only attenuation. Apply to stock/custom music and course music,
// never to engine sounds, crashes, course effects, item SFX or voice chat.
float item_music_gain() noexcept;
struct AudioStatistics {
    std::uint64_t submitted = 0, dropped = 0, rendered = 0;
    unsigned voices = 0;
};
AudioStatistics audio_statistics() noexcept;
} // namespace rr64::mk64_items

extern "C" void rr64_mk64_item_audio_step(unsigned char *memory);
