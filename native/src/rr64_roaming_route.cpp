#include "rr64_roaming_route.hpp"
#include "rr64_engine_layout.hpp"
#include "rr64_prediction_rules.hpp"
#include <algorithm>
#include <cmath>
#include <limits>

namespace {
using namespace rr64::engine;
// USA route records alternate endpoints and quadratic control points.
constexpr unsigned actor_base = 0x800D8570;
constexpr unsigned actor_stride = 0x118;
constexpr unsigned route_record_bytes = 16;
constexpr unsigned curve_samples = 32;
constexpr unsigned refinement_steps = 24;
unsigned word(unsigned char *m, unsigned p) {
    unsigned v = 0;
    read_u32(m, p, v);
    return v;
}
float number(unsigned char *m, unsigned p) {
    float v = 0;
    read_float(m, p, v);
    return v;
}
bool is_human_rider(unsigned char *m, unsigned actor) {
    constexpr unsigned first = actor_base;
    if (actor < first || (actor - first) % actor_stride ||
        (actor - first) / actor_stride >= kMaximumRacers)
        return false;
    const auto status = rr64::prediction::status_for_rules();
    if (status.active && status.authoritative)
        return status.connected &&
               (status.authority_humans & (1u << ((actor - first) / actor_stride)));
    std::uint16_t ai = 1;
    read_u16(m, actor + 0x26, ai);
    return !ai && word(m, actor + 8) < 4;
}
bool read_course(unsigned char *m, unsigned &base, unsigned &count) {
    base = word(m, 0x800A6544);
    count = word(m, 0x800A6540);
    return count >= 3 && count <= 8192 && valid_guest_range(base, count * route_record_bytes);
}
struct Point {
    double x, y;
};
Point curve(Point a, Point b, Point c, double t) {
    const double u = 1 - t;
    return {u * u * a.x + 2 * t * u * b.x + t * t * c.x,
            u * u * a.y + 2 * t * u * b.y + t * t * c.y};
}
double squared_distance(Point p, Point q) {
    return (p.x - q.x) * (p.x - q.x) + (p.y - q.y) * (p.y - q.y);
}
} // namespace

// The stock physical-route search (507B0) can advance but returns failure for
// a negative curve parameter. Walk backwards too, with the same bounded search
// budget. Without this, 674C4 never receives a new segment after backtracking.
extern "C" int rr64_roaming_previous_segment(unsigned char *m, void *opaque) {
    auto &c = *static_cast<recomp_context *>(opaque);
    unsigned base, count;
    const unsigned route = unsigned(c.r18);
    if (!valid_guest_range(route, 4) || !read_course(m, base, count) || word(m, 0x800D763C) != 0 ||
        !(c.f0.fl < c.f22.fl) || unsigned(c.r16) >= 5)
        return 0;
    const unsigned segment = word(m, route);
    if (segment < 2 || (segment & 1) || segment > count - 3)
        return 0;
    for (unsigned i = 0; i < kMaximumRacers; ++i) {
        const unsigned actor = actor_base + i * actor_stride;
        if (word(m, actor + 0xEC) == route && is_human_rider(m, actor)) {
            write_u32(m, route, segment - 2);
            ++c.r16;
            return 1;
        }
    }
    return 0;
}

// 674C4 is the forward-only race-progress accumulator. Its point-to-point
// backtracking branch clamps the old segment to t=0 instead of following the
// physical route. Use the game's bidirectional accumulator (6736C) only for
// human backward transitions. Preserve circuit wrap/lap and all AI behavior.
extern "C" int rr64_roaming_reverse_route(unsigned char *m, void *opaque) {
    auto &c = *static_cast<recomp_context *>(opaque);
    unsigned base, count;
    const unsigned state = unsigned(c.r5), target = unsigned(c.r6);
    if (!valid_guest_range(state, 0x64) || !read_course(m, base, count) || unsigned(c.r4) != base ||
        word(m, 0x800D763C) != 0 || (target & 1) || target > count - 3)
        return 0;
    const unsigned old = word(m, state);
    if ((old & 1) || old > count - 3 || target >= old)
        return 0;
    for (unsigned i = 0; i < kMaximumRacers; ++i) {
        const unsigned actor = actor_base + i * actor_stride;
        if (word(m, actor + 0xE8) == state)
            return is_human_rider(m, actor);
    }
    return 0;
}

