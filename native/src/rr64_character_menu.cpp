#include "rr64_character_preferences.hpp"
#include "rr64_engine_layout.hpp"
#include "rr64_native.hpp"
#include "rr64_netplay.hpp"

namespace {
using namespace rr64;
constexpr unsigned selected = 0x8009F670;
constexpr unsigned explicit_choice = 0x8009EAE8;
constexpr unsigned ready = 0x8009EF2C;

unsigned word(unsigned char* memory, unsigned address) {
    unsigned value = 0;
    engine::read_u32(memory, address, value);
    return value;
}

bool available(unsigned char* memory, unsigned slot, unsigned rider, bool multiplayer) {
    if (!memory || slot >= 4 || rider >= 45) return false;
    // A remembered choice must never unlock a rider or defeat the cop-bike
    // restriction. The original selector still owns preview/model updates.
    if (multiplayer && rr64_custom_cop_enabled() &&
        rr64_custom_cop_bike_entry(memory, slot, 0) == 0x800A684C)
        return rider >= 40;
    std::uint16_t cops_unlocked = 0;
    engine::read_u16(memory, 0x800A77D8, cops_unlocked);
    return rider < 40 || cops_unlocked != 0;
}
}

extern "C" void rr64_character_restore(unsigned char* memory, unsigned slot,
                                         unsigned multiplayer) {
    if (!memory || slot >= 4) return;
    std::uint16_t manual = 0;
    engine::read_u16(memory, explicit_choice + slot * 2, manual);
    if (manual) return; // Includes loaded campaign records and fresh user input.
    const unsigned rider = character_preferences::last(slot);
    if (!available(memory, slot, rider, multiplayer != 0)) return;
    engine::write_u32(memory, selected + slot * 4, rider);
    engine::write_u16(memory, explicit_choice + slot * 2, 1);
}

extern "C" void rr64_character_remember(unsigned char* memory, unsigned slot) {
    if (!memory || slot >= 4) return;
    character_preferences::remember(slot, word(memory, selected + slot * 4));
}

extern "C" void rr64_character_new_campaign(unsigned char* memory) {
    const unsigned rider = character_preferences::last(0);
    if (!available(memory, 0, rider, false)) return;
    // Only called after the two explicit New Game reset paths. Seed the menu,
    // not the campaign record: the original purchase/confirmation still owns
    // the character, bike and money saved in that record. Loaded saves bypass
    // these hooks entirely, including when their rider differs from this one.
    engine::write_u32(memory, selected, rider);
    engine::write_u16(memory, explicit_choice, 1);
}

extern "C" void rr64_character_remember_multiplayer(unsigned char* memory) {
    if (!memory) return;
    const auto status = netplay::get_status();
    unsigned count = word(memory, engine::local_race::menu_humans);
    if (status.active) {
        // Each machine's private online selector is local slot zero. Never
        // save a remote participant copied into guest arrays at race release.
        if (!status.connected || status.phase != netplay::Phase::CharacterSelect) return;
        count = 1;
    }
    if (count < 1 || count > 4) return;
    for (unsigned slot = 0; slot < count; ++slot)
        if (word(memory, ready + slot * 4) == 2)
            rr64_character_remember(memory, slot);
}
