#include "rr64_custom_cop.hpp"
#include "rr64_custom_cop_rules.hpp"
#include "rr64_engine_layout.hpp"
#include "rr64_local_players.hpp"
#include "rr64_netplay.hpp"
#include <cstring>
#include <cmath>

namespace {
using namespace rr64::engine;
constexpr unsigned actors = 0x800D8570, stride = 0x118;
bool race_enabled = false;
float win_started = -1;
unsigned initial_cops = 0, initial_racers = 0;
bool local() {
    return rr64::local_players::active.load() && !rr64::netplay::get_status().active;
}
unsigned word(unsigned char *m, unsigned address) {
    unsigned value = 0;
    read_u32(m, address, value);
    return value;
}
bool actor(unsigned char *m, unsigned address) {
    const unsigned count = word(m, 0x800A656C);
    return count <= 14 && address >= actors && (address - actors) % stride == 0 &&
           (address - actors) / stride < count;
}
}

extern "C" int rr64_custom_cop_active() {
    return race_enabled && local();
}
extern "C" void rr64_custom_cop_reset() {
    race_enabled = false;
    win_started = -1;
    initial_cops = initial_racers = 0;
}
extern "C" void rr64_custom_cop_begin(unsigned char *m, int enabled) {
    rr64_custom_cop_reset();
    const unsigned humans = word(m, 0x800A6578);
    race_enabled = enabled && local() && humans >= 1 && humans <= 4;
}
extern "C" unsigned rr64_custom_cop_bike_count(unsigned stock) {
    return rr64_custom_cop_enabled() && stock >= 1 && stock <= 4 ? stock + 1 : stock;
}
extern "C" int rr64_custom_cop_can_start(unsigned char *m) {
    if (!rr64_custom_cop_enabled())
        return 1;
    const unsigned humans = word(m, 0x8009EF5C);
    if (humans < 1 || humans > 4)
        return 0;
    for (unsigned i = 0; i < humans; ++i)
        if (rr64::custom_cop::selected_cop(true, word(m, 0x8009F660 + i * 4),
                                           word(m, 0x8009F400 + i * 4)))
            return 1;
    return 0;
}
extern "C" int rr64_custom_cop_confirm(unsigned char *m) {
    if (rr64_custom_cop_can_start(m))
        return 1;
    const unsigned humans = word(m, 0x8009EF5C);
    // Reopen each ready selector, keeping equipment intact. No synthetic
    // button presses: the player must confirm again after choosing a cop.
    if (humans >= 1 && humans <= 4)
        for (unsigned i = 0; i < humans; ++i)
            write_u32(m, 0x8009EF2C + i * 4, 1);
    return 0;
}
extern "C" unsigned rr64_custom_cop_bike_entry(unsigned char *m, unsigned player, unsigned stock) {
    if (!rr64_custom_cop_enabled() || player >= 4)
        return stock;
    const unsigned level = word(m, 0x800A6690);
    if (level >= 15)
        return stock;
    const unsigned count = word(m, 0x800A66B8 + level * 4);
    // Append the original cop record without altering the game's unlock tables.
    return count >= 1 && count <= 4 && word(m, 0x8009EF3C + player * 4) == count ? 0x800A684C
                                                                                 : stock;
}
extern "C" unsigned rr64_custom_cop_rider(unsigned char *rdram, unsigned player,
                                          unsigned selected) {
    if (!rr64_custom_cop_enabled() || player >= 4)
        return selected;
    if (rr64_custom_cop_bike_entry(rdram, player, 0) == 0x800A684C) {
        // Preserve all five stock police profiles, including their model and
        // voice identities. The stock decrement reaches 39; increment wraps
        // 44 to 0. Normalize those endpoints without writing unlock flags.
        if (selected >= 40 && selected <= 44)
            return selected;
        return selected == 39 ? 44 : 40;
    }
    if (MEM_HU(0, guest_address(0x800A77D8)))
        return selected;
    return selected <= 39 ? selected : 0;
}
extern "C" void rr64_custom_cop_roles(unsigned char *m) {
    if (!rr64_custom_cop_active())
        return;
    const unsigned humans = word(m, 0x800A6578), total = word(m, 0x800A656C);
    if (humans < 1 || humans > 4 || total < humans || total > 14)
        return;
    initial_cops = 0;
    for (unsigned i = 0; i < humans; ++i) {
        const unsigned a = actors + i * stride;
        const unsigned bike = word(m, a + 0x18), rider = word(m, a + 0x1C);
        const bool cop = rr64::custom_cop::selected_cop(true, bike, rider);
        // Run before the stock eligibility/count/model initialization. Leave
        // controller, viewport, profile ownership and AI flags untouched.
        write_u32(m, a + 0x20, cop ? 7 : bike < 12 ? 5 : bike < 24 ? 6 : 8);
        initial_cops += cop;
    }
    initial_racers = total - initial_cops;
}
extern "C" int rr64_custom_cop_arrest(unsigned char *m, unsigned attacker, unsigned victim) {
    if (!rr64_custom_cop_active() || attacker == victim || !actor(m, attacker) || !actor(m, victim))
        return 0;
    return word(m, attacker + 0x20) == 7 && word(m, victim + 0x20) != 7;
}
// A zero remaining count can also mean racers escaped or crashed out.
// Announce victory only when every opposing actor has the stock busted flag.
extern "C" float rr64_custom_cop_win_age(unsigned char *m) {
    if (!rr64_custom_cop_active() || !initial_cops || !initial_racers)
        return -1;
    const unsigned total = word(m, 0x800A656C);
    if (total > 14)
        return -1;
    unsigned busted = 0;
    for (unsigned i = 0; i < total; ++i) {
        const unsigned a = actors + i * stride;
        if (word(m, a + 0x20) == 7)
            continue;
        const unsigned state = word(m, a + 0xE8);
        std::uint16_t flag = 0;
        if (!valid_guest_range(state, 0x64) || !read_u16(m, state + 0x4C, flag) || !flag)
            return -1;
        ++busted;
    }
    if (busted != initial_racers)
        return -1;
    float now = 0;
    read_float(m, 0x800D7670, now);
    if (!std::isfinite(now))
        return -1;
    if (win_started < 0)
        win_started = now;
    return now - win_started;
}
extern "C" int rr64_custom_cop_finished(unsigned char *m) {
    if (!rr64_custom_cop_active() || !initial_cops || !initial_racers)
        return 0;
    const float win_age = rr64_custom_cop_win_age(m);
    // Let the announcement animate before the original results transition.
    if (win_age >= 0)
        return win_age >= 3.0f;
    return word(m, 0x800D764C) == 0;
}

