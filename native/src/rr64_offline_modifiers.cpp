#include "rr64_offline_modifiers.hpp"
#include "rr64_offline_modifiers_bikes.hpp"
#include "rr64_offline_modifiers_health.hpp"
#include "rr64_offline_modifiers_weapons.hpp"
#include "rr64_engine_layout.hpp"
#include "rr64_highlights.hpp"
#include "rr64_netplay.hpp"
#include "rr64_prediction_replay.hpp"

#include <array>
#include <atomic>
#include <cstdint>

namespace rr64::offline_modifiers {
namespace {
std::atomic<unsigned> requested{0};
std::atomic<std::uint64_t> weapon_epoch{1};
std::atomic<std::uint64_t> bike_epoch{1};
std::uint64_t applied_bike_epoch = 1;
unsigned char *live_memory = nullptr;
std::array<std::uint64_t, engine::kMaximumRacers> granted{};
constexpr unsigned actors = 0x800D8570u, actor_stride = 0x118u;
constexpr unsigned known_flags = 15;

bool offline() {
    // Reject the entire online session, including lobby, failed connect and
    // disconnect handling. Stored preferences are never part of session rules.
    return !prediction::active() && !netplay::get_physics_rules().active;
}
bool own_session(unsigned char *memory) {
    return memory && memory == live_memory && rr64_player_session_started() && offline();
}
bool live_race(unsigned char *memory) {
    unsigned mode = 0, pending = 0;
    return own_session(memory) && engine::read_u32(memory, engine::globals::main_mode, mode) &&
           engine::read_u32(memory, engine::globals::pending_mode, pending) &&
           engine::is_live_race_transition(mode, pending) && !rr64_highlights_presenting();
}
void grant_once(unsigned char *memory, unsigned actor) {
    if (actor < actors || (actor - actors) % actor_stride)
        return;
    const unsigned slot = (actor - actors) / actor_stride;
    if (slot >= granted.size())
        return;
    const auto epoch = weapon_epoch.load(std::memory_order_acquire);
    if (granted[slot] != epoch && grant_max_weapons(memory, actor, true))
        granted[slot] = epoch;
}
}

void set_requested(Flag flag, bool value) {
    const unsigned bit = static_cast<unsigned>(flag);
    if (!(bit & known_flags) || (bit & (bit - 1)))
        return;
    const unsigned previous = value ? requested.fetch_or(bit, std::memory_order_acq_rel)
                                    : requested.fetch_and(~bit, std::memory_order_acq_rel);
    if (flag == Flag::AllWeapons && value && !(previous & bit))
        weapon_epoch.fetch_add(1, std::memory_order_release);
    if (flag == Flag::AllBikes && ((previous & bit) != 0) != value)
        bike_epoch.fetch_add(1, std::memory_order_release);
}
bool enabled(Flag flag) {
    return (requested.load(std::memory_order_acquire) & static_cast<unsigned>(flag)) && offline();
}
bool enabled_any() {
    return requested.load(std::memory_order_acquire) && offline();
}
void reset(unsigned char *memory) {
    live_memory = memory;
    granted.fill(0);
    applied_bike_epoch = bike_epoch.load(std::memory_order_acquire);
    rr64_offline_bikes_reset();
}
}

extern "C" void rr64_offline_modifiers_race_begin(unsigned char *memory) {
    using namespace rr64::offline_modifiers;
    if (memory == live_memory && !rr64::prediction::active())
        granted.fill(0);
}
extern "C" void rr64_offline_modifiers_equipment(unsigned char *memory, unsigned actor) {
    using namespace rr64::offline_modifiers;
    // This hook follows native/Custom Cop equipment and reciprocal rider-bike
    // link construction. Loading is not a live-race mode.
    if (enabled(Flag::AllWeapons) && own_session(memory))
        grant_once(memory, actor);
}
extern "C" void rr64_offline_modifiers_frame(unsigned char *memory) {
    using namespace rr64::offline_modifiers;
    const auto menu_epoch = bike_epoch.load(std::memory_order_acquire);
    if (applied_bike_epoch != menu_epoch && own_session(memory)) {
        unsigned mode = 0;
        rr64::engine::read_u32(memory, rr64::engine::globals::main_mode, mode);
        // Native 2F150 dispatches the selectors at 21/23. Their idle path
        // skips cursor clamping unless dirty; shops 2D/2E clamp every tick.
        if (mode == 0x21 || mode == 0x23) {
            rr64::engine::write_u32(memory, rr64::engine::local_race::menu_dirty, 1);
            applied_bike_epoch = menu_epoch;
        }
    }
    // A toggle enabled in the overlay grants once on the next safe game tick.
    // Leaving it on grants again for a new race, never continuously refilling.
    if (!enabled(Flag::AllWeapons) || !live_race(memory))
        return;
    for (unsigned slot = 0; slot < rr64::engine::kMaximumRacers; ++slot)
        grant_once(memory, actors + slot * actor_stride);
}
extern "C" int rr64_offline_rider_protected(unsigned char *memory, unsigned rider) {
    using namespace rr64::offline_modifiers;
    return rider_damage_protected(memory, rider, enabled(Flag::RiderHealth) && live_race(memory));
}
extern "C" int rr64_offline_bike_protected(unsigned char *memory, unsigned bike) {
    using namespace rr64::offline_modifiers;
    return bike_damage_protected(memory, bike, enabled(Flag::BikeDurability) && live_race(memory));
}
