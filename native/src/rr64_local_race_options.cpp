#include "rr64_local_race_options.hpp"
#include "rr64_custom_cop.hpp"
#include "rr64_engine_layout.hpp"
#include "rr64_local_players.hpp"
#include "rr64_netplay.hpp"
#include "librecomp/addresses.hpp"
#include <atomic>
#include <fstream>
#include <algorithm>
#include <cstdio>
#include <cmath>

namespace {
using namespace rr64::engine;
namespace layout = rr64::engine::local_race;
std::atomic<unsigned> saved{0}, revision{0};
unsigned written_revision = 0;
std::filesystem::path settings_path;
bool initialized = false, menu_active = false, restore_choices = true;
// Packed preferences: AI count [0:3], pedestrian density [4:5], bike choice
// [6:8], Custom Cop Mode [9]. Bike choice 0 follows the track; 1..7 use stock indices 0..6.
unsigned race_bike_choice = 0;
void save(unsigned bits) {
    if (saved.exchange(bits, std::memory_order_acq_rel) != bits)
        revision.fetch_add(1, std::memory_order_release);
}
unsigned table_address = 0;
unsigned char *table_owner = nullptr;
unsigned read(unsigned char *rdram, unsigned a) {
    return MEM_W(0, guest_address(a));
}
void write(unsigned char *m, unsigned a, unsigned v) {
    auto *rdram = m;
    MEM_W(0, guest_address(a)) = v;
}
// Recomp heap addresses can be outside the original eight-MiB guest arena.
// Only this owned table and bounded stack text use direct guest access.
void text(unsigned char *rdram, unsigned address, const char *value) {
    // The stock menu's string buffer is sp+0x50..0x67; the next local starts
    // at sp+0x68. Do not overwrite it when adding translated labels later.
    for (unsigned i = 0; i < 23; ++i) {
        MEM_B(i, guest_address(address)) = value[i];
        if (!value[i])
            return;
    }
    MEM_B(23, guest_address(address)) = 0;
}
bool local() {
    return rr64::local_players::active.load(std::memory_order_acquire) &&
           !rr64::netplay::get_status().active;
}
unsigned menu_humans(unsigned char *rdram) {
    const unsigned count = read(rdram, layout::menu_humans);
    return count >= 1 && count <= 4 ? count : read(rdram, layout::humans);
}
unsigned ai_count(unsigned bits, unsigned humans) {
    return std::min(bits & 15u, rr64::local_race_options::max_ai(humans));
}
bool make_table(unsigned char *rdram) {
    if (table_owner != rdram) {
        table_owner = rdram;
        table_address = 0;
    }
    if (!table_address) {
        auto *host = static_cast<unsigned char *>(recomp::alloc(rdram, 11 * 36));
        if (!host)
            return false;
        table_address = 0x80000000u + static_cast<unsigned>(host - rdram);
    }
    // Copy stock records, keeping their colors, font, alignment and animation.
    for (unsigned i = 0; i < 8 * 36; i += 4)
        write(rdram, table_address + i, read(rdram, layout::menu_table + i));
    for (unsigned row : {8u, 9u})
        for (unsigned i = 0; i < 36; i += 4)
            write(rdram, table_address + row * 36 + i,
                  read(rdram, layout::menu_table + 7 * 36 + i));
    for (unsigned i = 0; i < 36; i += 4)
        write(rdram, table_address + 10 * 36 + i, 0);
    // Y is absolute when positive and advances by -Y otherwise. Leave space
    // for all seven option rows, including modes that show the extra stock row.
    // Smaller glyphs with fourteen-pixel line spacing avoid the previous overlap.
    write(rdram, table_address + 8, 0x42600000u); // 56.0f, below the title
    for (unsigned row = 0; row <= 2; ++row) {
        write(rdram, table_address + row * 36 + 0x1C, 0x3F400000u); // 0.75f
        if (row)
            write(rdram, table_address + row * 36 + 8, 0xC1A00000u); // -20.0f
    }
    for (unsigned row = 3; row <= 9; ++row) {
        write(rdram, table_address + row * 36 + 8,
              row == 3 ? 0xC1A00000u : 0xC1600000u);                // heading gap 20, rows 14
        write(rdram, table_address + row * 36 + 0x1C, 0x3F200000u); // 0.625f
    }
    return true;
}
}

namespace rr64::local_race_options {
void initialize(const std::filesystem::path &directory) {
    if (initialized)
        return;
    initialized = true;
    settings_path = directory / "local-race-options.cfg";
    unsigned version = 0, bits = 0;
    std::ifstream file(settings_path);
    if (file >> version >> bits) {
        if (version == 1 && bits <= 15) {
            // Preserve the old Off/full-field and Match Track/Level 1 choices.
            saved.store((bits & 1 ? 10u : 0u) | (((bits >> 1) & 3) << 4) | ((bits & 8) ? 64u : 0u));
            revision.fetch_add(1);
        } else if ((version == 2 || version == 3) && bits <= (version == 2 ? 511u : 1023u) &&
                   (bits & 15) <= 10) {
            saved.store(bits);
        }
    }
}
void flush() {
    const unsigned current = revision.load(std::memory_order_acquire);
    if (!initialized || current == written_revision)
        return;
    std::ofstream file(settings_path, std::ios::trunc);
    file << "3 " << saved.load(std::memory_order_acquire) << '\n';
    file.close();
    if (file)
        written_revision = current;
}
}

