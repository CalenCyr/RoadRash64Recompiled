#pragma once
#include <array>
#include <cmath>
#include <cstdint>
#include <initializer_list>
#include <type_traits>

namespace rr64::mk64_items {
// Native MK64 ItemId order. DoubleMushroom is the remaining triple stack.
enum class Item : std::uint8_t {
    None,
    Banana,
    BananaBunch,
    GreenShell,
    TripleGreenShell,
    RedShell,
    TripleRedShell,
    BlueShell,
    Lightning,
    FakeBox,
    Star,
    Boo,
    Mushroom,
    DoubleMushroom,
    TripleMushroom,
    GoldenMushroom
};
constexpr unsigned racer_capacity = 14, object_capacity = 64, ticks_per_second = 30;
constexpr std::uint8_t no_target = 255;
using Vec = std::array<float, 3>; // Road Rash rider-world XY, height Z.
constexpr bool valid_item(Item item) {
    return unsigned(item) <= 15;
}
constexpr unsigned reward_id(Item item) {
    return item == Item::None ? 0 : 16 + unsigned(item);
}
constexpr bool is_reward(unsigned value) {
    return value >= 17 && value <= 31;
}
constexpr Item reward_item(unsigned value) {
    return is_reward(value) ? Item(value - 16) : Item::None;
}
enum class ObjectMode : std::uint8_t { None, Flying, Resting, Orbiting, Trailing };
enum class Cue : std::uint8_t { None, Shell, Banana, Mushroom, Lightning, Star, Boo, Hit, FakeBox };

struct RiderState {
    Item held = Item::None;
    std::uint8_t charges = 0, deployed = 0, reserved = 0;
    std::uint32_t revision = 0;
    std::uint32_t star_until = 0, boo_until = 0, shrink_until = 0;
    std::uint32_t boost_until = 0, golden_until = 0, last_use = 0;
    std::uint32_t hit_until = 0;
    float boost_speed = 0;
    std::uint32_t event_serial = 0;
    Cue cue = Cue::None;
    std::uint8_t event_target = 0, reserved2 = 0, reserved3 = 0;
    bool operator==(const RiderState &) const = default;
};
struct Object {
    std::uint32_t generation = 0, born = 0, expires = 0;
    Item kind = Item::None;
    ObjectMode mode = ObjectMode::None;
    std::uint8_t owner = 0, target = 0;
    std::uint8_t orbit = 0, bounces = 0, reserved = 0, reserved2 = 0;
    Vec position{}, velocity{};
    bool operator==(const Object &) const = default;
};
struct Snapshot {
    // Enabled remains set even with no objects, so empty frames retire old ones.
    std::uint32_t enabled = 0, clock = 0, next_generation = 0, random = 0;
    std::array<RiderState, racer_capacity> riders{};
    std::array<Object, object_capacity> objects{};
    bool operator==(const Snapshot &) const = default;
};
static_assert(sizeof(RiderState) == 48 && sizeof(Object) == 44);
static_assert(std::is_trivially_copyable_v<Snapshot>);

constexpr bool shell(Item item) {
    return item == Item::GreenShell || item == Item::RedShell || item == Item::BlueShell;
}
constexpr bool object_item(Item item) {
    return shell(item) || item == Item::Banana || item == Item::FakeBox;
}
constexpr bool immune(const RiderState &rider, unsigned clock) {
    return rider.star_until > clock || rider.boo_until > clock;
}
inline bool valid(const RiderState &r, unsigned clock) noexcept {
    if (!valid_item(r.held) || r.reserved || r.reserved2 || r.reserved3 || r.deployed > 1 ||
        unsigned(r.cue) > unsigned(Cue::FakeBox) || r.event_target >= racer_capacity ||
        r.last_use > clock || (!r.event_serial && r.cue != Cue::None) ||
        !std::isfinite(r.boost_speed) || r.boost_speed < 0 || r.boost_speed > 400 ||
        (!r.boost_until && r.boost_speed != 0))
        return false;
    for (auto deadline :
         {r.star_until, r.boo_until, r.shrink_until, r.boost_until, r.golden_until, r.hit_until})
        if (deadline && (deadline <= clock || deadline - clock > 1800))
            return false;
    if (r.held == Item::None)
        return r.charges == 0 && !r.deployed && !r.golden_until;
    if (!r.charges || r.charges > 5)
        return false;
    const unsigned maximum = r.held == Item::BananaBunch ? 5
                             : r.held == Item::TripleGreenShell || r.held == Item::TripleRedShell ||
                                     r.held == Item::TripleMushroom
                                 ? 3
                             : r.held == Item::DoubleMushroom ? 2
                                                              : 1;
    if (r.charges > maximum || (r.golden_until && r.held != Item::GoldenMushroom))
        return false;
    return !r.deployed || r.held == Item::BananaBunch || r.held == Item::TripleGreenShell ||
           r.held == Item::TripleRedShell;
}
inline bool valid(const Object &o, unsigned clock) noexcept {
    if (!o.generation)
        return o == Object{};
    if (!object_item(o.kind) || o.mode == ObjectMode::None ||
        unsigned(o.mode) > unsigned(ObjectMode::Trailing) || o.owner >= racer_capacity ||
        (o.target != no_target && o.target >= racer_capacity) || o.orbit >= 5 || o.bounces > 12 ||
        o.reserved || o.reserved2 || o.born > clock || o.expires <= clock ||
        o.expires - clock > 1800)
        return false;
    for (unsigned axis = 0; axis < 3; ++axis)
        if (!std::isfinite(o.position[axis]) || std::abs(o.position[axis]) > 100000 ||
            !std::isfinite(o.velocity[axis]) || std::abs(o.velocity[axis]) > 1000)
            return false;
    return (o.mode != ObjectMode::Orbiting || (shell(o.kind) && o.orbit < 3)) &&
           (o.mode != ObjectMode::Trailing || o.kind == Item::Banana);
}
inline bool valid(const Snapshot &state) noexcept {
    if (!state.enabled)
        return state == Snapshot{};
    if (state.enabled != 1 || state.clock > 30000000 || !state.random)
        return false;
    for (const auto &r : state.riders)
        if (!valid(r, state.clock))
            return false;
    std::array<unsigned, racer_capacity> shields{};
    for (unsigned i = 0; i < object_capacity; ++i) {
        const auto &o = state.objects[i];
        if (!valid(o, state.clock) || o.generation > state.next_generation)
            return false;
        if (!o.generation)
            continue;
        for (unsigned j = 0; j < i; ++j)
            if (state.objects[j].generation == o.generation)
                return false;
        if (o.mode == ObjectMode::Orbiting || o.mode == ObjectMode::Trailing) {
            for (unsigned j = 0; j < i; ++j) {
                const auto &previous = state.objects[j];
                if (previous.generation && previous.owner == o.owner && previous.orbit == o.orbit &&
                    (previous.mode == ObjectMode::Orbiting ||
                     previous.mode == ObjectMode::Trailing))
                    return false;
            }
            const auto &owner = state.riders[o.owner];
            if (!owner.deployed || ++shields[o.owner] > owner.charges ||
                (o.kind == Item::Banana && owner.held != Item::BananaBunch) ||
                (o.kind == Item::GreenShell && owner.held != Item::TripleGreenShell) ||
                (o.kind == Item::RedShell && owner.held != Item::TripleRedShell) ||
                o.kind == Item::BlueShell)
                return false;
        }
    }
    for (unsigned slot = 0; slot < racer_capacity; ++slot)
        if (state.riders[slot].deployed && shields[slot] != state.riders[slot].charges)
            return false;
    return true;
}
} // namespace rr64::mk64_items
