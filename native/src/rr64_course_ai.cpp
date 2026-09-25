#include "rr64_course_ai.hpp"
#include "rr64_course_hazard_motion.hpp"
#include "rr64_engine_layout.hpp"
#include "rr64_experimental_course.hpp"
#include "rr64_online_flow.hpp"
#include "rr64_prediction_rules.hpp"
#include <algorithm>
#include <bit>
#include <cmath>
#include <limits>

namespace rr64::course_ai {
namespace {
using course_walls::Vec;
bool finite(Vec v) {
    return std::isfinite(v[0]) && std::isfinite(v[1]) && std::isfinite(v[2]);
}
float length(Vec v) {
    return std::sqrt(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);
}
}
Decision inspect(const course_walls::World *walls, const course_hazards::Data *hazards,
                 const netplay::CourseHazardState &presented,
                 std::span<const course_walls::Sphere> spheres, Vec velocity, Vec forward,
                 float braking_acceleration) noexcept {
    Decision result;
    if (spheres.empty() || spheres.size() > 3 || !finite(velocity) || !finite(forward) ||
        !std::isfinite(braking_acceleration) || braking_acceleration <= 0 ||
        !netplay::valid_course_hazard_state(presented))
        return result;
    for (const auto &s : spheres)
        if (!finite(s.center) || !std::isfinite(s.radius) || s.radius <= 0 || s.radius > 10)
            return result;
    const float speed = length(velocity);
    if (!std::isfinite(speed) || speed > 1000)
        return result;
    // The native AI's friction/profile estimate supplies deceleration. Add a
    // short reaction margin; cap prediction at two seconds (20 fixed queries).
    // At rest, probe only a small forward clearance so a waiting AI can resume.
    result.horizon = std::clamp(speed / (2 * braking_acceleration) + .15f, .15f, 2.f);
    if (speed < .5f) {
        const float norm = length(forward);
        if (!(norm > .01f))
            return {};
        for (unsigned k = 0; k < 3; ++k)
            velocity[k] = forward[k] / norm * .5f;
    }
    const unsigned steps = std::clamp(unsigned(std::ceil(result.horizon / .1f)), 1u, 20u);
    const float delta = result.horizon / steps;
    Vec move{};
    for (unsigned k = 0; k < 3; ++k)
        move[k] = velocity[k] * delta;
    std::array<course_walls::Sphere, 3> probes{};
    std::copy(spheres.begin(), spheres.end(), probes.begin());
    auto forecast = presented;
    for (unsigned step = 0; step < steps; ++step) {
        if (walls) {
            // Resolve the whole compound so an outward initial overlap cannot
            // hide a second, approaching wall at a corner. This remains a pure
            // query; actual depenetration belongs to the physics adapter.
            const auto hit = course_walls::resolve(
                *walls, {probes.data(), spheres.size()}, move, velocity);
            if (hit.contacts && hit.normal_speed > 1e-5f) {
                result.brake = true;
                result.wall = hit.triangle_id;
                return result;
            }
        }
        if (hazards) {
            // resolve_state expects end poses. Forecast only the current
            // velocity, never advance the authoritative hazard simulation.
            for (unsigned i = 0; i < forecast.count; ++i)
                for (unsigned k = 0; k < 3; ++k)
                    forecast.poses[i].position[k] += forecast.poses[i].velocity[k] * delta;
            const auto hit = course_hazards::resolve_state(
                *hazards, forecast, {probes.data(), spheres.size()}, move, velocity, delta);
            if (hit.contacts && hit.normal_speed > 1e-5f) {
                result.brake = true;
                result.hazard = hit.id;
                return result;
            }
        }
        for (unsigned i = 0; i < spheres.size(); ++i)
            for (unsigned k = 0; k < 3; ++k)
                probes[i].center[k] += move[k];
    }
    return result;
}

Decision choose(const course_walls::World *walls, const course_walls::World *floor,
                const course_hazards::Data *hazards,
                const netplay::CourseHazardState &presented,
                std::span<const course_walls::Sphere> spheres, Vec velocity, Vec forward,
                Vec normal, float deceleration, float left, float right,
                float preferred_side) noexcept {
    Decision result = inspect(walls, hazards, presented, spheres, velocity, forward, deceleration);
    if (!result.brake || !finite(normal) || !std::isfinite(left) ||
        !std::isfinite(right) || left > right)
        return result;
    const float norm = std::hypot(normal[0], normal[1]);
    if (!(norm > .01f)) return result;
    normal = {normal[0] / norm, normal[1] / norm, 0};
    const float speed = std::hypot(velocity[0], velocity[1]);
    // Avoidance begins with the actual velocity. Side displacement grows
    // smoothly from zero, so an impossible instantaneous lane change cannot
    // certify a path through the obstacle immediately in front of the bike.
    const float horizon = std::clamp(result.horizon, .5f, 2.f);
    const unsigned steps = unsigned(std::ceil(horizon / .1f));
    const float dt = horizon / steps;
    if (speed < .5f) {
        const float forward_length = std::hypot(forward[0], forward[1]);
        if (!(forward_length > .01f)) return result;
        velocity[0] = forward[0] / forward_length * .5f;
        velocity[1] = forward[1] / forward_length * .5f;
    }
    const float reach = std::clamp(std::max(speed, 3.f) * horizon * .45f, 1.5f, 12.f);
    const float side = preferred_side < 0 ? -1.f : 1.f;
    float best = std::numeric_limits<float>::infinity(), chosen = 0;
    // Six candidates at most, evaluated only after a real approaching contact.
    // Every candidate sees the same hazard snapshot and fixed prediction times.
    for (float fraction : {.4f, .7f, 1.f}) for (float direction : {side, -side}) {
        const float offset = direction * reach * fraction;
        if (offset < left || offset > right) continue;
        std::array<course_walls::Sphere, 3> probes{};
        std::copy(spheres.begin(), spheres.end(), probes.begin());
        auto forecast = presented;
        Vec previous{};
        bool safe = true;
        for (unsigned step = 1; step <= steps; ++step) {
            const float t = float(step) / steps, elapsed = t * horizon;
            // Cubic lane change, including zero initial lateral velocity.
            const float lateral = offset * t * t * (3.f - 2.f * t);
            Vec next{}, move{}, sample_velocity{};
            for (unsigned k = 0; k < 3; ++k) {
                next[k] = velocity[k] * elapsed + normal[k] * lateral;
                move[k] = next[k] - previous[k];
                sample_velocity[k] = move[k] / dt;
            }
            if (walls) {
                const auto hit = course_walls::resolve(*walls,
                    {probes.data(), spheres.size()}, move, sample_velocity);
                if (hit.contacts && hit.normal_speed > 1e-5f) { safe = false; break; }
            }
            if (hazards) {
                for (unsigned i = 0; i < forecast.count; ++i)
                    for (unsigned k = 0; k < 3; ++k)
                        forecast.poses[i].position[k] += forecast.poses[i].velocity[k] * dt;
                const auto hit = course_hazards::resolve_state(*hazards, forecast,
                    {probes.data(), spheres.size()}, move, sample_velocity, dt);
                if (hit.contacts && hit.normal_speed > 1e-5f) { safe = false; break; }
            }
            for (unsigned i = 0; i < spheres.size(); ++i)
                for (unsigned k = 0; k < 3; ++k) probes[i].center[k] += move[k];
            if (floor && step % 2 == 0) {
                // A side route must have real finite support. The forward path
                // is deliberately not subject to this test: authored jumps keep
                // native momentum and remain legitimate airborne trajectories.
                auto support = probes[0];
                support.radius = .05f;
                support.center[2] += 2;
                const auto hit = course_walls::sweep_sphere(*floor, support, {0, 0, -6});
                if (!hit.hit || hit.normal[2] < .3f) { safe = false; break; }
            }
            previous = next;
        }
        const float cost = std::abs(offset) +
            (preferred_side != 0 && offset * preferred_side < 0 ? reach * .2f : 0.f);
        if (safe && cost < best) { best = cost; chosen = offset; }
    }
    if (std::isfinite(best)) {
        result.brake = false;
        result.steer = true;
        result.lateral = chosen;
        result.horizon = horizon;
    }
    return result;
}
namespace {
using namespace engine;
unsigned word(unsigned char *m, unsigned a) {
    unsigned v = 0;
    read_u32(m, a, v);
    return v;
}
unsigned half(unsigned char *m, unsigned a) {
    std::uint16_t v = 0;
    read_u16(m, a, v);
    return v;
}
float real(unsigned char *m, unsigned a) {
    float v = NAN;
    read_float(m, a, v);
    return v;
}
Vec vector(unsigned char *m, unsigned a) {
    return {real(m, a), real(m, a + 4), real(m, a + 8)};
}
// Both adapters are restricted to native, attached AI racers. Human control,
// client prediction and original courses keep their existing paths.
bool eligible_racer(unsigned char *m, unsigned actor) {
    if (!m || !experimental_course::active() || prediction::active() ||
        !is_live_race_transition(word(m, globals::main_mode), word(m, globals::pending_mode)) ||
        half(m, globals::gameplay_pause_state))
        return false;
    constexpr unsigned actors = 0x800D8570, stride = 0x118;
    const unsigned slot = (actor - actors) / stride;
    if (actor < actors || slot >= kMaximumRacers || (actor - actors) % stride ||
        !half(m, actor + 0x24) || half(m, actor + 0x26) != 1 ||
        word(m, actor + 8) != 0xFFFFFFFF || word(m, actor + 0x20) == 7)
        return false;
    const auto rules = netplay::get_physics_rules();
    if (rules.active) {
        if (!rules.connected || !rules.authoritative || !rules.is_host ||
            rules.phase != netplay::Phase::Race || rules.local_slot >= kMaximumRacers)
            return false;
        const auto canonical =
            online_flow::mapped_slot(slot, rules.local_slot, rules.replicated_riders);
        if (rules.authority_humans & (1u << canonical))
            return false;
    }
    const unsigned bike = word(m, actor + 0xE0), rider = word(m, actor + 0xE4),
                   route = word(m, actor + 0xE8), profile = word(m, actor + 0xF8);
    if (!valid_guest_range(bike, bike::stride) || !valid_guest_range(rider, rider::stride) ||
        !valid_guest_range(route, 0x64) || !valid_guest_range(profile, 0x1C) ||
        word(m, bike + 0x800) != rider || word(m, rider + 0x584) != bike ||
        !half(m, bike + 0x7F8) || !half(m, rider + 0x57C) || half(m, bike + 0x7F6) ||
        half(m, rider + 0x57E))
        return false;
    for (unsigned offset : {0x4Cu, 0x4Eu, 0x50u, 0x52u})
        if (half(m, route + offset))
            return false;
    return true;
}

// The first clamp starts a target-selection pass. The second clamp is reached
// only by 52BFC's unobstructed branch; obstacle avoidance instead calls 526E0.
// Carry that distinction for one steering evaluation without touching guest
// profiles or mistaking a deliberate avoidance line for ordinary route guidance.
struct Guidance {
    unsigned char *memory = nullptr;
    unsigned actor = 0;
    unsigned profile = 0;
    bool unobstructed = false;
};
thread_local std::array<Guidance, kMaximumRacers> guidance{};

struct PlannedControl {
    unsigned char *memory = nullptr;
    unsigned actor = 0, bike = 0, tick = 0;
    Decision decision{};
    float side = 0;
};
thread_local std::array<PlannedControl, kMaximumRacers> planned{};

PlannedControl *plan_for(unsigned actor) {
    constexpr unsigned actors = 0x800D8570, stride = 0x118;
    if (actor < actors || (actor - actors) % stride ||
        (actor - actors) / stride >= kMaximumRacers) return nullptr;
    return &planned[(actor - actors) / stride];
}

unsigned spheres_for(unsigned char *m, unsigned bike,
                     std::array<course_walls::Sphere, 3> &spheres) {
    const unsigned count = word(m, bike + 0x134);
    if (!count || count > spheres.size()) return 0;
    const Vec origin = vector(m, bike + 0x16C), x = vector(m, bike + 0x220),
              y = vector(m, bike + 0x238), z = vector(m, bike + 0x22C);
    if (!finite(origin) || !finite(x) || !finite(y) || !finite(z)) return 0;
    for (unsigned i = 0; i < count; ++i) {
        const Vec local = vector(m, bike + 0x138 + i * 12);
        for (unsigned k = 0; k < 3; ++k)
            spheres[i].center[k] =
                ((local[0] * x[k] + local[1] * y[k]) + local[2] * z[k]) + origin[k];
        spheres[i].radius = real(m, bike + 0x15C + i * 4);
        if (!finite(spheres[i].center) || !std::isfinite(spheres[i].radius) ||
            spheres[i].radius <= 0 || spheres[i].radius > 10) return 0;
    }
    return count;
}

float deceleration_for(unsigned char *m, unsigned bike, unsigned profile) {
    // Original55660 tyre grip, bike bias and per-rider braking profile.
    return ((real(m, bike + 0x9C) * real(m, 0x80005050) + real(m, 0x80005054)) *
        real(m, 0x8009F260)) * std::min(real(m, bike + 0x3E0), real(m, bike + 0x318)) *
        real(m, profile + 0x18);
}

Guidance *guidance_for(unsigned actor) {
    constexpr unsigned actors = 0x800D8570, stride = 0x118;
    if (actor < actors || (actor - actors) % stride ||
        (actor - actors) / stride >= kMaximumRacers)
        return nullptr;
    return &guidance[(actor - actors) / stride];
}

// Imported routes contain many short curves. Native4F9C0 also inserts connector
// curves between them. Position can be continuous while the signed radius from
//17204 reverses at the join; the native speed-squared feedforward then demands
// opposite steering without the rider having moved. Estimate this one guidance
// term over a finite distance, using the actual native cache and its trimmed
// parameter intervals (not donor record indices). Wheel/grip limits, damping,
// neighbour avoidance, crash handling and race progress remain native.
bool route_turn_radius(unsigned char *m, unsigned stack, float speed, float &radius) {
    constexpr unsigned stride = 0x50;
    const unsigned cache = word(m, 0x800A21C8), count = word(m, 0x800A21C4),
                   wrap = word(m, 0x800A21C0);
    if (!valid_guest_range(stack, 0x64) || count < 4 || count > 8196 ||
        !wrap || wrap > count || !valid_guest_range(cache, count * stride) ||
        !std::isfinite(speed) || speed < 0 || speed > 1000)
        return false;
    unsigned segment = word(m, stack + 0x58);
    double parameter = real(m, stack + 0x60);
    if (segment >= count || !std::isfinite(parameter)) return false;
    using Point = std::array<double, 2>;
    struct Curve { std::array<Point, 3> p; double lo, hi; };
    const auto read = [&](unsigned index, Curve &curve) {
        const unsigned address = cache + index * stride;
        for (unsigned i = 0; i < 3; ++i) for (unsigned k = 0; k < 2; ++k) {
            curve.p[i][k] = real(m, address + i * 8 + k * 4);
            if (!std::isfinite(curve.p[i][k])) return false;
        }
        curve.lo = real(m, address + 0x1C);
        curve.hi = real(m, address + 0x20);
        return std::isfinite(curve.lo) && std::isfinite(curve.hi) &&
               0 <= curve.lo && curve.lo <= curve.hi && curve.hi <= 1;
    };
    const auto point = [](const Curve &curve, double t) {
        const double u = 1 - t;
        Point p{};
        for (unsigned k = 0; k < 2; ++k)
            p[k] = u*u*curve.p[0][k] + 2*u*t*curve.p[1][k] + t*t*curve.p[2][k];
        return p;
    };
    Curve initial;
    if (!read(segment, initial) || parameter < initial.lo - 1e-5 ||
        parameter > initial.hi + 1e-5) return false;
    parameter = std::clamp(parameter, initial.lo, initial.hi);
    const Point middle = point(initial, parameter);
    // A spatial window has no retained steering history to drag through a
    // crash, remount, owner change or pause. At racing speed it spans several
    // short donor/connector pieces instead of chasing each one's curvature.
    const double distance = std::clamp(double(speed) * .35, 4.0, 20.0);
    const auto advance = [&](int direction, Point &result) {
        unsigned index = segment;
        Curve curve = initial;
        double t = parameter, remaining = distance;
        Point here = middle;
        // Eight chords per trimmed quadratic; at most96 chords/boundaries in
        // either direction. Degenerate or corrupt paths fall back to native.
        for (unsigned step = 0; step < 96; ++step) {
            const double edge = direction > 0 ? curve.hi : curve.lo;
            if (std::abs(t - edge) < 1e-9 || curve.hi - curve.lo < 1e-9) {
                // Native cache construction appends the wrapped approach.
                // Follow it continuously instead of jumping to curve0's
                // earlier starting-grid point in the middle of this sample.
                if (direction > 0 && index + 1 >= count) return false;
                index = direction > 0 ? index + 1 : (index ? index - 1 : wrap - 1);
                if (!read(index, curve)) return false;
                t = direction > 0 ? curve.lo : curve.hi;
                const Point joined = point(curve, t);
                // Do not bridge unrelated/incomplete cache entries, including
                // a nonclosed route that happens to have a wrap index.
                if (std::hypot(joined[0]-here[0], joined[1]-here[1]) > .1) return false;
                here = joined;
                continue;
            }
            const double fraction = (t - curve.lo) / (curve.hi - curve.lo) * 8;
            const double grid = direction > 0 ? std::floor(fraction + 1e-8) + 1 :
                                               std::ceil(fraction - 1e-8) - 1;
            const double next_t = curve.lo + std::clamp(grid, 0.0, 8.0) *
                                              (curve.hi - curve.lo) / 8;
            const Point next = point(curve, next_t);
            const double length = std::hypot(next[0]-here[0], next[1]-here[1]);
            if (!std::isfinite(length)) return false;
            if (length >= remaining && length > 1e-9) {
                for (unsigned k = 0; k < 2; ++k)
                    result[k] = here[k] + (next[k]-here[k]) * remaining / length;
                return true;
            }
            remaining -= length;
            here = next;
            t = next_t;
        }
        return false;
    };
    Point before{}, after{};
    if (!advance(-1, before) || !advance(1, after)) return false;
    const Point a{middle[0]-before[0], middle[1]-before[1]},
                b{after[0]-middle[0], after[1]-middle[1]};
    const double denominator = std::hypot(a[0], a[1]) * std::hypot(b[0], b[1]) *
                               std::hypot(after[0]-before[0], after[1]-before[1]);
    if (!(denominator > 1e-6) || !std::isfinite(denominator)) return false;
    const double curvature = 2 * (a[0]*b[1] - a[1]*b[0]) / denominator;
    if (!std::isfinite(curvature)) return false;
    // Native17204 uses zero as the straight-line sentinel, not infinity.
    radius = std::abs(curvature) < 1e-6 ? 0.f : float(1 / curvature);
    return std::isfinite(radius);
}

void fit_corridor(unsigned char *m, recomp_context &c, unsigned kind) {
    if (kind > 1)
        return;
    const unsigned actor = unsigned(kind ? c.r18 : c.r17),
                   profile = unsigned(kind ? c.r23 : c.r20);
    auto *permission = guidance_for(actor);
    if (!permission)
        return;
    // A new pass revokes old permission even after a pause, crash, owner change
    // or course unload. Invalid callbacks cannot leave a later steering pass
    // believing it came through the unobstructed branch.
    if (kind == 0)
        *permission = {};
    if (!eligible_racer(m, actor) || !valid_guest_range(profile, 0x28) ||
        word(m, actor + 0xF8) != profile) {
        *permission = {};
        return;
    }
    auto &pass = *permission;
    if (kind == 0) pass = {m, actor, profile, false};
    unsigned left_address, right_address;
    if (kind == 0) {
        const unsigned steering = unsigned(c.r16);
        if (!valid_guest_range(steering, 0xB4) || word(m, actor + 0x104) != steering)
            return;
        left_address = steering + 0xA4;
        right_address = steering + 0xB0;
    } else {
        const unsigned stack = unsigned(c.r29);
        if (!valid_guest_range(stack, 0x17C))
            return;
        left_address = stack + 0x10C;
        right_address = stack + 0x110;
    }
    // The native clamp subtracts profile clearance from both sides. Imported
    // corridors can be narrower than twice that margin, inverting its bounds
    // and choosing a target outside the lane. Adapt only temporary f2, never
    // widen the authored road or overwrite a shared AI profile.
    const float left = real(m, left_address), right = real(m, right_address),
                margin = c.f2.fl;
    if (!std::isfinite(left) || !std::isfinite(right) || right < left ||
        !std::isfinite(margin) || margin < 0)
        return;
    float half_width = (right - left) * .5f;
    if (!std::isfinite(half_width))
        return;
    if (kind == 1 && pass.memory == m && pass.actor == actor && pass.profile == profile)
        pass.unobstructed = true;
    // Preserve the native single-precision operation order. With translated,
    // asymmetric bounds the rounded midpoint can otherwise invert by one ULP.
    if (left + half_width > right - half_width)
        half_width = std::nextafter(half_width, 0.f);
    if (margin > half_width)
        c.f2.fl = half_width;
    // 5384C still chooses lateral targets; 52BFC still blends avoidance and
    // 4EB6C still calculates steering through native bike physics. No actor
    // position, velocity, progress or input is forced onto the route.
}

void relax_line(unsigned char *m, recomp_context &c) {
    const unsigned actor = unsigned(c.r19), ai = unsigned(c.r20), projection = unsigned(c.r18);
    auto *permission = guidance_for(actor);
    if (!permission)
        return;
    const Guidance pass = *permission;
    *permission = {}; // Consume even when eligibility/owner validation fails.
    auto *control = plan_for(actor);
    const PlannedControl previous = control ? *control : PlannedControl{};
    if (control) *control = {};
    if (!eligible_racer(m, actor) || !valid_guest_range(ai, 0xB4) ||
        !valid_guest_range(projection, 0x20) || word(m, actor + 0x104) != ai ||
        word(m, actor + 0xF0) != projection) {
        if (control) *control = {};
        return;
    }
    const bool clear = pass.memory == m && pass.actor == actor && pass.unobstructed &&
                       pass.profile == word(m, actor + 0xF8);
    const float left = real(m, ai + 0xA4), right = real(m, ai + 0xB0),
                current = real(m, projection + 0xC), target = real(m, ai + 0x94), error = c.f12.fl;
    if (!std::isfinite(left) || !std::isfinite(right) || !std::isfinite(current) ||
        !std::isfinite(target) || !std::isfinite(error) || right <= left)
        return;
    const float half_width = (right - left) * .5f;
    const unsigned bike = word(m, actor + 0xE0), tick = word(m, 0x800A182C);
    const Vec velocity = vector(m, bike + 0x178);
    if (!std::isfinite(half_width) || !finite(velocity))
        return;
    float radius = c.f0.fl;
    if (std::isfinite(radius) && route_turn_radius(m, unsigned(c.r29),
            std::hypot(velocity[0], velocity[1]), radius))
        c.f0.fl = radius;
    // MK64 navigation widths describe the donor's racing line, not a physical
    // rail. Give native momentum room beyond those widths; farther away,
    // request the nearest edge of a broad guidance band rather than snapping
    // the steering spring back to the exact preferred centre line.
    const float extra = std::clamp(std::hypot(velocity[0], velocity[1]) * .2f, 2.f, 8.f);
    const float broad_left = left - extra, broad_right = right + extra;
    std::array<course_walls::Sphere, 3> spheres{};
    const unsigned count = spheres_for(m, bike, spheres);
    const auto *walls = course_walls::world();
    const auto *hazards = course_hazards::data();
    if (control && count && (walls || hazards)) {
        const float previous_side = previous.memory == m && previous.actor == actor &&
            previous.bike == bike && tick - previous.tick <= 2 ? previous.side : 0;
        const auto state = hazards ? course_hazards::capture_state() : netplay::CourseHazardState{};
        // projection+14 is the native signed lateral normal used by4EB6C's
        // velocity damping. The same basis means a positive target has the
        // same steering sign as the original neighbour-avoidance branch.
        const Vec normal{real(m, projection + 0x14), real(m, projection + 0x18), 0};
        const auto decision = choose(walls, course_walls::surface_world(), hazards, state,
            {spheres.data(), count}, velocity, vector(m, bike + 0x220), normal,
            deceleration_for(m, bike, word(m, actor + 0xF8)),
            broad_left - current, broad_right - current, previous_side);
        *control = {m, actor, bike, tick, decision,
                    decision.steer ? decision.lateral : previous_side};
        if (decision.steer) {
            c.f12.fl = -decision.lateral;
            return; // Retain complete native steering response to an obstacle.
        }
    } else if (control) *control = {};
    if (!clear || target < left || target > right) return;
    const float nearest = std::clamp(current, broad_left, broad_right);
    c.f12.fl = current - nearest;
}

void avoid(unsigned char *m, const recomp_context &c) {
    const unsigned actor = unsigned(c.r19), stack = unsigned(c.r29);
    if (!eligible_racer(m, actor) || !valid_guest_range(stack, 0xA0))
        return;
    const unsigned bike = word(m, actor + 0xE0), profile = word(m, actor + 0xF8);
    const auto *walls = course_walls::world();
    const auto *hazards = course_hazards::data();
    if (!walls && !hazards)
        return;
    std::array<course_walls::Sphere, 3> spheres{};
    const unsigned count = spheres_for(m, bike, spheres);
    if (!count) return;
    const float target = real(m, stack + 0x64);
    if (!std::isfinite(target))
        return;
    auto *control = plan_for(actor);
    Decision decision;
    if (control && control->memory == m && control->actor == actor &&
        control->bike == bike && control->tick == word(m, 0x800A182C)) {
        decision = control->decision;
    } else {
        const auto state = hazards ? course_hazards::capture_state() : netplay::CourseHazardState{};
        decision = inspect(walls, hazards, state, {spheres.data(), count},
            vector(m, bike + 0x178), vector(m, bike + 0x220), deceleration_for(m, bike, profile));
    }
    // An alternate line was already fed into this frame's native steering.
    // Preserve native corner/traffic braking; add an emergency stop only when
    // every physically swept alternative is blocked or unsupported.
    if (decision.brake) {
        write_float(m, stack + 0x64, std::min(target, 0.f));
        write_u16(m, stack + 0x48, std::uint16_t(half(m, stack + 0x48) | 0x10));
    }
}

void reacquire(unsigned char *m, recomp_context &c) {
    const unsigned actor = unsigned(c.r30);
    // Original5A9DC admits only forward-facing curves ahead of saved progress.
    // A crash can leave an attached, healthy AI facing backwards or behind that
    // search window. It then returns -1 on every retry. Reacquire navigation
    // from physical position only after that exact failure, like a rider
    // looking for the track; do not reset the crash animation or move the bike.
    if (unsigned(c.r2) != 0xFFFFFFFF || !eligible_racer(m, actor)) return;
    const auto *data = experimental_course::route_data();
    if (!data || !data->records_be || !data->record_heights ||
        data->height_count != data->record_count || data->record_count < 5 ||
        data->record_count > 8192 || data->byte_count != data->record_count * 16 ||
        data->wrap_segment < 2 || data->wrap_segment + 2 >= data->record_count ||
        (data->wrap_segment & 1)) return;
    const unsigned bike = word(m, actor + 0xE0), course_projection = word(m, actor + 0xEC);
    if (!valid_guest_range(course_projection, 12)) return;
    const Vec here = vector(m, bike + 0x16C);
    if (!finite(here)) return;
    const auto number = [](const std::uint8_t *p) {
        return std::bit_cast<float>((unsigned(p[0]) << 24) | (unsigned(p[1]) << 16) |
            (unsigned(p[2]) << 8) | unsigned(p[3]));
    };
    double best = std::numeric_limits<double>::infinity();
    unsigned segment = 0;
    // Bounded immutable curve search, invoked only on native reacquisition
    // failure (the stock retry is staggered across the fourteen rider slots).
    // Include height to distinguish crossed/stacked routes. Duplicate approach
    // segment0 is represented by its reachable seam at wrap_segment.
    for (unsigned s = 2; s <= data->wrap_segment; s += 2) {
        std::array<Vec, 3> points{};
        bool valid = true;
        for (unsigned i = 0; i < 3; ++i) {
            const auto *p = data->records_be + (s + i) * 16;
            points[i] = {number(p + 8), number(p + 12), data->record_heights[s + i]};
            valid &= finite(points[i]);
        }
        if (!valid) continue;
        const auto error = [&](double t) {
            const double u = 1 - t;
            double sum = 0;
            for (unsigned k = 0; k < 3; ++k) {
                const double d = u*u*points[0][k] + 2*u*t*points[1][k] +
                    t*t*points[2][k] - here[k];
                sum += d*d;
            }
            return sum;
        };
        double local = std::numeric_limits<double>::infinity(), parameter = 0;
        for (unsigned sample = 0; sample <= 8; ++sample) {
            const double t = sample / 8.0, e = error(t);
            if (e < local) { local = e; parameter = t; }
        }
        double lo = std::max(0.0, parameter - .125), hi = std::min(1.0, parameter + .125);
        for (unsigned iteration = 0; iteration < 12; ++iteration) {
            const double a = (2*lo+hi)/3, b = (lo+2*hi)/3;
            if (error(a) < error(b)) hi = b; else lo = a;
        }
        local = std::min(local, error((lo+hi)*.5));
        if (local < best) { best = local; segment = s; }
    }
    if (!segment || !std::isfinite(best)) return;
    // This is the same transient field native5ABB8 writes on success. Native
    //4F658/52038 still perform actual curve projection and lap/progress rules.
    write_u32(m, course_projection, segment);
    c.r2 = segment;
}
}
}
extern "C" void rr64_course_ai_avoid(unsigned char *m, void *context) {
    if (context)
        rr64::course_ai::avoid(m, *static_cast<recomp_context *>(context));
}

extern "C" void rr64_course_ai_fit_corridor(unsigned char *m, void *context, unsigned kind) {
    if (context)
        rr64::course_ai::fit_corridor(m, *static_cast<recomp_context *>(context), kind);
}

extern "C" void rr64_course_ai_relax_line(unsigned char *m, void *context) {
    if (context)
        rr64::course_ai::relax_line(m, *static_cast<recomp_context *>(context));
}

extern "C" void rr64_course_ai_reacquire(unsigned char *m, void *context) {
    if (context)
        rr64::course_ai::reacquire(m, *static_cast<recomp_context *>(context));
}
