#include "rr64_custom_cop.hpp"
#include "rr64_engine_layout.hpp"
#include "recomp.h"
#include <array>
#include <algorithm>
#include <cmath>
#include <cstring>

extern "C" void func_8006B740(unsigned char *, recomp_context *);
namespace {
using namespace rr64::engine;
struct Post {
    bool placed = false, pursuit = false, muted = true, shout_held = false, hold_toggled = false,
         trick = false;
    float distance = 0, cue = -100, shout_started = 0;
};
std::array<Post, 4> posts;
unsigned word(unsigned char *m, unsigned a) {
    unsigned v = 0;
    read_u32(m, a, v);
    return v;
}
float number(unsigned char *m, unsigned a) {
    float v = 0;
    read_float(m, a, v);
    return v;
}
unsigned bits(float v) {
    unsigned u;
    std::memcpy(&u, &v, 4);
    return u;
}
int slot_for(unsigned char *m, unsigned bike) {
    if (!rr64_custom_cop_active())
        return -1;
    const unsigned humans = word(m, 0x800A6578);
    if (humans > 4)
        return -1;
    for (unsigned i = 0; i < humans; ++i) {
        const unsigned a = 0x800D8570 + i * 0x118;
        if (word(m, a + 0xE0) == bike && word(m, a + 0x20) == 7)
            return int(i);
    }
    return -1;
}
}
// Run after each player's original initialization, before camera preparation.
// Reuse Big Game's route/shoulder placement and its human-safe initialization.
extern "C" void rr64_custom_cop_post(unsigned char *rdram, void *context, unsigned slot) {
    if (slot == 0)
        posts = {};
    if (!context || slot >= 4 || !rr64_custom_cop_active())
        return;
    const unsigned a = 0x800D8570 + slot * 0x118, bike = word(rdram, a + 0xE0);
    if (slot_for(rdram, bike) != int(slot))
        return;
    const unsigned state = word(rdram, a + 0xE8);
    if (!valid_guest_range(state, 0x64))
        return;
    auto call = *static_cast<recomp_context *>(context);
    if (!valid_guest_range(unsigned(call.r29) - 0x400, 0x400))
        return;
    call.r29 -= 0x100;
    std::array<unsigned, 25> saved;
    for (unsigned i = 0; i < saved.size(); ++i)
        saved[i] = word(rdram, state + i * 4);
    const unsigned rank = saved[0x40 / 4];
    if (rank >= 14)
        return;
    const unsigned order = 0x800D77A4 + rank * 8, old_order = word(rdram, order);
    const float length = number(rdram, 0x800D762C);
    if (!std::isfinite(length) || length <= 0)
        return;
    for (unsigned attempt = 0; attempt < 6; ++attempt) {
        const float distance = std::min(200.0f + slot * 40.0f + attempt * 25.0f, length * 0.25f);
        call.r4 = guest_address(a);
        call.r5 = bits(distance);
        func_8006B740(rdram, &call);
        if (call.r2) {
            rr64_custom_cop_equipment(rdram, a);
            posts[slot].placed = true;
            posts[slot].distance = number(rdram, order);
            return;
        }
        // The original routine updates route state before rejecting unsuitable road.
        // Restore it on failure; never leave a grid rider with a distant route index.
        for (unsigned i = 0; i < saved.size(); ++i)
            write_u32(rdram, state + i * 4, saved[i]);
        write_u32(rdram, order, old_order);
    }
}
// Read-only with respect to engine input. Once a racer passes the posted route
// position, this cop remains in pursuit until the next race initialization.
extern "C" int rr64_custom_cop_pursuit(unsigned char *m, unsigned bike) {
    const int slot = slot_for(m, bike);
    if (slot < 0)
        return -1;
    auto &p = posts[slot];
    if (!p.placed)
        return 0;
    const unsigned total = word(m, 0x800A656C);
    if (total > 14)
        return 0;
    for (unsigned i = 0; i < total && !p.pursuit; ++i) {
        const unsigned a = 0x800D8570 + i * 0x118, state = word(m, a + 0xE8);
        if (word(m, a + 0x20) == 7 || !valid_guest_range(state, 0x64))
            continue;
        const float progress =
            number(m, state + 0x20) + number(m, state + 8) * number(m, state + 0xC);
        std::uint16_t eligible = 0;
        read_u16(m, state + 0x48, eligible);
        if (eligible && std::isfinite(progress) && progress > p.distance) {
            p.pursuit = true;
            p.cue = number(m, 0x800D7670);
        }
    }
    return p.pursuit ? 1 : 0;
}
// The stock parked-cop filter uses brake and zero drive values. Apply only
// while posted, then release immediately when a racer passes this officer.
extern "C" void rr64_custom_cop_control(unsigned char *rdram, void *context) {
    if (!context || !rr64_custom_cop_active())
        return;
    auto &c = *static_cast<recomp_context *>(context);
    // Guest ABI: a1 is newly pressed, a2 is held (80040968/8004096C).
    const unsigned a = unsigned(c.r4), bike = word(rdram, a + 0xE0);
    const int slot = slot_for(rdram, bike);
    if (slot < 0) {
        if (c.r6 & 4) {
            c.r6 = (c.r6 & ~4) | 0x20;
            if (c.r5 & 4)
                c.r5 = (c.r5 & ~4) | 0x20;
        }
        return;
    }
    auto &p = posts[slot];
    p.trick = (c.r6 & 0x4) != 0;
    const bool trick_pressed = (c.r5 & 0x4) != 0;
    c.r6 &= ~0x4;
    c.r5 &= ~0x4;
    // Delay the stock shout until release so a long press never also shouts.
    const float now = number(rdram, 0x800A1820);
    const bool held = (c.r6 & 0x20) != 0;
    c.r6 &= ~0x20;
    c.r5 &= ~0x20;
    if (held && !p.shout_held) {
        p.shout_started = now;
        p.hold_toggled = false;
    }
    const float duration = now - p.shout_started;
    // Toggle once per hold; release only rearms the next gesture.
    if (held && !p.hold_toggled && duration >= 1.0f) {
        p.muted = !p.muted;
        p.hold_toggled = true;
    }
    if (!held && p.shout_held && !p.hold_toggled && duration >= 0 && duration < 1.0f) {
        c.r6 |= 0x20;
        c.r5 |= 0x20;
    }
    p.shout_held = held;
    if (p.trick) {
        c.r6 |= 0x20;
        if (trick_pressed)
            c.r5 |= 0x20;
    }
    const int pursuit = rr64_custom_cop_pursuit(rdram, bike);
    if (p.placed) {
        MEM_H(0x81C, guest_address(bike)) = pursuit == 0 ? 1 : 0;
        if (pursuit == 0) {
            c.r6 = (c.r6 & 0x20) | 0x10;
            c.r5 = (c.r5 & 0x20) | 0x10;
            c.r7 = 0;
            MEM_W(16, c.r29) = 0;
            const unsigned axes = word(rdram, unsigned(c.r29) + 20);
            if (valid_guest_range(axes, 12))
                for (unsigned j = 0; j < 3; ++j)
                    write_u32(rdram, axes + j * 4, 0);
        }
    }
}
extern "C" int rr64_custom_cop_siren(unsigned char *m, unsigned bike) {
    const int slot = slot_for(m, bike);
    return slot < 0 ? -1 : posts[slot].muted ? 0 : 1;
}
extern "C" float rr64_custom_cop_cue(unsigned char *m, unsigned slot) {
    if (slot >= 4 || !rr64_custom_cop_active() || !posts[slot].pursuit)
        return -1;
    const float age = number(m, 0x800D7670) - posts[slot].cue;
    return age >= 0 && age < 4 ? age : -1;
}

