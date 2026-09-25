#include "rr64_course_scene_motion.hpp"
#include "rr64_course_hazard_motion.hpp"
#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace rr64::course_hazards {
namespace {
constexpr float tau = 6.2831853071795864769f;
Vec add(Vec a, Vec b) {
    for (unsigned k = 0; k < 3; ++k)
        a[k] += b[k];
    return a;
}
Vec sub(Vec a, Vec b) {
    for (unsigned k = 0; k < 3; ++k)
        a[k] -= b[k];
    return a;
}
Vec mul(Vec a, float b) {
    for (auto &v : a)
        v *= b;
    return a;
}
float distance(Vec a, Vec b) {
    auto d = sub(a, b);
    return std::hypot(d[0], d[1]);
}
float angle(std::uint16_t value) { return float(value & 0xfff0u) * tau / 65536.f; }
std::uint16_t yaw(Vec d) { return std::uint16_t(int(std::atan2(d[0], -d[1]) * 65536.f / tau)); }
Vec forward(std::uint16_t a) { return {std::sin(angle(a)), -std::cos(angle(a)), 0}; }
Vec rotate(Vec v, std::uint16_t a) {
    float c = std::cos(angle(a)), s = std::sin(angle(a));
    return {v[0] * c - v[1] * s, v[0] * s + v[1] * c, v[2]};
}
unsigned random(std::uint32_t &v, unsigned limit) {
    v ^= v << 13;
    v ^= v >> 17;
    v ^= v << 5;
    return limit ? v % limit : 0;
}
std::uint16_t source_turn(std::uint16_t from, std::uint16_t to) {
    // Original func800417B4 quantized angle approach, including the low-byte snap.
    if ((from >> 8) == (to >> 8))
        return to;
    const std::uint16_t d = to - from;
    const int step = d < 0x400    ? 0x80
                     : d < 0x800  ? 0x200
                     : d < 0x4000 ? 0x400
                     : d < 0x8000 ? 0x700
                     : d < 0xc000 ? -0x700
                     : d < 0xf800 ? -0x400
                     : d < 0xfc00 ? -0x200
                                  : -0x80;
    return std::uint16_t(from + step);
}
const Racer *near(std::span<const Racer> racers, Vec p, float radius) {
    for (const auto &r : racers)
        if (distance(r.position, p) < radius)
            return &r;
    return nullptr;
}
// Source8008ACE0 /8008ADD0, with circular source controls and integer10000 clock.
std::pair<Vec, Vec> spline(const Path &path, unsigned node, unsigned clock) {
    const float t = float(clock) / 10000.f, t2 = t * t, t3 = t2 * t, u = 1 - t;
    const std::array<float, 4> w{u * u * u / 6, t3 * .5f - t2 + 2.f / 3,
                                 -t3 * .5f + t2 * .5f + t * .5f + 1.f / 6, t3 / 6};
    const std::array<float, 4> v{-u * u * .5f, t2 * 1.5f - 2 * t, -t2 * 1.5f + t + .5f, t2 * .5f};
    Vec p{}, d{};
    for (unsigned k = 0; k < 4; ++k) {
        const auto &q = path.points[(node + k) % path.points.size()];
        p = add(p, mul(q, w[k]));
        d = add(d, mul(q, v[k]));
    }
    return {p, d};
}
} // namespace

void SceneSimulation::reset_effect(unsigned i, const Definition &d, const Data &data,
                                   netplay::CourseHazardState &state) {
    auto &a = actors_[i];
    auto &p = state.poses[i];
    a.random = 0xA511E9B3u ^ (d.id * 0x9E3779B9u);
    if (d.kind == Kind::Bat && d.subtype != 1 && d.subtype != 2)
        throw std::runtime_error("unknown source bat group");
    if (d.kind == Kind::Smoke && (d.parent >= state.count || (d.subtype != 0 && d.subtype != 1 &&
                                                              d.subtype != 3 && d.subtype != 4)))
        throw std::runtime_error("unknown source smoke/emission parent");
    if (d.kind == Kind::Flame && d.subtype != 0 && d.subtype != 4 && d.subtype != 5 &&
        d.subtype != 9)
        throw std::runtime_error("unknown source flame state");
    if (d.kind == Kind::Boo) {
        if (d.path >= data.paths.size() || data.paths[d.path].points.size() < 4 ||
            data.paths[d.path].durations.size() != data.paths[d.path].points.size())
            throw std::runtime_error("Boo needs source spline controls and durations");
        for (auto value : data.paths[d.path].durations)
            if (!value || value > 10000)
                throw std::runtime_error("Boo spline duration");
        if (d.animation_frames < 58)
            throw std::runtime_error("Boo needs normal and mirrored views");
        p.opacity = 0;
    }
    if (d.kind == Kind::Flame) {
        p.active = d.subtype == 0 ? 2 : 0;
        p.tint = 0xffff1e;
        p.environment_tint = 0xff0000;
    }
}

