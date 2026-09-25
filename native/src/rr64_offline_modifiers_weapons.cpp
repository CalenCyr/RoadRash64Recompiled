#include "rr64_offline_modifiers_weapons.hpp"
#include "rr64_engine_layout.hpp"
#include "rr64_netplay.hpp"
#include "rr64_prediction_replay.hpp"

extern "C" void func_80037920(unsigned char *, recomp_context *);

namespace rr64::offline_modifiers {
bool grant_max_weapons(unsigned char *memory, unsigned actor, bool enabled) {
    using namespace engine;
    constexpr unsigned actors = 0x800D8570, actor_stride = 0x118;
    constexpr unsigned inventory = 0x838, first_weapon = 2, last_weapon = 14, maximum = 4;
    if (!enabled || !memory || prediction::active() || netplay::get_physics_rules().active ||
        actor < actors || (actor - actors) % actor_stride)
        return false;
    unsigned humans = 0, category = 0, model = 0, bike_address = 0, rider_address = 0;
    const unsigned slot = (actor - actors) / actor_stride;
    if (!read_u32(memory, local_race::humans, humans) || !humans || humans > 4 || slot >= humans ||
        !read_u32(memory, actor + 0x20, category) || category == 7 ||
        !read_u32(memory, actor + 0x18, model) || model == 31 ||
        !read_u32(memory, actor + 0xE0, bike_address) ||
        !read_u32(memory, actor + 0xE4, rider_address) ||
        !valid_guest_range(bike_address, bike::stride) ||
        !valid_guest_range(rider_address, rider::stride))
        return false;
    unsigned linked_rider = 0, linked_bike = 0, selected = 0;
    if (!read_u32(memory, bike_address + bike::rider_pointer, linked_rider) ||
        !read_u32(memory, rider_address + rider::bike_pointer, linked_bike) ||
        linked_rider != rider_address || linked_bike != bike_address ||
        !read_u32(memory, rider_address + rider::selected_weapon, selected) ||
        selected < rider::fists_weapon || selected > last_weapon)
        return false;
    std::array<std::uint16_t, last_weapon + 1> quantities{};
    for (unsigned weapon = first_weapon; weapon <= last_weapon; ++weapon)
        if (!read_u16(memory, bike_address + inventory + weapon * 2, quantities[weapon]) ||
            quantities[weapon] > maximum)
            return false; // Validate the entire transaction before any grant.

    // Native37920 is a leaf: it reads rider+584, increments one inventory
    // halfword up to four, and selects that weapon. It needs no guest stack.
    recomp_context call{};
    call.f_odd = &call.f0.u32h;
    for (unsigned weapon = first_weapon; weapon <= last_weapon; ++weapon)
        for (unsigned quantity = quantities[weapon]; quantity < maximum; ++quantity) {
            call.r4 = guest_address(rider_address);
            call.r5 = weapon;
            func_80037920(memory, &call);
        }
    // Granting the final entry must not silently switch the player's weapon.
    write_u32(memory, rider_address + rider::selected_weapon, selected);
    return true;
}
}