extern "C" void rr64_custom_cop_equipment(unsigned char *rdram, unsigned a) {
    if (!rr64_custom_cop_active() || !actor(rdram, a) || word(rdram, a + 0x20) != 7)
        return;
    const unsigned bike = word(rdram, a + 0xE0), rider = word(rdram, a + 0xE4);
    if (!valid_guest_range(bike, 0x868) || !valid_guest_range(rider, 0x5F0))
        return;
    // All stock category-7 profiles select weapon 5. Initialize inventory and
    // selection before the original weapon/model setup call at 8006D704.
    // Keep fists (slot 1), but remove all other starting weapons.
    for (unsigned weapon = 2; weapon < 15; ++weapon)
        MEM_H(0x838 + weapon * 2, guest_address(bike)) = weapon == 5 ? 1 : 0;
    write_u32(rdram, rider + 0x5B0, 5);
}
extern "C" void rr64_custom_cop_spawn(unsigned char *rdram, unsigned slot, unsigned offsets) {
    if (!rr64_custom_cop_active() || slot >= word(rdram, 0x800A6578) || slot >= 4 ||
        !valid_guest_range(offsets, 8))
        return;
    const unsigned a = actors + slot * stride;
    if (word(rdram, a + 0x20) != 7)
        return;
    unsigned rank = 0;
    for (unsigned i = 0; i < slot; ++i)
        rank += word(rdram, actors + i * stride + 0x20) == 7;
    // Stock grid rows advance backwards by seven units. Place cops two rows
    // ahead on the shoulder, spaced apart. The existing track-start rotation,
    // world placement and bike/rider synchronization still handle the result.
    const float ahead = 14.0f + 7.0f * rank, shoulder = 7.5f;
    unsigned bits;
    std::memcpy(&bits, &ahead, 4);
    write_u32(rdram, offsets, bits);
    std::memcpy(&bits, &shoulder, 4);
    write_u32(rdram, offsets + 4, bits);
}
extern "C" void rr64_custom_cop_bust_message(unsigned char *rdram, unsigned attacker,
                                             unsigned victim) {
    if (!rr64_custom_cop_arrest(rdram, attacker, victim))
        return;
    const unsigned state = word(rdram, victim + 0xE8);
    if (!valid_guest_range(state, 0x64) || !MEM_HU(0x4C, guest_address(state)))
        return;
    const auto message = [&](unsigned a, unsigned name, unsigned offset, const char *prefix) {
        // The stock dispatcher uses a 32-byte attacker text field and a
        // separate victim field, sharing the normal message lifetime timer.
        const unsigned hud = a + 0x88;
        if (!MEM_HU(0, guest_address(hud)))
            return;
        char label[32]{};
        unsigned at = 0;
        while (*prefix && at < 30)
            label[at++] = *prefix++;
        for (unsigned i = 0; i < 12 && at < 30; ++i) {
            const char c = MEM_B(i, guest_address(name));
            if (!c)
                break;
            label[at++] = c;
        }
        label[at++] = '!';
        for (unsigned i = 0; i < 32; ++i)
            MEM_B(offset + i, guest_address(hud)) = label[i];
        write_u32(rdram, hud + 0x50, word(rdram, 0x80006568));
    };
    message(attacker, victim + 0xC, 0xA, "Busted ");
    message(victim, attacker + 0xC, 0x2A, "Busted by ");
}

extern "C" void rr64_custom_cop_notification(unsigned char *m, unsigned stats, unsigned event,
                                             unsigned name) {
    // KO/crash callers enqueue their ordinary text after awarding the bust.
    // Restore the bust wording only for that same arrested pair, rather than
    // having the following stock notification immediately overwrite it.
    if (!rr64_custom_cop_active() || event < 3 || event > 22 || stats < 0x2C || name < 0xC)
        return;
    const unsigned recipient = stats - 0x2C, other = name - 0xC;
    if (!actor(m, recipient) || !actor(m, other))
        return;
    if (word(m, recipient + 0x20) == 7)
        rr64_custom_cop_bust_message(m, recipient, other);
    else if (word(m, other + 0x20) == 7)
        rr64_custom_cop_bust_message(m, other, recipient);
}
