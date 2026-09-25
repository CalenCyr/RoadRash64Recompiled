#include "rr64_course_hazard_motion.hpp"
#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>

namespace rr64::course_hazards {
namespace {
constexpr float hz = 30.f, tau = 6.2831853071795864769f;
Vec add(Vec a, const Vec &b) {
    for (unsigned i = 0; i < 3; ++i)
        a[i] += b[i];
    return a;
}
Vec sub(Vec a, const Vec &b) {
    for (unsigned i = 0; i < 3; ++i)
        a[i] -= b[i];
    return a;
}
Vec mul(Vec a, float s) {
    for (float &x : a)
        x *= s;
    return a;
}
float dot(const Vec &a, const Vec &b) { return a[0] * b[0] + a[1] * b[1] + a[2] * b[2]; }
float length(Vec a) { return std::sqrt(dot(a, a)); }
bool approach(float &x, float target, float step) {
    x = x < target ? std::min(x + step, target) : std::max(x - step, target);
    // Uniform course scaling must not add a whole source frame because many
    // decimal steps landed a few float ULPs short of the same authored endpoint.
    if (std::abs(x - target) <= std::abs(step) * .001f)
        x = target;
    return x == target;
}
std::uint16_t heading(Vec d) {
    return static_cast<std::uint16_t>(static_cast<int>(std::atan2(d[0], -d[1]) * 65536.f / tau));
}
std::uint16_t turn(std::uint16_t from, std::uint16_t to, int step) {
    const auto d = static_cast<std::int16_t>(to - from);
    return static_cast<std::uint16_t>(from + std::clamp(int(d), -step, step));
}
} // namespace

void Simulation::reset(const Data &data) {
    if (data.definitions.size() > netplay::kMaximumCourseHazards)
        throw std::runtime_error("hazard capacity");
    if (!std::isfinite(data.source_to_world_scale) || data.source_to_world_scale <= 0 ||
        data.source_to_world_scale > 1)
        throw std::runtime_error("hazard source scale");
    scale = data.source_to_world_scale;
    state_ = {};
    actors_ = {};
    random_ = 0x6D2B79F5u;
    state_.count = static_cast<std::uint16_t>(data.definitions.size());
    for (unsigned i = 0; i < state_.count; ++i) {
        const auto &d = data.definitions[i];
        auto &p = state_.poses[i];
        auto &a = actors_[i];
        p.position = d.position;
        p.rotation = d.rotation;
        p.model = static_cast<std::uint16_t>(d.model);
        p.active = 1;
        a.node = d.node;
        a.lane = d.lane;
        a.id = d.id;
        a.kind = d.kind;
        a.parent = d.parent;
        a.random = 0x9E3779B9u ^ (d.id * 0x85EBCA6Bu);
        if (d.kind == Kind::Thwomp) {
            p.rotation[1] = (d.subtype == 3 || d.subtype == 5) ? 0x4000 : 0xC000;
            if (d.subtype == 2)
                a.offset[2] = 20 * scale;
            if (d.subtype == 3)
                a.offset[2] = 15 * scale;
            if (d.subtype == 6)
                a.offset[2] = 10 * scale;
            if (d.subtype == 5)
                a.offset[2] = 70 * scale;
            a.timer = d.subtype == 4 ? (d.phase ? int(d.phase) * 60 : 2) : 60;
            p.position = add(d.position, a.offset);
        } else if (d.kind == Kind::Train || d.kind == Kind::Traffic) {
            const auto &path = data.paths.at(d.path);
            if (path.points.size() < 5 || d.node >= path.points.size())
                throw std::runtime_error("hazard path");
            p.position = path.points[d.node];
            // Source initialization follows the path once before its first draw.
            const auto before = p.position;
            vehicle(i, d, path);
            p.rotation[1] = heading(sub(p.position, before));
        } else {
            reset_sprite(i, d);
        }
    }
    scenery_.reset(data, state_);
}

void Simulation::thwomp(unsigned i, const Definition &d, std::span<const Racer> racers) {
    auto &a = actors_[i];
    auto &p = state_.poses[i];
    const float rest = (d.subtype == 2 ? 20.f : d.subtype == 3 ? 15.f : 30.f) * scale;
    if (d.subtype == 6) { // Source large, hovering/laughing statue; no invented slam.
        bool near = false;
        for (const auto &r : racers)
            if (length(sub(r.position, p.position)) < 100 * scale)
                near = true;
        p.model = static_cast<std::uint16_t>(near ? 3 + (state_.clock / 8) % 3 : 0);
        return;
    }
    if (d.subtype == 5) { // Original 70-high, 250-long shuttle; source velocities1/1.5.
        const float target = (a.leg & 1) ? 0.f : 250 * scale;
        if (approach(a.offset[1], target, (d.phase ? 1.5f : 1.f) * scale))
            ++a.leg;
        p.model = static_cast<std::uint16_t>(3 + (state_.clock / 8) % 3);
        p.position = add(d.position, a.offset);
        return;
    }
    // Source vertical states50..54: windup +15 at1.5, slam at2,
    // faces3/2 for7/51 updates, recovery at.5. The target always stays solid.
    switch (a.stage) {
    case 0: {
        bool trigger = d.subtype != 3;
        if (!trigger)
            for (unsigned target = 0; target < racers.size(); ++target)
                if (length(sub(racers[target].position, p.position)) < 300 * scale) {
                    trigger = true;
                    a.leg = target;
                    break;
                }
        if (trigger && --a.timer < 0) {
            a.stage = d.subtype == 3 ? 7 : 1;
            if (d.subtype == 3)
                a.timer = 160;
        }
        break;
    }
    case 1:
        if (approach(a.offset[2], rest + 15 * scale, 1.5f * scale)) {
            a.stage = 2;
            p.model = 1;
        }
        break;
    case 2:
        if (approach(a.offset[2], 0, 2 * scale)) {
            a.stage = 3;
            a.timer = 6;
            p.model = 3;
        }
        break;
    case 3:
        if (--a.timer < 0) {
            a.stage = 4;
            a.timer = 50;
            p.model = 2;
        }
        break;
    case 4:
        if (--a.timer < 0)
            a.stage = 5;
        break;
    case 5:
        if (approach(a.offset[2], rest, .5f * scale)) {
            a.stage = d.subtype == 2 ? 6 : d.subtype == 3 ? 8 : 0;
            a.timer = 60;
            p.model = 0;
        } else
            p.model = a.offset[2] >= 20 * scale ? 0 : 1;
        break;
    case 6: {
        // Moving subtype uses the original rectangle endpoints and step sizes.
        // Road Rash adaptation: source camera/animation waits become a60-frame
        // pause per slam; collision cannot depend on which viewport sees it.
        const float sign = d.phase ? -1.f : 1.f;
        const std::array<Vec, 4> corners{{{200 * scale * sign, 0, rest},
                                          {200 * scale * sign, 100 * scale * sign, rest},
                                          {0, 100 * scale * sign, rest},
                                          {0, 0, rest}}};
        const auto &target = corners[a.leg % 4];
        const auto before = a.offset;
        const bool x = approach(a.offset[0], target[0], 4 * scale);
        const bool y = approach(a.offset[1], target[1], 2 * scale);
        if (before != a.offset)
            p.rotation[1] = turn(p.rotation[1], heading(sub(a.offset, before)), 0x400);
        if (x && y) {
            ++a.leg;
            a.stage = 0;
            a.timer = 0;
        }
        break;
    }
    case 7: {
        // Source subtype3 pursues along the corridor at1.25× its target speed,
        // then slams and returns. Use canonical nearby racers instead of MK's
        // camera/path-index trigger; all screens share the same host decision.
        const float speed = a.leg < racers.size() ? length(racers[a.leg].velocity) / hz : 0;
        a.offset[0] += std::min(speed * 1.25f, 200 * scale);
        if (a.timer < 110 - int(d.phase) * 25) {
            if (a.velocity[1] == 0)
                a.velocity[1] = 1.5f * scale;
            a.offset[1] += a.velocity[1];
            if (std::abs(a.offset[1]) >= 40 * scale)
                a.velocity[1] = -a.velocity[1];
        }
        if (a.timer <= 100) {
            p.rotation[1] = turn(p.rotation[1], 0xC000, 0x400);
            p.model = 1;
        }
        if (--a.timer < 0 || a.offset[0] >= 1000 * scale) {
            a.offset[0] = std::min(a.offset[0], 1000 * scale);
            a.stage = 1;
        }
        break;
    }
    case 8: {
        const bool x = approach(a.offset[0], 0, 5 * scale), y = approach(a.offset[1], 0, 2 * scale);
        if (x && y) {
            a.stage = 9;
            a.velocity = {};
        }
        break;
    }
    case 9:
        p.rotation[1] = turn(p.rotation[1], 0x4000, 0x400);
        if (p.rotation[1] == 0x4000) {
            a.stage = 0;
            a.timer = 60;
        }
        break;
    default:
        a.stage = 0;
        break;
    }
    p.position = add(d.position, a.offset);
}

void Simulation::rock(unsigned i, const Definition &d, const course_walls::World *surfaces) {
    auto &a = actors_[i];
    auto &p = state_.poses[i];
    if (a.timer > 0) {
        --a.timer;
        return;
    }
    if (p.position[2] < d.minimum_height) {
        p.position = d.position;
        a.velocity = {};
        p.rotation = {};
        ++p.generation;
        a.timer = 60 * int(d.phase + 1);
        return;
    }
    // Source update_actor_falling_rocks: acceleration -.1 and terminal -2 in
    // source units/update, original10-unit collision sphere, 1.2 vertical bounce.
    a.velocity[2] = std::max(a.velocity[2] - .1f * scale, -2 * scale);
    p.rotation[0] += static_cast<std::int16_t>((-a.velocity[1] / scale) * 5461.f / 20.f);
    p.rotation[2] += static_cast<std::int16_t>((a.velocity[0] / scale) * 5461.f / 20.f);
    Vec remaining = a.velocity;
    for (unsigned pass = 0; pass < 3; ++pass) {
        const auto hit =
            surfaces ? course_walls::sweep_sphere(*surfaces, {p.position, 10 * scale}, remaining)
                     : course_walls::SweepHit{};
        if (!hit.hit) {
            p.position = add(p.position, remaining);
            break;
        }
        p.position = add(add(p.position, mul(remaining, hit.fraction)),
                         mul(hit.normal, hit.penetration + .0005f));
        const float closing = dot(a.velocity, hit.normal), old_vertical = a.velocity[2];
        if (closing < 0)
            a.velocity = sub(a.velocity, mul(hit.normal, 2 * closing));
        if (hit.normal[2] > .01f)
            a.velocity[2] = -1.2f * old_vertical;
        remaining = mul(a.velocity, 1 - hit.fraction);
    }
}

void Simulation::vehicle(unsigned i, const Definition &d, const Path &path) {
    auto &a = actors_[i];
    auto &p = state_.poses[i];
    const auto count = unsigned(path.points.size());
    float closest = std::numeric_limits<float>::infinity();
    unsigned node = a.node;
    // Original train nearest-point window [-2,+6]. Toad's path also advances
    // locally; including +/-10 avoids branch jumps at a crossing or stacked road.
    const int behind = d.kind == Kind::Train ? 2 : 10, ahead = d.kind == Kind::Train ? 7 : 11;
    for (int j = -behind; j < ahead; ++j) {
        const unsigned k = unsigned(((int(a.node) + j) % int(count) + int(count)) % int(count));
        Vec delta = sub(path.points[k], p.position);
        if (d.kind == Kind::Train)
            delta[2] = 0;
        const float distance = dot(delta, delta);
        if (distance < closest) {
            closest = distance;
            node = k;
        }
    }
    a.node = node;
    auto point = [&](unsigned n) {
        Vec target = path.points[n];
        if (d.kind == Kind::Traffic && path.left.size() == count && path.right.size() == count) {
            const float weight = .5f - .5f * a.lane;
            target[0] = path.left[n][0] * weight + path.right[n][0] * (1 - weight);
            target[1] = path.left[n][1] * weight + path.right[n][1] * (1 - weight);
        }
        return target;
    };
    if (d.kind == Kind::Traffic) {
        const float lane = node < 0x28A ? (d.subtype == 0   ? -.7f
                                           : d.subtype == 2 ? .7f
                                                            : 0.f)
                                        : (d.subtype == 2 ? .5f : -.5f);
        approach(a.lane, lane, .06f);
    }
    Vec target = mul(add(point((node + 3) % count), point((node + 4) % count)), .5f);
    if (d.kind == Kind::Train)
        target[2] = p.position[2];
    const Vec direction = sub(target, p.position);
    const float distance = length(direction);
    if (distance > .0005f) {
        p.position = add(p.position, mul(direction, d.speed / distance));
        const auto yaw = heading(direction);
        p.rotation[1] = d.kind == Kind::Train ? yaw : turn(p.rotation[1], yaw, 100);
    }
}

void Simulation::advance(const Data &data, std::span<const Racer> racers,
                         const course_walls::World *surfaces) {
    if (state_.count != data.definitions.size() || scale != data.source_to_world_scale)
        reset(data);
    ++state_.clock;
    spawn_moles(data, racers);
    for (unsigned i = 0; i < state_.count; ++i) {
        const auto &d = data.definitions[i];
        auto &p = state_.poses[i];
        const auto old = p.position;
        const auto generation = p.generation;
        switch (d.kind) {
        case Kind::Thwomp:
            thwomp(i, d, racers);
            break;
        case Kind::Rock:
            rock(i, d, surfaces);
            break;
        case Kind::Train:
        case Kind::Traffic:
            vehicle(i, d, data.paths.at(d.path));
            break;
        case Kind::Mole:
            mole(i, d);
            break;
        case Kind::Crab:
        case Kind::Hedgehog:
            patrol(i, d, surfaces);
            break;
        case Kind::Plant:
            plant(i, d, racers);
            break;
        case Kind::Snowman:
            snowman(i, d);
            break;
        default:
            break;
        }
        p.velocity = p.generation == generation ? mul(sub(p.position, old), hz) : Vec{};
    }
    scenery_.advance(data, racers, state_);
}
} // namespace rr64::course_hazards