// The stock recovery routine also runs a route-boundary timer (+858).
// Local humans may roam while mounted; actual crashes, ejects, busts and
// exhausted durability still need its original recovery/death decisions.
extern "C" int rr64_local_player_roaming(unsigned char *rdram, unsigned actor) {
    if (!local())
        return 0;
    const unsigned humans = read(rdram, layout::humans);
    constexpr unsigned first_actor = 0x800D8570, actor_stride = 0x118;
    if (humans < 1 || humans > 4 || actor < first_actor ||
        (actor - first_actor) % actor_stride != 0 ||
        (actor - first_actor) / actor_stride >= humans)
        return 0;
    const unsigned bike = read(rdram, actor + 0xE0);
    const unsigned rider = read(rdram, actor + 0xE4);
    const unsigned state = read(rdram, actor + 0xE8);
    if (!valid_guest_range(bike, 0x868) || !valid_guest_range(rider, 0x5F0) ||
        !valid_guest_range(state, 0x64))
        return 0;
    std::uint16_t attached = 0, crashing = 0, busted = 0;
    float health = -1;
    read_u16(rdram, rider + 0x57C, attached);
    read_u16(rdram, bike + 0x7F6, crashing);
    read_u16(rdram, state + 0x4C, busted);
    read_float(rdram, bike + 0x4F8, health);
    if (!attached || crashing || busted || !std::isfinite(health) || health < 0)
        return 0;
    write(rdram, bike + 0x858, 0); // Also clear any pending HUD countdown.
    return 1;
}

extern "C" void rr64_local_options_menu_begin(unsigned char *rdram) {
    menu_active = false;
    if (read(rdram, globals::multiplayer_stage) != 1)
        restore_choices = true;
}
extern "C" int rr64_custom_cop_enabled() {
    return local() && (saved.load() & 512u);
}
extern "C" void rr64_local_options_reset_race() {
    race_bike_choice = 0;
    rr64_custom_cop_reset();
}
extern "C" int rr64_local_options_input(unsigned char *rdram) {
    menu_active = local() && make_table(rdram);
    if (!menu_active) {
        restore_choices = true;
        return 0;
    }
    unsigned bits = saved.load(std::memory_order_acquire);
    if (restore_choices) {
        write(rdram, layout::pedestrian_choice, (bits >> 4) & 3);
        write(rdram, layout::menu_dirty, 1);
        restore_choices = false;
    }
    const unsigned row = read(rdram, layout::menu_cursor);
    const unsigned buttons = read(rdram, layout::menu_buttons) & 0x60;
    if ((row == 4 || row == 8 || row == 9) && buttons) {
        const bool increase = (buttons & 0x40) != 0;
        const unsigned maximum = row == 4   ? rr64::local_race_options::max_ai(menu_humans(rdram))
                                 : row == 8 ? 7
                                            : 1;
        unsigned value = row == 4   ? ai_count(bits, menu_humans(rdram))
                         : row == 8 ? (bits >> 6) & 7
                                    : (bits >> 9) & 1;
        value = increase ? (value < maximum ? value + 1 : 0) : (value ? value - 1 : maximum);
        bits = row == 4   ? (bits & ~15u) | value
               : row == 8 ? (bits & ~448u) | (value << 6)
                          : (bits & ~512u) | (value << 9);
        save(bits);
        write(rdram, layout::menu_dirty, 1);
    }
    // Stock code still expects a boolean, never the new AI count. Skip its
    // row-4 input handler so it cannot increment this flag a second time.
    const unsigned enabled = ai_count(bits, menu_humans(rdram)) != 0;
    if (read(rdram, layout::ai_choice) != enabled)
        write(rdram, layout::menu_dirty, 1);
    write(rdram, layout::ai_choice, enabled);
    return row == 4 || row == 8 || row == 9;
}

