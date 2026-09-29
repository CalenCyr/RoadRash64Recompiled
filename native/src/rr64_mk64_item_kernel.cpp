#include "rr64_mk64_item_kernel.hpp"
#include "rr64_mk64_item_dimensions.hpp"
#include <algorithm>
#include <limits>

namespace rr64::mk64_items {
namespace {
Vec add(Vec a, Vec b) {
    for (unsigned i = 0; i < 3; ++i)
        a[i] += b[i];
    return a;
}
Vec sub(Vec a, Vec b) {
    for (unsigned i = 0; i < 3; ++i)
        a[i] -= b[i];
    return a;
}
Vec mul(Vec a, float v) {
    for (auto &f : a)
        f *= v;
    return a;
}
float dot(Vec a, Vec b) {
    return a[0] * b[0] + a[1] * b[1] + a[2] * b[2];
}
float length(Vec a) {
    return std::sqrt(dot(a, a));
}
Vec normalized(Vec a, Vec fallback = {0, 1, 0}) {
    const float n = length(a);
    return n > .00001f ? mul(a, 1 / n) : fallback;
}
bool finite(Vec a) {
    for (float v : a)
        if (!std::isfinite(v) || std::abs(v) > 100000)
            return false;
    return true;
}
unsigned next(unsigned &value) {
    if (++value == 0)
        value = 1;
    return value;
}
unsigned random(Snapshot &s) {
    auto v = s.random;
    v ^= v << 13;
    v ^= v >> 17;
    v ^= v << 5;
    s.random = v ? v : 1;
    return s.random;
}
void cue(Snapshot &s, unsigned slot, Cue kind, unsigned target) {
    auto &r = s.riders[slot];
    next(r.event_serial);
    r.cue = kind;
    r.event_target = std::uint8_t(target);
}
void inventory_clear(RiderState &r) {
    r.held = Item::None;
    r.charges = 0;
    r.deployed = 0;
    r.golden_until = 0;
    next(r.revision);
}
void remove_object(Snapshot &s, unsigned index) {
    auto &o = s.objects[index];
    if (o.generation && (o.mode == ObjectMode::Orbiting || o.mode == ObjectMode::Trailing)) {
        auto &r = s.riders[o.owner];
        if (r.charges)
            --r.charges;
        if (!r.charges)
            inventory_clear(r);
        else
            next(r.revision);
    }
    o = {};
}
void detach_shields(Snapshot &s, unsigned slot) {
    for (auto &o : s.objects)
        if (o.generation && o.owner == slot &&
            (o.mode == ObjectMode::Orbiting || o.mode == ObjectMode::Trailing))
            o = {};
}
bool usable(const Racer &r) {
    if (r.contact_count > r.contacts.size())
        return false;
    for (unsigned i = 0; i < r.contact_count; ++i)
        if (!finite(r.contacts[i].offset) || !std::isfinite(r.contacts[i].radius) ||
            r.contacts[i].radius <= .001f || r.contacts[i].radius >= 10)
            return false;
    return r.active && r.riding && !r.finished && finite(r.position) && finite(r.velocity) &&
           finite(r.forward) && std::isfinite(r.progress) && std::isfinite(r.radius) &&
           r.radius > .01f && r.radius < 10 && std::isfinite(r.maximum_speed) &&
           r.maximum_speed > 0 && r.maximum_speed <= 300;
}
// Spawn outside the native bike/rider envelope. A bike's first sphere alone
// does not include its front wheel, rear wheel or mounted rider.
float extent(const Racer &r, Vec direction) {
    float reach = r.radius;
    for (unsigned i = 0; i < r.contact_count; ++i)
        reach = std::max(reach, dot(r.contacts[i].offset, direction) + r.contacts[i].radius);
    return reach;
}
float orbit_extent(const Racer &r) {
    float reach = r.radius;
    for (unsigned i = 0; i < r.contact_count; ++i)
        reach = std::max(reach, std::hypot(r.contacts[i].offset[0], r.contacts[i].offset[1]) +
                                    r.contacts[i].radius);
    return reach;
}
// Relative swept spheres, including initial overlap and stationary contact.
float entry(Vec p, Vec d, float r) {
    const double c = double(dot(p, p)) - double(r) * r;
    if (c <= 0)
        return 0;
    const double a = dot(d, d), b = dot(p, d), discriminant = b * b - a * c;
    if (a < 1e-12 || b >= 0 || discriminant < 0)
        return 2;
    const double t = c / (-b + std::sqrt(discriminant));
    return t >= 0 && t <= 1 ? float(t) : 2;
}
void hit(Snapshot &s, StepResult &out, unsigned victim, unsigned owner, Item item, Vec point,
         Vec direction, Vec surface_velocity = {}, Vec victim_displacement = {}) {
    auto &r = s.riders[victim];
    if (immune(r, s.clock) || r.hit_until > s.clock || out.hits[victim].active)
        return;
    r.hit_until = s.clock + 24;
    out.hits[victim] = {true,
                        std::uint8_t(owner),
                        item,
                        point,
                        normalized(direction),
                        surface_velocity,
                        victim_displacement};
    cue(s, victim, Cue::Hit, owner);
}
unsigned target(const Snapshot &s, std::span<const Racer> racers, unsigned owner, Item item) {
    unsigned selected = no_target;
    float best = item == Item::BlueShell ? -std::numeric_limits<float>::infinity()
                                         : std::numeric_limits<float>::infinity();
    for (unsigned slot = 0; slot < racers.size(); ++slot) {
        if ((slot == owner && item != Item::BlueShell) || !usable(racers[slot]) ||
            (item != Item::BlueShell && immune(s.riders[slot], s.clock)))
            continue;
        if (item == Item::BlueShell) {
            if (racers[slot].progress > best) {
                best = racers[slot].progress;
                selected = slot;
            }
        } else {
            const float ahead = racers[slot].progress - racers[owner].progress;
            const float distance = length(sub(racers[slot].position, racers[owner].position));
            if (ahead >= 0 && distance < 100 && distance < best) {
                best = distance;
                selected = slot;
            }
        }
    }
    return selected;
}
bool reserve(const Snapshot &s, unsigned needed) {
    unsigned free = 0;
    for (const auto &o : s.objects)
        if (!o.generation && ++free >= needed)
            return true;
    return false;
}
Object *spawn(Snapshot &s, unsigned owner, Item item, ObjectMode mode, Vec position, Vec velocity,
              unsigned orbit = 0) {
    for (auto &o : s.objects)
        if (!o.generation) {
            // Never reuse a live generation at uint32 wrap. End this very long race
            // generation stream safely rather than creating an ambiguous identity.
            if (s.next_generation == UINT32_MAX)
                return nullptr;
            o = {++s.next_generation,
                 s.clock,
                 s.clock + (shell(item) ? 450u : 1800u),
                 item,
                 mode,
                 std::uint8_t(owner),
                 no_target,
                 std::uint8_t(orbit),
                 0,
                 0,
                 0,
                 position,
                 velocity};
            return &o;
        }
    return nullptr;
}
void boost(Snapshot &s, unsigned slot, const Racer &r) {
    auto &state = s.riders[slot];
    state.boost_until = s.clock + 60;
    // Reference the bike's unmodified rated speed; repeatedly using golden
    // mushrooms must not multiply an already boosted velocity without bound.
    state.boost_speed = std::clamp(r.maximum_speed * 1.35f, 8.f, 400.f);
    cue(s, slot, Cue::Mushroom, slot);
}
void use(Snapshot &s, std::span<const Racer> racers, unsigned slot, const Use &input,
         StepResult &out) {
    auto &r = s.riders[slot];
    const auto &pose = racers[slot];
    if (!input.pressed || !usable(pose) || r.held == Item::None ||
        (r.event_serial && s.clock - r.last_use < 4))
        return;
    const Item held = r.held;
    Vec forward = pose.forward;
    forward[2] = 0;
    forward = normalized(forward);
    const bool backward = input.direction < -20;
    const auto fire = [&](Object &o) {
        o.mode = ObjectMode::Flying;
        o.born = s.clock;
        o.expires = s.clock + 450;
        const bool rear = backward && o.kind == Item::GreenShell;
        const Vec direction = mul(forward, rear ? -1.f : 1.f);
        o.position = add(pose.position,
                         mul(direction, extent(pose, direction) + object_radius(o.kind) + .1f));
        o.position[2] += .35f;
        o.velocity = mul(direction, std::clamp(length(pose.velocity) + 12.f, 18.f, 400.f));
        o.target = rear ? no_target : std::uint8_t(target(s, racers, slot, o.kind));
    };
    if (held == Item::TripleGreenShell || held == Item::TripleRedShell ||
        held == Item::BananaBunch) {
        const Item kind = held == Item::TripleGreenShell ? Item::GreenShell
                          : held == Item::TripleRedShell ? Item::RedShell
                                                         : Item::Banana;
        if (!r.deployed) {
            if (!reserve(s, r.charges) || UINT32_MAX - s.next_generation < r.charges)
                return;
            for (unsigned n = 0; n < r.charges; ++n)
                spawn(s, slot, kind,
                      kind == Item::Banana ? ObjectMode::Trailing : ObjectMode::Orbiting,
                      pose.position, {}, n);
            r.deployed = 1;
        } else {
            Object *selected = nullptr;
            for (auto &candidate : s.objects)
                if (candidate.generation && candidate.owner == slot &&
                    (candidate.mode == ObjectMode::Orbiting ||
                     candidate.mode == ObjectMode::Trailing) &&
                    (!selected || (kind == Item::Banana && candidate.orbit > selected->orbit)))
                    selected = &candidate;
            if (selected) {
                auto &o = *selected;
                if (kind == Item::Banana) {
                    o.mode = ObjectMode::Flying;
                    o.born = s.clock;
                    o.expires = s.clock + 1800;
                    const bool thrown = input.direction > 20;
                    if (thrown)
                        o.position =
                            add(pose.position,
                                mul(forward, extent(pose, forward) + object_radius(o.kind) + .1f));
                    o.position[2] += .35f;
                    o.velocity = thrown ? add(pose.velocity, mul(forward, 6)) : mul(forward, -2);
                    o.velocity[2] = thrown ? 5.f : 1.f;
                } else
                    fire(o);
                if (--r.charges == 0)
                    inventory_clear(r);
            }
        }
        cue(s, slot, kind == Item::Banana ? Cue::Banana : Cue::Shell, slot);
    } else if (object_item(held)) {
        if (!reserve(s, 1))
            return;
        auto *o = spawn(s, slot, held, ObjectMode::Flying, pose.position, {});
        if (!o)
            return;
        if (shell(held))
            fire(*o);
        else {
            const bool thrown = held == Item::Banana && input.direction > 20;
            const Vec direction = mul(forward, thrown ? 1.f : -1.f);
            o->position = add(pose.position,
                              mul(direction, extent(pose, direction) + object_radius(held) + .1f));
            o->position[2] += .35f;
            o->velocity = thrown ? add(pose.velocity, mul(forward, 6)) : mul(forward, -2);
            o->velocity[2] = thrown ? 5.f : 1.f;
        }
        cue(s, slot,
            shell(held)            ? Cue::Shell
            : held == Item::Banana ? Cue::Banana
                                   : Cue::FakeBox,
            slot);
        inventory_clear(r);
    } else if (held == Item::Mushroom || held == Item::DoubleMushroom ||
               held == Item::TripleMushroom) {
        boost(s, slot, pose);
        if (--r.charges == 0)
            inventory_clear(r);
        else
            r.held = r.charges == 2 ? Item::DoubleMushroom : Item::Mushroom;
    } else if (held == Item::GoldenMushroom) {
        if (!r.golden_until)
            r.golden_until = s.clock + 225;
        boost(s, slot, pose);
    } else if (held == Item::Star) {
        r.star_until = s.clock + 300;
        r.shrink_until = 0;
        cue(s, slot, Cue::Star, slot);
        inventory_clear(r);
    } else if (held == Item::Lightning) {
        for (unsigned victim = 0; victim < racers.size(); ++victim)
            if (victim != slot && usable(racers[victim]) && !immune(s.riders[victim], s.clock)) {
                s.riders[victim].shrink_until = s.clock + 300;
                detach_shields(s, victim);
                inventory_clear(s.riders[victim]);
            }
        cue(s, slot, Cue::Lightning, slot);
        inventory_clear(r);
    } else if (held == Item::Boo) {
        std::array<unsigned, racer_capacity> eligible{};
        unsigned count = 0;
        for (unsigned victim = 0; victim < racers.size(); ++victim)
            if (victim != slot && racers[victim].active && !racers[victim].finished &&
                s.riders[victim].held != Item::None && !immune(s.riders[victim], s.clock))
                eligible[count++] = victim;
        inventory_clear(r);
        r.boo_until = s.clock + 210;
        unsigned stolen_from = slot;
        if (count) {
            stolen_from = eligible[random(s) % count];
            auto &victim = s.riders[stolen_from];
            r.held = victim.held;
            r.charges = victim.charges;
            detach_shields(s, stolen_from);
            inventory_clear(victim);
        }
        cue(s, slot, Cue::Boo, stolen_from);
    }
    r.last_use = s.clock;
    next(r.revision);
}
void update_shield(Snapshot &s, Object &o, const Racer &r) {
    o.expires = s.clock + 1800;
    if (o.mode == ObjectMode::Orbiting) {
        const float angle = float(s.clock) * .14f + float(o.orbit) * 2.0943951024f;
        const float reach = orbit_extent(r) + object_radius(o.kind) + .1f;
        o.position = add(r.position, {std::cos(angle) * reach, std::sin(angle) * reach, .4f});
    } else {
        auto forward = r.forward;
        forward[2] = 0;
        const Vec rear = mul(normalized(forward), -1);
        const float spacing = 2 * object_radius(o.kind) + .15f;
        o.position = add(r.position, mul(rear, extent(r, rear) + object_radius(o.kind) + .1f +
                                                   o.orbit * spacing));
        o.position[2] += .15f;
    }
    o.velocity = r.velocity;
}
// Resolve each leg of a projectile's path before its next world contact. A
// single chord from the initial to the final position loses the bounce path
// and can hit riders through a wall. Actor motion covers this same time slice.
bool contacts(Snapshot &s, StepResult &out, std::span<const Racer> racers, unsigned index,
              Vec start, Vec movement, float elapsed, float duration, float delta, float limit) {
    auto &o = s.objects[index];
    for (unsigned pass = 0; pass <= racer_capacity; ++pass) {
        unsigned victim = no_target, defender = object_capacity;
        float first = limit;
        Vec hit_point{}, hit_normal{}, victim_displacement{};
        if (o.mode == ObjectMode::Flying && shell(o.kind)) {
            for (unsigned n = 0; n < object_capacity; ++n) {
                const auto &shield = s.objects[n];
                if (!shield.generation || shield.owner == o.owner ||
                    (shield.mode != ObjectMode::Orbiting && shield.mode != ObjectMode::Trailing))
                    continue;
                const float t = entry(sub(start, shield.position), movement,
                                      object_radius(o.kind) + object_radius(shield.kind));
                if (t < first) {
                    first = t;
                    defender = n;
                }
            }
        }
        for (unsigned slot = 0; slot < racers.size(); ++slot) {
            if (!usable(racers[slot]) || immune(s.riders[slot], s.clock) ||
                s.riders[slot].hit_until > s.clock || out.hits[slot].active ||
                (slot == o.owner && (s.clock - o.born < 24 || o.mode == ObjectMode::Orbiting ||
                                     o.mode == ObjectMode::Trailing)))
                continue;
            const auto &racer = racers[slot];
            const Vec actor_motion = mul(racer.velocity, duration);
            // Use fresh transformed native contacts, not an invented point
            // above the bike. Keep actor and projectile on the same time slice.
            for (unsigned n = 0; n < racer.contact_count; ++n) {
                auto center = add(racer.position, racer.contacts[n].offset);
                center = sub(center, mul(racer.velocity, delta - elapsed));
                const float t = entry(sub(start, center), sub(movement, actor_motion),
                                      object_radius(o.kind) + racer.contacts[n].radius);
                if (t < first) {
                    first = t;
                    victim = slot;
                    defender = object_capacity;
                    const Vec object_center = add(start, mul(movement, t));
                    const Vec actor_center = add(center, mul(actor_motion, t));
                    hit_normal = normalized(
                        sub(actor_center, object_center),
                        normalized(sub(o.velocity, racer.velocity), mul(racer.forward, -1)));
                    hit_point = add(object_center, mul(hit_normal, object_radius(o.kind)));
                    victim_displacement = mul(racer.velocity, delta - elapsed - duration * t);
                }
            }
        }
        if (defender < object_capacity) {
            remove_object(s, defender);
            remove_object(s, index);
            return true;
        }
        if (victim == no_target)
            return false;
        hit(s, out, victim, o.owner, o.kind, hit_point, hit_normal, o.velocity,
            victim_displacement);
        // A ground-following blue shell can hit intervening riders while
        // continuing toward the leader. hit_until prevents duplicate contacts.
        if (o.kind != Item::BlueShell || victim == o.target) {
            remove_object(s, index);
            return true;
        }
    }
    return false;
}
} // namespace

void initialize(Snapshot &s, std::uint32_t seed, std::uint32_t clock) noexcept {
    s = {};
    s.enabled = 1;
    s.clock = clock;
    s.random = seed ? seed : 1;
}
void runover(Snapshot &s, StepResult &out, unsigned victim, unsigned owner, Vec point,
             Vec direction) noexcept {
    if (!s.enabled || victim >= racer_capacity || owner >= racer_capacity || victim == owner ||
        s.riders[victim].shrink_until <= s.clock || s.riders[owner].shrink_until > s.clock ||
        s.riders[owner].boo_until > s.clock)
        return;
    hit(s, out, victim, owner, Item::Lightning, point, direction);
}
void retire(Snapshot &s, unsigned slot) noexcept {
    if (slot >= racer_capacity)
        return;
    for (auto &o : s.objects)
        if (o.generation && o.owner == slot)
            o = {};
    s.riders[slot] = {};
}
bool grant(Snapshot &s, unsigned slot, Item item) noexcept {
    if (!s.enabled || slot >= racer_capacity || !valid_item(item) || item == Item::None ||
        s.riders[slot].held != Item::None)
        return false;
    auto &r = s.riders[slot];
    r.held = item;
    r.charges = item == Item::BananaBunch      ? 5
                : item == Item::DoubleMushroom ? 2
                : item == Item::TripleGreenShell || item == Item::TripleRedShell ||
                        item == Item::TripleMushroom
                    ? 3
                    : 1;
    next(r.revision);
    return true;
}
StepResult step(Snapshot &s, std::span<const Racer> racers, std::span<const Use> input,
                unsigned clock, float delta, const Environment &world) noexcept {
    StepResult out;
    if (!s.enabled || racers.size() > racer_capacity || input.size() != racers.size() ||
        clock < s.clock || clock > 30000000 || !std::isfinite(delta) || delta <= 0 || delta > .25f)
        return out;
    s.clock = clock;
    for (unsigned slot = 0; slot < racer_capacity; ++slot) {
        auto &r = s.riders[slot];
        if (slot >= racers.size() || !racers[slot].active || racers[slot].finished) {
            retire(s, slot);
            continue;
        }
        for (auto *deadline :
             {&r.star_until, &r.boo_until, &r.shrink_until, &r.boost_until, &r.hit_until})
            if (*deadline <= clock)
                *deadline = 0;
        if (!r.boost_until)
            r.boost_speed = 0;
        if (r.golden_until && r.golden_until <= clock)
            inventory_clear(r);
        if (!racers[slot].riding && r.deployed) {
            detach_shields(s, slot);
            inventory_clear(r);
        }
        use(s, racers, slot, input[slot], out);
    }
    std::array<Vec, object_capacity> previous{};
    // Update all defenses first: interception must not depend on object slot.
    for (unsigned index = 0; index < object_capacity; ++index) {
        auto &o = s.objects[index];
        if (!o.generation)
            continue;
        if (o.expires <= clock || o.owner >= racers.size() || !finite(o.position) ||
            !finite(o.velocity)) {
            remove_object(s, index);
            continue;
        }
        previous[index] = o.position;
        if (o.mode == ObjectMode::Orbiting || o.mode == ObjectMode::Trailing) {
            if (!usable(racers[o.owner])) {
                remove_object(s, index);
                continue;
            }
            update_shield(s, o, racers[o.owner]);
        }
    }
    for (unsigned index = 0; index < object_capacity; ++index) {
        auto &o = s.objects[index];
        if (!o.generation)
            continue;
        if (o.mode == ObjectMode::Flying) {
            if (shell(o.kind) && o.kind != Item::GreenShell) {
                // Follow the current leader even when the lead changes or
                // that leader is temporarily protected by Star/Boo.
                if (o.kind == Item::BlueShell || o.target >= racers.size() ||
                    !usable(racers[o.target]) || immune(s.riders[o.target], clock))
                    o.target = std::uint8_t(target(s, racers, o.owner, o.kind));
                if (o.target < racers.size()) {
                    const Vec goal = world.guide ? world.guide(world.context, o.position,
                                                               racers[o.target].position)
                                                 : racers[o.target].position;
                    if (finite(goal)) {
                        Vec desired = sub(goal, o.position);
                        desired[2] = 0;
                        // Insanity bikes can exceed 140 world units/s before
                        // mushrooms. A fixed normal-bike cap lets every one
                        // outrun a homing shell indefinitely.
                        const auto &chased = racers[o.target].velocity;
                        const float speed =
                            std::clamp(std::max(std::hypot(o.velocity[0], o.velocity[1]),
                                                std::hypot(chased[0], chased[1]) + 12.f),
                                       18.f, 400.f);
                        Vec old{o.velocity[0], o.velocity[1], 0};
                        const Vec steered =
                            normalized(add(normalized(old), mul(normalized(desired), delta * 5)));
                        o.velocity[0] = steered[0] * speed;
                        o.velocity[1] = steered[1] * speed;
                    }
                }
            }
            o.velocity[2] = std::max(-60.f, o.velocity[2] - 15.f * delta);
            Vec remaining = mul(o.velocity, delta);
            float time_left = delta;
            if (length(remaining) <= .000001f) {
                // A rider can cross a stationary projectile between samples.
                contacts(s, out, racers, index, o.position, remaining, 0, delta, delta, 1.000001f);
                continue;
            }
            for (unsigned pass = 0; pass < 4 && length(remaining) > .000001f; ++pass) {
                SurfaceHit contact = world.sweep ? world.sweep(world.context, o.position, remaining,
                                                               object_radius(o.kind))
                                                 : SurfaceHit{};
                if (!contact.hit) {
                    if (!contacts(s, out, racers, index, o.position, remaining, delta - time_left,
                                  time_left, delta, 1.000001f))
                        o.position = add(o.position, remaining);
                    break;
                }
                if (!std::isfinite(contact.fraction) || contact.fraction < 0 ||
                    contact.fraction > 1 || !finite(contact.normal) ||
                    length(contact.normal) < .5f || !std::isfinite(contact.penetration) ||
                    contact.penetration < 0 || contact.penetration > 10) {
                    remove_object(s, index);
                    break;
                }
                if (contacts(s, out, racers, index, o.position, remaining, delta - time_left,
                             time_left, delta, contact.fraction))
                    break;
                const Vec normal = normalized(contact.normal);
                o.position = add(o.position, mul(remaining, contact.fraction));
                o.position = add(o.position, mul(normal, contact.penetration + .001f));
                const bool floor = normal[2] > .45f;
                if (!floor && shell(o.kind)) {
                    if (o.kind != Item::GreenShell || ++o.bounces > 6) {
                        remove_object(s, index);
                        break;
                    }
                    o.velocity =
                        sub(o.velocity, mul(normal, 2 * std::min(0.f, dot(o.velocity, normal))));
                } else
                    o.velocity =
                        sub(o.velocity, mul(normal, std::min(0.f, dot(o.velocity, normal))));
                if (floor && !shell(o.kind)) {
                    o.mode = ObjectMode::Resting;
                    o.velocity = {};
                    break;
                }
                time_left *= 1 - contact.fraction;
                remaining = mul(o.velocity, time_left);
            }
        } else
            contacts(s, out, racers, index, previous[index], sub(o.position, previous[index]), 0,
                     delta, delta, 1.000001f);
    }
    for (unsigned source = 0; source < racers.size(); ++source) {
        if (!usable(racers[source]) || s.riders[source].star_until <= clock)
            continue;
        for (unsigned victim = 0; victim < racers.size(); ++victim) {
            if (victim == source || !usable(racers[victim]))
                continue;
            const Vec relative = sub(racers[source].position, racers[victim].position);
            const Vec movement = mul(sub(racers[source].velocity, racers[victim].velocity), delta);
            const float t = entry(sub(relative, movement), movement,
                                  racers[source].radius + racers[victim].radius + .15f);
            if (t <= 1)
                hit(s, out, victim, source, Item::Star,
                    sub(racers[victim].position, mul(racers[victim].velocity, delta * (1 - t))),
                    normalized(movement, mul(relative, -1)));
        }
    }
    return out;
}
} // namespace rr64::mk64_items
