#pragma once
#include "rr64_mk64_item_state.hpp"

namespace rr64::mk64_items {
struct LightningVisual {
    unsigned age = 0;
    unsigned tint = 0xffffff;
    unsigned bolt_alpha = 0;
    bool active = false;
};

// Shrink lasts 300 ticks in the authoritative kernel. Deriving the onset from
// that recorded deadline makes every view/replay agree without a local event
// latch, and an unchanged snapshot cannot restart the effect each frame.
inline LightningVisual lightning_visual(const RiderState &rider, unsigned clock) noexcept {
    if (rider.shrink_until <= clock || rider.shrink_until - clock > 300 ||
        rider.star_until > clock || rider.boo_until > clock)
        return {};
    const unsigned age = 300 - (rider.shrink_until - clock);
    if (age >= 30)
        return {};
    // The donor cycles the struck player's shade every six updates. Retain
    // that cadence in a brief one-second adaptation, without a screen flash
    // or the donor's spin/crash phase. The bolt fades over its first half.
    constexpr unsigned shades[] = {0x080810, 0x505078, 0x909040};
    return {age, shades[(age % 6) / 2], age < 15 ? (15 - age) * 17 : 0, true};
}
} // namespace rr64::mk64_items