void SceneSimulation::effect(unsigned i, const Definition &d, const Data &data,
                             std::span<const Racer> racers, netplay::CourseHazardState &state) {
    auto &a = actors_[i];
    auto &p = state.poses[i];
    switch (d.kind) {
    case Kind::Bat: {
        const bool first = d.subtype == 1;
        const bool nearby = near(racers, d.position, (first ? 1150.f : 700.f) * scale_);
        if (nearby)
            ++a.cycle;
        else
            a.cycle = 0;
        unsigned slots = 0;
        for (const auto &other : data.definitions)
            if (other.kind == Kind::Bat && other.subtype == d.subtype)
                ++slots;
        const unsigned period = std::max(1u, slots) * 4;
        const unsigned phase = (first ? (d.phase / 2) * 8 : d.phase * 4) % period;
        // The original bin emits two every8 updates, the second group one/4,
        // during a210-update burst followed by a360-update trigger cooldown.
        if (!p.active && nearby && a.cycle % 360 < 210 && a.cycle % period == phase) {
            p.position = d.position;
            p.position[0] -= random(a.random, 30) * scale_;
            p.position[1] -= random(a.random, 30) * scale_;
            p.position[2] += random(a.random, 25) * scale_;
            Vec target = d.target;
            target[1] +=
                (first ? float(random(a.random, 150)) : -float(random(a.random, 200))) * scale_;
            a.angle = yaw(sub(target, p.position));
            a.phase = first ? float(0xdc00) : 0;
            a.speed = (4 + random(a.random, 4)) * scale_;
            a.age = 0;
            p.active = 1;
            ++p.generation;
        } else if (p.active) {
            const auto pitch = source_turn(std::uint16_t(a.phase), first && a.age < 31 ? 0x800 : 0);
            a.phase = float(pitch);
            const float radians = angle(pitch);
            auto velocity = mul(forward(a.angle), a.speed * std::cos(radians));
            velocity[2] = -a.speed * std::sin(radians);
            p.position = add(p.position, velocity);
            p.rotation[1] = a.angle;
            if (p.position[0] <= d.target[0] - (first ? 40.f : 50.f) * scale_ || ++a.age > 600)
                p.active = 0;
        }
        const unsigned roll = state.clock % 16;
        p.rotation[2] =
            std::uint16_t(roll < 8 ? int(roll) * 1024 - 4096 : 12288 - int(roll) * 1024);
        a.animation = state.clock; // One shared original four-frame wing animation.
        break;
    }
    case Kind::Boo: {
        const Racer *target = nullptr;
        for (const auto &r : racers)
            if (r.slot == a.target_slot) {
                target = &r;
                break;
            }
        if (!p.active) {
            const auto *candidate = near(racers, d.position, 150 * scale_);
            if (!candidate) {
                a.triggered = false;
                break;
            }
            if (a.triggered)
                break;
            target = candidate;
            a.target_slot = target->slot;
            a.target_valid = false;
            a.triggered = true;
            a.stage = 1;
            a.age = 0;
            a.node = 0;
            a.path_clock = 0;
            a.animation = 0;
            p.active = 2;
            p.opacity = 0;
            ++p.generation;
            a.jitter = {(int(random(a.random, 60)) - 30) * scale_,
                        -(int(random(a.random, 80)) - 40) * scale_,
                        (int(random(a.random, 20)) - 10) * scale_};
        }
        if (!target || distance(target->position, d.target) < 100 * scale_ ||
            distance(target->position, d.position) > 1500 * scale_)
            a.stage = 3;
        if (a.stage == 1) {
            if (++a.age % 2 == 0)
                p.opacity = std::min(80u, p.opacity + 2);
            if (p.opacity == 80) {
                a.stage = 2;
                a.age = 0;
            }
        } else if (a.stage == 2) {
            const unsigned at = a.age++ % 80;
            p.opacity = at < 40 ? 80 + at : 160 - at;
        } else {
            p.opacity = p.opacity > 2 ? p.opacity - 2 : 0;
            if (!p.opacity) {
                p.active = 0;
                break;
            }
        }
        if (target) {
            if (target_relocated(a, *target))
                ++p.generation; // A racer recovery is not a swept ghost movement.
            const auto &path = data.paths[d.path];
            const auto [local, derivative] = spline(path, a.node, a.path_clock);
            const float t = float(a.path_clock) / 10000;
            const float duration = float(path.durations[a.node]) * (1 - t) +
                                   path.durations[(a.node + 1) % path.durations.size()] * t;
            a.path_clock += unsigned(10000.f / duration);
            if (a.path_clock >= 10000) {
                a.path_clock = 0;
                a.node = (a.node + 1) % path.points.size();
            }
            if (std::hypot(target->velocity[0], target->velocity[1]) > .0001f)
                a.angle = yaw(target->velocity);
            // Original relative spline rotated by0x8000-cameraYaw. Use the
            // canonical rider heading so every client sees the same haunting.
            Vec offset = rotate(add(local, a.jitter), std::uint16_t(a.angle - 0x8000));
            offset[2] += 6.5f * scale_;
            p.position = add(target->position, offset);
            p.rotation[1] = source_turn(p.rotation[1], yaw(derivative));
            const unsigned view = unsigned(p.rotation[1]) * 36 / 65536;
            p.model = std::uint16_t(d.model + (view <= 18 ? view : 29 + 36 - view));
        }
        break;
    }
    case Kind::Fish:
        if (!p.active) {
            if (near(racers, d.target, 150 * scale_)) {
                p.position = d.position;
                a.velocity = mul(forward(0x5800), 25 * scale_);
                a.velocity[2] = 18 * scale_;
                a.age = 0;
                p.active = 2;
                ++p.generation;
            }
        } else {
            a.velocity[2] -= .7f * scale_;
            p.position = add(p.position, a.velocity);
            // Original pitch uses absolute Z velocity, not horizontal length.
            p.rotation[0] = std::uint16_t(
                int(std::atan2(a.velocity[2], std::abs(a.velocity[1])) * 65536.f / tau));
            p.rotation[1] = 0x5800;
            if (++a.age >= 71)
                p.active = 0;
        }
        break;
    case Kind::Flame:
        a.animation = state.clock;
        if (d.subtype == 0) {
            p.active = 2;
            p.position = d.position;
            p.tint = 0xffff1e;
            p.opacity = 255;
            p.environment_tint = 0xff0000;
            break;
        }
        if (d.subtype == 4 || d.subtype == 5) {
            const bool big = d.subtype == 4;
            bool nearby = false;
            for (const auto &r : racers)
                if (r.human && distance(r.position, d.position) < (big ? 750.f : 300.f) * scale_)
                    nearby = true;
            // The source camera/track-section gate becomes canonical human
            // proximity. Keep its first three90-tick bursts then300-tick pause;
            // each emitter has its original20/10 staggered particle slots.
            if (nearby)
                ++a.cycle;
            else
                a.cycle = 0;
            const bool burst = a.cycle == 1 || a.cycle == 91 || a.cycle == 181 ||
                               (a.cycle >= 481 && (a.cycle - 181) % 300 == 0);
            if (!a.stage && burst) {
                a.stage = 1;
                a.timer = d.phase * 2;
                a.age = 0;
                a.phase = float(0x0c00);
                p.position = d.position;
                p.visual_scale = 1;
                p.opacity = 255;
                p.tint = 0xffff00;
                p.environment_tint = 0;
                p.active = 0;
            } else if (a.stage == 1) {
                if (a.timer) {
                    --a.timer;
                    break;
                }
                a.stage = 2;
                p.active = 2;
                ++p.generation;
            } else if (a.stage == 2) {
                const unsigned held = big ? 15 : 3;
                if (a.age >= held + 6) {
                    p.active = 0;
                    a.stage = 0;
                    break;
                }
                ++a.age;
                // Native sizeScaling is .5→1 for the small particles and1→4
                // for the large ones; visual_scale multiplies the authored start.
                p.visual_scale = std::min(big ? 4.f : 2.f, p.visual_scale + (big ? .1f : .2f));
                p.tint = 0xff0000 | (unsigned(std::max(0, 255 - int(a.age) * 24)) << 8);
                if (a.age > held)
                    p.opacity = unsigned(std::max(80, 255 - int(a.age - held) * 32));
            }
            if (a.stage == 2) {
                if (a.age >= (big ? 15u : 3u))
                    a.phase = std::max(0.f, a.phase - 1024.f);
                const float pitch = angle(std::uint16_t(a.phase));
                auto velocity =
                    mul(forward(d.rotation[1]), (big ? 8.f : 4.f) * scale_ * std::cos(pitch));
                velocity[2] = -(big ? 8.f : 4.f) * scale_ * std::sin(pitch);
                p.position = add(p.position, velocity);
            }
            break;
        }
        // Source subtype9 exists but has no live caller in the pinned game.
        // Only explicitly authored records use this upward fire-particle path.
        if (!p.active) {
            if (a.timer) {
                --a.timer;
                break;
            }
            p.position = d.position;
            p.active = 2;
            p.opacity = 255;
            p.visual_scale = 1;
            p.tint = 0xffff1e;
            a.velocity = {0, 0, 8 * scale_};
            a.age = 0;
            ++p.generation;
        } else {
            p.position = add(p.position, a.velocity);
            ++a.age;
            p.visual_scale = std::min(2.f, p.visual_scale + .05f);
            p.tint = 0xff0000 | (unsigned(std::max(0, 255 - int(a.age) * 24)) << 8);
            p.environment_tint = 0xff0000;
            if (a.age > 11)
                p.opacity = unsigned(std::max(80, 255 - int(a.age - 11) * 32));
            if (p.opacity == 80) {
                p.active = 0;
                a.timer = d.phase;
            }
        }
        break;
    case Kind::Smoke: {
        if (d.parent >= state.count) {
            p.active = 0;
            break;
        }
        const auto &parent = state.poses[d.parent];
        const auto &parent_def = data.definitions[d.parent];
        if (d.subtype == 3 || d.subtype == 4) {
            bool triggered = false;
            if (d.subtype == 3) {
                triggered = parent.active == 1 && parent.model == parent_def.model + 1 &&
                            a.seen_generation != parent.generation;
            } else
                triggered = parent.active == 0 && parent.generation &&
                            a.seen_generation != parent.generation;
            if (triggered) {
                a.seen_generation = parent.generation;
                p.active = 2;
                ++p.generation;
                a.age = 0;
                p.position = d.subtype == 3 ? parent_def.position : parent.position;
                if (d.subtype == 3)
                    p.position[2] -= 13 * scale_;
                const auto direction = std::uint16_t(d.phase * 0x2000);
                const float speed = (d.subtype == 3 ? .8f + random(a.random, 5) * .01f
                                                    : 4.5f + random(a.random, 10) * .1f) *
                                    scale_;
                a.velocity = mul(forward(direction), speed);
                a.velocity[2] = (d.subtype == 3 ? 4.8f + random(a.random, 10) * .1f
                                                : 2.6f + random(a.random, 20) * .5f) *
                                scale_;
                p.visual_scale = d.subtype == 3 ? 1.f : 1.f + random(a.random, 100) * .02f;
                a.angle = std::uint16_t(0x1000 + random(a.random, 0x4000));
                p.opacity = 255;
            } else if (p.active) {
                a.velocity[2] -= (d.subtype == 3 ? .3f : .74f) * scale_;
                p.position = add(p.position, a.velocity);
                if (d.subtype == 4)
                    p.rotation[2] += a.angle;
                if (++a.age >= (d.subtype == 3 ? 50u : 100u))
                    p.active = 0;
            }
            break;
        }
        unsigned slots = 0;
        for (const auto &other : data.definitions)
            if (other.kind == Kind::Smoke && other.subtype == d.subtype &&
                other.parent == d.parent && other.target == d.target)
                ++slots;
        const unsigned interval = d.subtype == 1 ? 10 : 5, period = std::max(1u, slots) * interval;
        if (!p.active) {
            if (parent.active && state.clock % period == (d.phase * interval) % period) {
                p.position = add(parent.position, rotate(d.target, parent.rotation[1]));
                a.origin = p.position;
                a.velocity = {0, 0, 1.1f * scale_};
                a.age = 0;
                p.active = 2;
                p.opacity = 255;
                p.visual_scale = 1;
                ++p.generation;
                const unsigned gray = d.subtype == 1 ? 255 : 30 + random(a.random, 100);
                p.tint = gray * 0x010101u;
                p.environment_tint = d.subtype == 1 ? 0x969696 : 0;
            }
        } else {
            a.velocity[2] -= .03f * scale_;
            p.position[2] = std::min(a.origin[2] + 100 * scale_, p.position[2] + a.velocity[2]);
            ++a.age;
            if (a.age == 1)
                p.visual_scale = 1.1f;
            else if (a.age % 2 == 1)
                p.visual_scale = std::min(2.f, p.visual_scale + .2f);
            p.opacity = unsigned(std::max(30, 255 - int(a.age > 0 ? a.age - 1 : 0) * 7));
            if (a.age >= 35)
                p.active = 0;
        }
        break;
    }
    default:
        break;
    }
}
} // namespace rr64::course_hazards
