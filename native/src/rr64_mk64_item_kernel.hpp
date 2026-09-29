#pragma once
#include "rr64_mk64_item_state.hpp"
#include <span>

namespace rr64::mk64_items {
struct Contact {
    Vec offset{}; // Current transformed native sphere center, relative to position.
    float radius = 0;
};
struct Racer {
    bool active = false, riding = false, finished = false, human = false;
    Vec position{}, velocity{}, forward{0, 1, 0};
    float radius = .45f, progress = 0, maximum_speed = 25;
    std::array<Contact, 6> contacts{};
    unsigned contact_count = 0;
};
struct Use {
    bool pressed = false;
    std::int8_t direction = 0; // Negative: green shell backward. Positive: throw banana forward.
};
struct SurfaceHit {
    bool hit = false;
    float fraction = 1;
    Vec normal{}, point{};
    float penetration = 0;
};
struct Environment {
    void *context = nullptr;
    // Actual course surface sweep in rider-world units. Include floors/ceilings,
    // not just wall panels. Missing ground permits a real fall and item expiry.
    SurfaceHit (*sweep)(void *, Vec start, Vec motion, float radius) = nullptr;
    // Return a nearby point on the route towards this goal, not a direct ray
    // through terrain. Adapter chooses route branch/progress from the real pack.
    Vec (*guide)(void *, Vec from, Vec goal) = nullptr;
};
struct Hit {
    bool active = false;
    std::uint8_t owner = 0;
    Item item = Item::None;
    Vec point{}, direction{};
    Vec surface_velocity{};    // Raw object velocity at contact, before retirement.
    Vec victim_displacement{}; // Contact-time to current pose; preserves the torque lever.
};
struct StepResult {
    std::array<Hit, racer_capacity> hits{};
};
// Called only after the native bike contact test succeeds. Recheck effects at
// the commit boundary, since a use/expiry in this update may protect the rider.
void runover(Snapshot &, StepResult &, unsigned victim, unsigned owner, Vec point,
             Vec direction) noexcept;
void initialize(Snapshot &, std::uint32_t seed, std::uint32_t clock = 0) noexcept;
// Retire all objects as well as inventory/effects on actor replacement. A reused
// canonical slot must never inherit an earlier rider's projectiles or credit.
void retire(Snapshot &, unsigned slot) noexcept;
bool grant(Snapshot &, unsigned slot, Item) noexcept;
// Finite bounded integration, no heap allocations or native-memory writes.
// Each use edge is consumed once by caller, regardless of native substep count.
StepResult step(Snapshot &, std::span<const Racer>, std::span<const Use>, unsigned clock,
                float delta, const Environment &) noexcept;
} // namespace rr64::mk64_items
