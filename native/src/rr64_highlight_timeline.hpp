#pragma once
#include "rr64_highlight_recording.hpp"
#include <algorithm>

namespace rr64::highlights {
// Real time is shared online; source motion runs at half speed. Camera cuts
// have no effect on event timing, physical state, or the original race result.
struct Cursor {
    unsigned clip = 0, angle = 0;
    std::uint64_t source_us = 0;
    bool valid = false;
};
inline std::uint64_t duration(std::span<const Clip> clips) noexcept {
    std::uint64_t total = 0;
    for (const auto &clip : clips)
        if (clip.frames.size() >= 2)
            total += 2 * (clip.frames.back().time_us - clip.frames.front().time_us);
    return total;
}
inline Cursor locate(std::span<const Clip> clips, std::uint64_t elapsed_us) noexcept {
    const auto cycle = duration(clips);
    if (!cycle)
        return {};
    // The host clock keeps advancing across repeats; only presentation wraps.
    // Results resume through the authorized A edge, never a playback deadline.
    elapsed_us %= cycle;
    for (unsigned i = 0; i < clips.size(); ++i) {
        const auto &clip = clips[i];
        if (clip.frames.size() < 2)
            continue;
        const auto length = 2 * (clip.frames.back().time_us - clip.frames.front().time_us);
        if (elapsed_us < length) {
            const auto source = clip.frames.front().time_us + elapsed_us / 2;
            // Follow from the side up to impact, then show the flight from
            // the other side; the next clip starts from a different angle.
            return {i, (i + (source >= clip.event_time_us ? 1u : 0u)) % 3u, source, true};
        }
        elapsed_us -= length;
    }
    return {};
}
} // namespace rr64::highlights
