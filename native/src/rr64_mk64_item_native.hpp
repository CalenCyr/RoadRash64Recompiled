#pragma once

#include "rr64_engine_layout.hpp"
#include <algorithm>
#include <array>
#include <bit>
#include <cmath>

namespace rr64::mk64_items::native {
using Vec = std::array<float, 3>;
constexpr unsigned actors = 0x800D8570u, actor_stride = 0x118u;
inline unsigned word(unsigned char *m, unsigned a) {
    unsigned value = 0;
    engine::read_u32(m, a, value);
    return value;
}
inline unsigned half(unsigned char *m, unsigned a) {
    std::uint16_t value = 0;
    engine::read_u16(m, a, value);
    return value;
}
inline float scalar(unsigned char *m, unsigned a) {
    float value = 0;
    engine::read_float(m, a, value);
    return value;
}
inline bool vector(unsigned char *m, unsigned a, Vec &value) {
    for (unsigned axis = 0; axis < 3; ++axis)
        if (!engine::read_float(m, a + axis * 4, value[axis]) || !std::isfinite(value[axis]))
            return false;
    return true;
}
inline void put(unsigned char *m, unsigned a, const Vec &value) {
    for (unsigned axis = 0; axis < 3; ++axis)
        engine::write_float(m, a + axis * 4, value[axis]);
}
struct Pair {
    unsigned actor = 0, bike = 0, rider = 0, route = 0, model = 0, character = 0;
    bool operator==(const Pair &) const = default;
};
inline Pair pair(unsigned char *m, unsigned slot) {
    using namespace engine;
    Pair p;
    if (!m || slot >= kMaximumRacers)
        return p;
    p.actor = actors + slot * actor_stride;
    p.bike = word(m, p.actor + 0xE0);
    p.rider = word(m, p.actor + 0xE4);
    p.route = word(m, p.actor + 0xE8);
    p.model = word(m, p.actor + 0x18);
    p.character = word(m, p.actor + 0x1C);
    if (!half(m, p.actor + 0x24) || !valid_guest_range(p.bike, bike::stride) ||
        !valid_guest_range(p.rider, rider::stride) || !valid_guest_range(p.route, 0x64) ||
        word(m, p.bike + 4) != p.actor || word(m, p.rider + 4) != p.actor ||
        word(m, p.bike + bike::rider_pointer) != p.rider ||
        word(m, p.rider + rider::bike_pointer) != p.bike)
        return {};
    return p;
}
inline bool mounted(unsigned char *m, const Pair &p) {
    return p.actor && half(m, p.bike + engine::bike::rider_attached) &&
           half(m, p.rider + engine::rider::bike_attached) &&
           !half(m, p.rider + engine::rider::ejected) &&
           !half(m, p.bike + engine::bike::drive_control_lockout);
}

struct ContactSphere {
    Vec center{};
    float radius = 0;
};
struct Contacts {
    std::array<ContactSphere, 6> spheres{};
    unsigned count = 0;
};

// Match native4E754's current contact centers without changing its cache. Bike
// and mounted rider bodies each author up to three spheres in the same world
// units as the imported course. The local X/Y/Z basis order is 118/130/124.
inline bool contact_spheres(unsigned char *m, const Pair &p, Contacts &out) noexcept {
    out = {};
    if (!mounted(m, p))
        return false;
    Contacts result;
    for (const unsigned body : {p.bike + 0x108, p.rider + 0x28}) {
        if (!engine::valid_guest_range(body, 0x200))
            return false;
        const unsigned count = word(m, body + 0x2C);
        if (!count || count > 3)
            return false;
        Vec origin{};
        std::array<Vec, 3> basis{};
        if (!vector(m, body + 0x64, origin) || !vector(m, body + 0x118, basis[0]) ||
            !vector(m, body + 0x130, basis[1]) || !vector(m, body + 0x124, basis[2]))
            return false;
        for (const auto &axis : basis) {
            float norm = 0;
            for (float component : axis)
                norm += component * component;
            if (!std::isfinite(norm) || norm < .01f || norm > 4)
                return false;
        }
        for (unsigned i = 0; i < count; ++i) {
            Vec local{};
            const float radius = scalar(m, body + 0x54 + i * 4);
            if (!vector(m, body + 0x30 + i * 12, local) || !std::isfinite(radius) || radius <= 0 ||
                radius > 20)
                return false;
            for (float component : local)
                if (std::abs(component) > 20)
                    return false;
            auto &sphere = result.spheres[result.count++];
            sphere.center = origin;
            sphere.radius = radius;
            for (unsigned world = 0; world < 3; ++world) {
                sphere.center[world] += local[0] * basis[0][world] + local[1] * basis[1][world] +
                                        local[2] * basis[2][world];
                if (!std::isfinite(sphere.center[world]))
                    return false;
            }
        }
    }
    out = result;
    return true;
}

// Inverse of native3A098..3A0C0: RPM = wheel radians/s * final drive *
// selected gear * (60 / 2pi). Maximum RPM and highest gear are authored bike
// specifications; neither live RPM nor an already-boosted velocity is a rating.
inline bool rated_speed(unsigned char *m, const Pair &p, float &speed) {
    if (!p.actor || !engine::valid_guest_range(p.bike, engine::bike::stride))
        return false;
    const unsigned gear = word(m, p.bike + 0x54);
    if (!gear || gear > 6)
        return false;
    const float rpm = scalar(m, p.bike + 0xC), drive = scalar(m, p.bike + 0x4C),
                ratio = scalar(m, p.bike + 0x58 + gear * 4), radius = scalar(m, p.bike + 0x3C8),
                conversion = scalar(m, 0x80004534);
    for (const float v : {rpm, drive, ratio, radius, conversion})
        if (!std::isfinite(v) || v <= 0)
            return false;
    if (rpm > 100000 || drive > 100 || ratio > 100 || radius > 2 || conversion < 9.54f ||
        conversion > 9.56f)
        return false;
    speed = rpm * radius / (drive * ratio * conversion);
    return std::isfinite(speed) && speed >= 1 && speed <= 300;
}

// Shared by native4E754's contact-sphere producer and the imported wall sweep.
// The native body has at most three local XYZ centers followed by three radii.
// Store original words rather than invert a factor at expiry, avoiding drift.
struct Geometry {
    unsigned body = 0, count = 0;
    std::array<unsigned, 12> original{}, written{};
    bool active = false;
    bool operator==(const Geometry &) const = default;
    void restore(unsigned char *m) {
        if (!active)
            return;
        if (!m || !count || count > 3 || !engine::valid_guest_range(body, 0x200)) {
            *this = {};
            return;
        }
        for (unsigned i = 0; i < count * 4; ++i) {
            const unsigned a = body + (i < count * 3 ? 0x30 + i * 4 : 0x54 + (i - count * 3) * 4);
            if (word(m, a) == written[i])
                engine::write_u32(m, a, original[i]);
        }
        engine::write_u16(m, body + 0x188, 0);
        *this = {};
    }
    bool scale(unsigned char *m, unsigned address, float factor, Vec shift = {}) {
        const unsigned n = word(m, address + 0x2C);
        if (!engine::valid_guest_range(address, 0x200) || !n || n > 3 || !std::isfinite(factor) ||
            factor <= 0 || factor > 1)
            return false;
        for (float v : shift)
            if (!std::isfinite(v) || std::abs(v) > 10)
                return false;
        if (active && (body != address || count != n))
            restore(m);
        std::array<unsigned, 12> base{}, target{};
        for (unsigned i = 0; i < n * 4; ++i) {
            const unsigned a = address + (i < n * 3 ? 0x30 + i * 4 : 0x54 + (i - n * 3) * 4);
            const unsigned current = word(m, a);
            // Original373F0 rebuilds geometry at detach. New native words are
            // a fresh baseline; only our exact prior write is unscaled here.
            base[i] = active && current == written[i] ? original[i] : current;
            const float value = std::bit_cast<float>(base[i]);
            if (!std::isfinite(value) || std::abs(value) > 20 || (i >= n * 3 && value <= 0))
                return false;
            target[i] = std::bit_cast<unsigned>(value * factor + (i < n * 3 ? shift[i % 3] : 0));
        }
        body = address;
        count = n;
        original = base;
        written = target;
        active = true;
        for (unsigned i = 0; i < n * 4; ++i) {
            const unsigned a = body + (i < n * 3 ? 0x30 + i * 4 : 0x54 + (i - n * 3) * 4);
            engine::write_u32(m, a, written[i]);
        }
        engine::write_u16(m, body + 0x188, 0);
        return true;
    }
};

inline Vec mounted_center_shift(unsigned char *m, const Pair &p, float factor) {
    Vec out{}, bike{}, rider{};
    if (!mounted(m, p) || !vector(m, p.bike + 0x16C, bike) || !vector(m, p.rider + 0x8C, rider))
        return out;
    // Native4E754 transforms localXYZ by body+118,+130,+124 respectively.
    // Move mounted rider collision centers about the same bike anchor as its
    // rendered root without altering the simulation pose or rider attachment.
    const unsigned body = p.rider + 0x28;
    const unsigned offsets[3] = {0x118, 0x130, 0x124};
    for (unsigned local = 0; local < 3; ++local) {
        Vec axis{};
        if (!vector(m, body + offsets[local], axis))
            return {};
        for (unsigned world = 0; world < 3; ++world)
            out[local] += (bike[world] - rider[world]) * (1 - factor) * axis[world];
    }
    return out;
}

inline bool speed(unsigned char *m, const Pair &p, float minimum, float maximum) {
    if (!mounted(m, p) || !std::isfinite(minimum) || !std::isfinite(maximum) || minimum < 0 ||
        maximum < minimum || maximum > 400)
        return false;
    Vec velocity{}, forward{}, attached{}, comparison{};
    if (!vector(m, p.bike + 0x178, velocity) || !vector(m, p.bike + 0x220, forward) ||
        !vector(m, p.rider + 0x98, attached) || !vector(m, p.rider + 0xC0, comparison))
        return false;
    const float horizontal = std::hypot(velocity[0], velocity[1]);
    const float target = std::clamp(horizontal, minimum, maximum);
    if (target == horizontal)
        return false;
    Vec change{};
    if (horizontal > .1f) {
        for (unsigned axis = 0; axis < 2; ++axis)
            change[axis] = velocity[axis] * (target / horizontal - 1);
    } else {
        const float norm = std::hypot(forward[0], forward[1]);
        if (norm < .1f)
            return false;
        for (unsigned axis = 0; axis < 2; ++axis)
            change[axis] = forward[axis] * target / norm - velocity[axis];
    }
    //36B78 compares body velocity to rider+C0. Item propulsion alters all three
    // baselines by the same delta; a later real collision still causes a crash.
    for (unsigned axis = 0; axis < 3; ++axis) {
        velocity[axis] += change[axis];
        attached[axis] += change[axis];
        comparison[axis] += change[axis];
    }
    put(m, p.bike + 0x178, velocity);
    put(m, p.rider + 0x98, attached);
    put(m, p.rider + 0xC0, comparison);
    engine::write_float(m, p.bike + 0x184,
                        std::sqrt(velocity[0] * velocity[0] + velocity[1] * velocity[1] +
                                  velocity[2] * velocity[2]));
    engine::write_u16(m, p.bike + 0x168, half(m, p.bike + 0x168) & ~1u);
    return true;
}
} // namespace rr64::mk64_items::native
