#pragma once

#include "rr64_mk64_item_native.hpp"
#include "rr64_mk64_item_state.hpp"

namespace rr64::mk64_items {
struct ReplayActor {
    native::Pair owner{};
    native::Geometry bike_geometry{}, rider_geometry{};
    bool operator==(const ReplayActor &) const = default;
};
struct ReplayState {
    Snapshot state{};
    std::array<ReplayActor, racer_capacity> actors{};
    std::uint64_t elapsed_us = 0;
    bool operator==(const ReplayState &) const = default;
};
// Private simulation binds a copy scoped to its memory and ReplayScope epoch.
// These APIs never step inventory/projectiles, consume input, or emit effects.
bool capture_replay(ReplayState &) noexcept;
bool bind_replay(unsigned char *, const ReplayState &) noexcept;
bool replay_state(ReplayState &) noexcept;
bool correct_replay(unsigned char *, ReplayState &, const Snapshot &) noexcept;
void finish_replay(std::uint32_t duration_us) noexcept;
} // namespace rr64::mk64_items