extern "C" int rr64_local_options_navigation(unsigned char *rdram) {
    if (!menu_active)
        return -1;
    unsigned mask = 3u | (1u << 4) | (1u << 7) | (1u << 8) | (1u << 9);
    for (unsigned row = 2; row < 8; ++row)
        if (MEM_B(row - 2, guest_address(layout::visibility)))
            mask |= 1u << row;
    unsigned row = std::min(read(rdram, layout::menu_cursor), 9u);
    const unsigned buttons = read(rdram, layout::menu_buttons);
    if (buttons & 0x18)
        row = rr64::local_race_options::next_row(row, buttons & 8 ? -1 : 1, mask);
    // Match the stock path's dirty/cursor state without touching controller input.
    write(rdram, layout::menu_cursor, row);
    return static_cast<int>(row);
}
extern "C" unsigned rr64_local_options_table(unsigned stock) {
    return menu_active ? table_address : stock;
}
extern "C" unsigned rr64_local_options_visible(unsigned row, unsigned stock) {
    return menu_active && (row == 4 || row == 7 || row == 8 || row == 9) ? 1u : stock;
}
extern "C" void rr64_local_options_text(unsigned char *rdram, unsigned row, unsigned buffer) {
    if (!menu_active || !valid_guest_range(buffer, 24))
        return;
    char label[24];
    const unsigned bits = saved.load(std::memory_order_acquire);
    if (row == 4) {
        std::snprintf(label, sizeof(label), "AI Racers: %u", ai_count(bits, menu_humans(rdram)));
        text(rdram, buffer, label);
    }
    if (row == 9)
        text(rdram, buffer, bits & 512u ? "Custom Cop Mode: On" : "Custom Cop Mode: Off");
    if (row == 8) {
        const unsigned level = (bits >> 6) & 7;
        if (level == 6)
            text(rdram, buffer, "Bike Level: Scooter");
        else if (level == 7)
            text(rdram, buffer, "Bike Level: Insanity");
        else if (level) {
            std::snprintf(label, sizeof(label), "Bike Level: %u", level);
            text(rdram, buffer, label);
        } else
            text(rdram, buffer, "Bike Level: Match Track");
    }
}
extern "C" void rr64_local_options_finish(unsigned char *rdram) {
    if (!menu_active)
        return;
    const unsigned peds = read(rdram, layout::pedestrian_choice);
    if (peds <= 3)
        save((saved.load() & ~48u) | (peds << 4));
    menu_active = false;
}
extern "C" void rr64_local_options_race(unsigned char *rdram) {
    race_bike_choice = 0;
    rr64_custom_cop_begin(rdram, rr64_custom_cop_enabled());
    if (!local())
        return;
    const unsigned humans = read(rdram, layout::humans);
    if (humans < 1 || humans > 4)
        return;
    const unsigned bits = saved.load(std::memory_order_acquire);
    write(rdram, layout::racers,
          rr64::local_race_options::roster(bits & 15u, humans, read(rdram, layout::racers)));
    race_bike_choice = (bits >> 6) & 7;
    restore_choices = true;
}
extern "C" unsigned rr64_local_bike_level(unsigned original) {
    return race_bike_choice && local() ? race_bike_choice - 1 : original;
}

// The first four profile records are mutable human selections. Keep their
// rider identity and rider statistics; replace only bike identity and its
// physics profile with a stock entry from the selected bike tier.
extern "C" unsigned rr64_local_bike_profile(unsigned char *rdram, unsigned racer,
                                            unsigned profile) {
    if (!race_bike_choice || !local() || racer < 0x800D8570u)
        return profile;
    const unsigned offset = racer - 0x800D8570u;
    if (offset % 0x118 || offset / 0x118 >= 4 || !valid_guest_range(profile, 16))
        return profile;
    if (rr64_custom_cop_active() && read(rdram, racer + 0x18) == 31)
        return profile;
    unsigned donor = 0;
    for (unsigned i = 4; i < 160; ++i) {
        const unsigned entry = 0x800A3460u + i * 16;
        const unsigned tier = static_cast<unsigned char>(MEM_B(8, guest_address(entry)));
        if (tier == 12)
            break;
        if (tier != race_bike_choice || MEM_B(9, guest_address(entry)) == 7)
            continue;
        if (!donor)
            donor = entry;
        if (MEM_B(10, guest_address(entry)) == MEM_B(10, guest_address(profile))) {
            donor = entry;
            break;
        }
    }
    if (!donor)
        return profile;
    if (profile < 0x800A3460u || profile >= 0x800A34A0u)
        return profile;
    // Keep the original selection intact for Match Track and later races.
    static unsigned char *owner = nullptr;
    static unsigned copies = 0;
    if (owner != rdram) {
        owner = rdram;
        copies = 0;
    }
    if (!copies) {
        auto *host = static_cast<unsigned char *>(recomp::alloc(rdram, 4 * 16));
        if (!host)
            return profile;
        copies = 0x80000000u + static_cast<unsigned>(host - rdram);
    }
    const unsigned copy = copies + offset / 0x118 * 16;
    for (unsigned i = 0; i < 16; i += 4)
        write(rdram, copy + i, read(rdram, profile + i));
    MEM_B(10, guest_address(copy)) = MEM_B(10, guest_address(donor));
    MEM_B(13, guest_address(copy)) = MEM_B(13, guest_address(donor));
    write(rdram, racer + 0x18, static_cast<unsigned char>(MEM_B(10, guest_address(donor))));
    return copy;
}
