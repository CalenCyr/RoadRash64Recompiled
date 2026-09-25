#include "rr64_course_boost.hpp"
#include "rr64_engine_layout.hpp"
#include "rr64_experimental_course.hpp"
#include "rr64_netplay.hpp"
#include "rr64_prediction_replay.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <stdexcept>

namespace rr64::course_boost {
namespace {
using namespace engine;
bool finite(Vec v) noexcept {
    return std::all_of(v.begin(), v.end(), [](float n) { return std::isfinite(n); });
}
float dot(Vec a, Vec b) noexcept {
    return a[0] * b[0] + a[1] * b[1] + a[2] * b[2];
}
Vec subtract(Vec a, Vec b) noexcept {
    return {a[0] - b[0], a[1] - b[1], a[2] - b[2]};
}
bool height_at(const std::array<Vec, 3> &t, Vec p, float &height) noexcept {
    const auto &a = t[0], &b = t[1], &c = t[2];
    const double den = double(b[1] - c[1]) * (a[0] - c[0]) + double(c[0] - b[0]) * (a[1] - c[1]);
    if (std::abs(den) < 1e-7)
        return false;
    const double u =
        (double(b[1] - c[1]) * (p[0] - c[0]) + double(c[0] - b[0]) * (p[1] - c[1])) / den;
    const double v =
        (double(c[1] - a[1]) * (p[0] - c[0]) + double(a[0] - c[0]) * (p[1] - c[1])) / den;
    if (u < -.0001 || v < -.0001 || u + v > 1.0001)
        return false;
    height = float(u * a[2] + v * b[2] + (1 - u - v) * c[2]);
    return true;
}
struct State {
    unsigned char *memory = nullptr;
    const Data *course = nullptr;
    unsigned bike = 0, recovery = 0, pad = ~0u;
    float fired = -100, latest = 0;
};
std::array<State, kMaximumRacers> states{};
unsigned reports = 0;
unsigned word(unsigned char *m, unsigned a) noexcept {
    unsigned v = 0;
    read_u32(m, a, v);
    return v;
}
unsigned half(unsigned char *m, unsigned a) noexcept {
    std::uint16_t v = 0;
    read_u16(m, a, v);
    return v;
}
float scalar(unsigned char *m, unsigned a) noexcept {
    float v = 0;
    read_float(m, a, v);
    return v;
}
Vec vector(unsigned char *m, unsigned a) noexcept {
    return {scalar(m, a), scalar(m, a + 4), scalar(m, a + 8)};
}
void put(unsigned char *m, unsigned a, Vec v) noexcept {
    for (unsigned i = 0; i < 3; ++i)
        write_float(m, a + i * 4, v[i]);
}
}
void validate(const Data &d) {
    if (d.pads.size() > 32)
        throw std::runtime_error("too many course boost pads");
    unsigned next = 0;
    for (const auto &p : d.pads) {
        if (p.id != next++ || p.triangles.empty() || p.triangles.size() > 64 ||
            !finite(p.direction) || !finite(p.lip) || std::abs(p.direction[2]) > 1e-6f ||
            std::abs(dot(p.direction, p.direction) - 1) > 1e-4f || !std::isfinite(p.slope) ||
            p.slope <= 0 || p.slope > 1 || !std::isfinite(p.minimum_speed) || p.minimum_speed < 5 ||
            p.minimum_speed > 180 || !std::isfinite(p.length) || p.length <= .1f || p.length > 300)
            throw std::runtime_error("invalid course boost pad");
        for (const auto &t : p.triangles) {
            for (auto v : t)
                if (!finite(v) || std::abs(v[0]) > 8750 || std::abs(v[1]) > 8750 ||
                    std::abs(v[2]) > 8192)
                    throw std::runtime_error("invalid boost triangle vertex");
            float h = 0;
            Vec center{};
            for (unsigned k = 0; k < 3; ++k)
                center[k] = (t[0][k] + t[1][k] + t[2][k]) / 3;
            if (!height_at(t, center, h))
                throw std::runtime_error("vertical boost triangle");
        }
    }
}
void reset_runtime() noexcept {
    states = {};
    reports = 0;
}
Result launch(const Data &d, Vec p, Vec velocity, float dt) noexcept {
    if (!finite(p) || !finite(velocity) || !std::isfinite(dt) || dt <= 0 || dt > .25f)
        return {};
    const float speed = std::hypot(velocity[0], velocity[1]);
    if (speed < 2 || speed > 300)
        return {};
    for (const auto &pad : d.pads) {
        const float along = dot(velocity, pad.direction);
        if (along < speed * .7f)
            continue; // Backwards/sideways arrivals are ordinary contact.
        const float distance = -dot(subtract(p, pad.lip), pad.direction);
        // Apply at the lip, not at the base of the ramp. Include the next native
        // step so fast bikes cannot jump over a narrow final strip.
        if (distance < -.1f || distance > std::max(.35f, along * dt + .1f))
            continue;
        bool supported = false;
        for (const auto &t : pad.triangles) {
            float ground = 0;
            if (height_at(t, p, ground) && p[2] >= ground - .3f && p[2] <= ground + 1.6f) {
                supported = true;
                break;
            }
        }
        if (!supported)
            continue;
        // Propulsion preserves the incoming heading/lateral intent. Upward
        // speed comes from the authored ramp slope, never a target-position warp.
        const float target = std::max(speed, pad.minimum_speed);
        const float vertical = std::max(velocity[2], target * pad.slope);
        Result r{true,
                 pad.id,
                 {velocity[0] * (target / speed - 1), velocity[1] * (target / speed - 1),
                  vertical - velocity[2]}};
        if (!finite(r.delta))
            return {};
        return r;
    }
    return {};
}
} // namespace rr64::course_boost