// Called inside 68E20 after its bust/wreck exclusion, before rank-based catchup.
// Select the nearest usable race-road curve from the bike's *current* position.
// The original tail still rebuilds bike/body pose, samples terrain, sets cameras
// and performs recovery. No persistent cache/state can leak between races/replay.
extern "C" int rr64_roaming_recovery_point(unsigned char *m, void *opaque) {
    auto &c = *static_cast<recomp_context *>(opaque);
    const unsigned actor = unsigned(c.r17), bike = unsigned(c.r21), sp = unsigned(c.r29);
    unsigned base, count;
    if (!is_human_rider(m, actor) || !valid_guest_range(bike, 0x868) ||
        !valid_guest_range(sp, 0xC0) || !read_course(m, base, count))
        return 0;
    const Point p{number(m, bike + 0x16C), number(m, bike + 0x170)};
    if (!std::isfinite(p.x) || !std::isfinite(p.y))
        return 0;
    double best = std::numeric_limits<double>::infinity(), best_t = 0;
    unsigned best_segment = 0;
    for (unsigned i = 0; i + 2 < count; i += 2) {
        const unsigned q = base + i * route_record_bytes;
        // Same spawnable-road/blocked-segment flags checked by 69274..692B0.
        const auto byte = [&](unsigned p) {
            std::uint8_t v = 0;
            read_u8(m, p, v);
            return v;
        };
        std::uint16_t flags = 0;
        read_u16(m, q + 0x12, flags);
        if ((byte(q) != 1 && byte(q + 0x20) != 1) || (flags & 2))
            continue;
        const Point a{number(m, q + 8), number(m, q + 12)}, b{number(m, q + 24), number(m, q + 28)},
            d{number(m, q + 40), number(m, q + 44)};
        if (!std::isfinite(a.x + a.y + b.x + b.y + d.x + d.y))
            continue;
        // Sample all intervals: curved hairpins need a global candidate, not
        // a single projection onto the endpoint chord. Refine its neighborhood.
        double local_best = std::numeric_limits<double>::infinity(), t = 0;
        for (unsigned n = 0; n <= curve_samples; ++n) {
            const double v = n / double(curve_samples),
                         dist = squared_distance(p, curve(a, b, d, v));
            if (dist < local_best) {
                local_best = dist;
                t = v;
            }
        }
        double lo = std::max(0.0, t - 1.0 / curve_samples),
               hi = std::min(1.0, t + 1.0 / curve_samples);
        for (unsigned n = 0; n < refinement_steps; ++n) {
            const double l = (2 * lo + hi) / 3, r = (lo + 2 * hi) / 3;
            if (squared_distance(p, curve(a, b, d, l)) < squared_distance(p, curve(a, b, d, r)))
                hi = r;
            else
                lo = l;
        }
        const double refined = (lo + hi) / 2, dist = squared_distance(p, curve(a, b, d, refined));
        if (dist < local_best) {
            local_best = dist;
            t = refined;
        }
        if (local_best < best) {
            best = local_best;
            best_t = t;
            best_segment = i;
        }
    }
    if (!std::isfinite(best))
        return 0;
    write_u32(m, sp + 0x58, best_segment);
    write_float(m, sp + 0x5C, float(best_t));
    c.r7 = guest_address(sp + 0x48); // Native curve output pointer at 692BC.
    c.f24.fl = 0;
    c.f28.fl = 0; // Recover at rest, not at a competitor's speed.
    return 1;
}