// Let the native weapon flourish execute for cops instead of its cop-shout branch.
extern "C" int rr64_custom_cop_trick(unsigned char *m, unsigned actor) {
    const int slot = slot_for(m, word(m, actor + 0xE0));
    return slot >= 0 && posts[slot].trick;
}

// Cops are excluded from race placement (state+48 == 0), but a human cop
// with usable health still needs the ordinary recovery path. Do not change
// that flag globally: doing so would add cops to the remaining-racer count.
// Other stock relocation triggers (including detached out-of-terrain bodies)
// can reach this branch early. Do not grant the custom role recovery until the
// ordinary crash clock passes the stock threshold used at 8003FFC8.
extern "C" int rr64_custom_cop_can_recover(unsigned char *m, unsigned actor) {
    const unsigned bike = word(m, actor + 0xE0);
    if (slot_for(m, bike) < 0)
        return 0;
    const unsigned state = word(m, actor + 0xE8);
    if (!valid_guest_range(state, 0x64) || !valid_guest_range(bike, 0x868) ||
        word(m, state + 0x4C) != 0)
        return 0;
    const float health = number(m, bike + 0x4F8);
    const float crash_age = number(m, bike + 0x4CC);
    const float stock_delay = number(m, 0x800048EC);
    return std::isfinite(health) && health >= 0 && std::isfinite(crash_age) &&
           std::isfinite(stock_delay) && stock_delay > 0 && crash_age > stock_delay;
}

// Route separation must not relocate a healthy, mounted human cop. Keep the
// stock recovery/death path once an actual crash/eject or depleted health occurs.
extern "C" int rr64_custom_cop_roaming(unsigned char *m, unsigned actor) {
    const unsigned bike = word(m, actor + 0xE0);
    if (slot_for(m, bike) < 0 || !valid_guest_range(bike, 0x868))
        return 0;
    const unsigned rider = word(m, actor + 0xE4);
    if (!valid_guest_range(rider, 0x5F0))
        return 0;
    std::uint16_t attached = 0, crashing = 0;
    read_u16(m, rider + 0x57C, attached);
    read_u16(m, bike + 0x7F6, crashing);
    return attached && !crashing && number(m, bike + 0x4F8) >= 0;
}