extern "C" void rr64_course_boost_step(unsigned char *m, unsigned b) {
#ifdef RR64_EXPERIMENTAL_COURSE
    using namespace rr64;
    using namespace engine;
    using namespace course_boost;
    const auto *d = data();
    if (!m || !d || d->pads.empty() || !experimental_course::active() || prediction::active() ||
        half(m, globals::gameplay_pause_state) || !valid_guest_range(b, bike::stride))
        return;
    const auto rules = netplay::get_physics_rules();
    if (rules.active && (!rules.connected || !rules.authoritative || !rules.is_host ||
                         rules.phase != netplay::Phase::Race))
        return;
    if (!is_live_race_transition(word(m, globals::main_mode), word(m, globals::pending_mode)))
        return;
    const unsigned actor = word(m, b + 4);
    if (actor < 0x800D8570u || (actor - 0x800D8570u) % 0x118u)
        return;
    const unsigned slot = (actor - 0x800D8570u) / 0x118u;
    if (slot >= kMaximumRacers || !half(m, actor + 0x24) || word(m, actor + 0xE0) != b)
        return;
    const unsigned r = word(m, actor + 0xE4), route = word(m, actor + 0xE8);
    if (!valid_guest_range(r, rider::stride) || !valid_guest_range(route, 0x64) ||
        word(m, r + 4) != actor || word(m, b + bike::rider_pointer) != r ||
        word(m, r + rider::bike_pointer) != b)
        return;
    auto &s = states[slot];
    if (!half(m, b + bike::rider_attached) || !half(m, r + rider::bike_attached) ||
        half(m, r + rider::ejected) || half(m, route + 0x4C) || half(m, route + 0x4E) ||
        half(m, route + 0x50)) {
        s = {};
        return;
    }
    const float elapsed = scalar(m, 0x800D7670u), dt = scalar(m, globals::physics_delta);
    if (!std::isfinite(elapsed) || elapsed < 0)
        return;
    const unsigned recovery = word(m, route + 0x40);
    if (s.memory != m || s.course != d || s.bike != b || s.recovery != recovery ||
        elapsed < s.latest)
        s = {m, d, b, recovery, ~0u, -100, elapsed};
    s.latest = elapsed;
    const auto result = launch(*d, vector(m, b + 0x16C), vector(m, b + 0x178), dt);
    if (!result.applied || (s.pad == result.pad && elapsed - s.fired < 1.0f))
        return;
    // Preserve only the added propulsion in the attached body's comparison
    // baselines. Any real wall/vehicle impulse applied by the ensuing native
    // integration remains a difference, and36B78 still detaches the rider.
    for (unsigned a : {b + 0x178u, r + 0x98u, r + 0xC0u})
        if (!finite(vector(m, a)))
            return;
    for (unsigned a : {b + 0x178u, r + 0x98u, r + 0xC0u}) {
        auto v = vector(m, a);
        for (unsigned i = 0; i < 3; ++i)
            v[i] += result.delta[i];
        put(m, a, v);
    }
    const auto v = vector(m, b + 0x178u);
    write_float(m, b + 0x184u, std::sqrt(dot(v, v)));
    // Native34594's resting branch otherwise replaces the velocity with a
    // static contact impulse. Wake this same body, as the stock rail jump does.
    write_u16(m, b + 0x168u, half(m, b + 0x168u) & ~1u);
    s.pad = result.pad;
    s.fired = elapsed;
    if (reports < 64 && std::getenv("RR64_PRIVATE_COURSE_DIAGNOSTICS")) {
        ++reports;
        std::fprintf(
            stderr,
            "[RR64-COURSE-BOOST] actor=%u pad=%u speed=%.3f vertical=%.3f delta=%.5f,%.5f,%.5f\n",
            slot, result.pad, std::hypot(v[0], v[1]), v[2], result.delta[0], result.delta[1],
            result.delta[2]);
    }
#else
    (void)m;
    (void)b;
#endif
}
