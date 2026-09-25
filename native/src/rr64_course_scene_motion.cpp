#include "rr64_course_scene_motion.hpp"
#include "rr64_course_hazard_motion.hpp"
#include <algorithm>
#include <cmath>
#include <limits>
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
Vec mul(Vec a, float s) {
    for (auto &v : a)
        v *= s;
    return a;
}
float squared(Vec a) { return a[0] * a[0] + a[1] * a[1] + a[2] * a[2]; }
float length(Vec a) { return std::sqrt(squared(a)); }
float approach(float from, float to, float step) {
    return from < to ? std::min(to, from + step) : std::max(to, from - step);
}
std::uint16_t heading(Vec d) {
    return static_cast<std::uint16_t>(int(std::atan2(d[0], -d[1]) * 65536.f / tau));
}
std::uint16_t turn(std::uint16_t a, std::uint16_t b, int amount) {
    return static_cast<std::uint16_t>(
        a + std::clamp(int(static_cast<std::int16_t>(b - a)), -amount, amount));
}
// Original func_800417B4. The high-byte equality test is intentional: it
// converges to the authored direction without a fixed-step oscillation.
std::uint16_t object_turn(std::uint16_t a, std::uint16_t b) {
    if ((a >> 8) == (b >> 8))
        return b;
    const std::uint16_t delta = b - a;
    const int step = delta < 0x400    ? 0x80
                     : delta < 0x800  ? 0x200
                     : delta < 0x4000 ? 0x400
                     : delta < 0x8000 ? 0x700
                     : delta < 0xc000 ? -0x700
                     : delta < 0xf800 ? -0x400
                     : delta < 0xfc00 ? -0x200
                                      : -0x80;
    return static_cast<std::uint16_t>(a + step);
}
Vec forward(std::uint16_t angle) {
    const float a = float(angle & 0xfff0u) * tau / 65536.f;
    return {std::sin(a), -std::cos(a), 0};
}
unsigned closest(const Path &path, Vec p, unsigned node, int behind, int ahead, float maximum,
                 bool horizontal = false) {
    unsigned best = node;
    float distance = maximum;
    const int n = int(path.points.size());
    for (int j = -behind; j <= ahead; ++j) {
        const unsigned at = unsigned((int(node) + j + n) % n);
        auto delta = sub(path.points[at], p);
        if (horizontal)
            delta[2] = 0;
        const float d = squared(delta);
        if (d < distance) {
            distance = d;
            best = at;
        }
    }
    return best;
}
Vec lane_point(const Path &path, unsigned node, float factor) {
    const unsigned next = (node + 1) % path.points.size();
    const float left = .5f - factor * .5f, right = 1 - left;
    Vec result = add(mul(add(path.left[node], path.left[next]), left * .5f),
                     mul(add(path.right[node], path.right[next]), right * .5f));
    result[2] = path.points[node][2];
    return result;
}
void ferry_step(const Path &path, Vec &position, unsigned &node, float speed, float scale) {
    node = closest(path, position, node, 2, 6, 250000.f * scale * scale, true);
    auto target = mul(add(path.points[(node + 3) % path.points.size()],
                          path.points[(node + 4) % path.points.size()]),
                      .5f);
    target[2] = position[2];
    const auto delta = sub(target, position);
    const float distance = length(delta);
    if (distance > .01f * scale)
        position = add(position, mul(delta, speed / distance));
}
unsigned schedule_index(const Definition &d, unsigned clock) {
    const unsigned tick = clock / d.frame_ticks, count = unsigned(d.frame_sequence.size());
    return tick < count ? tick
                        : d.animation_loop_start +
                              (tick - d.animation_loop_start) % (count - d.animation_loop_start);
}
unsigned frame(const Definition &d, unsigned clock, unsigned clip = 0) {
    if (!d.frame_sequence.empty())
        return d.model + d.frame_sequence[schedule_index(d, clock)];
    if (clip < d.clip_count.size() && d.clip_count[clip])
        return d.model + d.clip_start[clip] + (clock / d.frame_ticks) % d.clip_count[clip];
    return d.model + (clock / d.frame_ticks) % d.animation_frames;
}
Vec rotate_offset(Vec v, std::uint16_t angle) {
    const auto f = forward(angle);
    return {v[0] * -f[1] - v[1] * f[0], v[0] * f[0] - v[1] * f[1], v[2]};
}
} // namespace

void SceneSimulation::reset(const Data &data, netplay::CourseHazardState &state) {
    actors_ = {};
    scale_ = data.source_to_world_scale;
    for (unsigned i = 0; i < data.definitions.size(); ++i) {
        const auto &d = data.definitions[i];
        auto &a = actors_[i];
        auto &p = state.poses[i];
        a.kind = d.kind;
        a.origin = d.position;
        a.node = d.node;
        a.speed = d.speed;
        a.timer = d.phase;
        a.angle = d.rotation[1];
        if (d.kind < Kind::Egg)
            continue;
        if ((d.kind == Kind::Ferry || d.kind == Kind::Chomp || d.kind == Kind::Seagull ||
             (d.kind == Kind::Penguin && d.subtype == 0) || d.kind == Kind::Kiwano) &&
            (d.path >= data.paths.size() || data.paths[d.path].points.size() < 5 ||
             d.node >= data.paths[d.path].points.size()))
            throw std::runtime_error("scene actor path");
        p.active = d.solid ? 1 : 2;
        if (d.kind == Kind::Bat || d.kind == Kind::Boo || d.kind == Kind::Fish ||
            d.kind == Kind::Kiwano || d.kind == Kind::Smoke)
            p.active = 0;
        if (d.kind == Kind::Balloon) {
            a.velocity[2] = -2 * scale_;
            p.position[2] += 300 * scale_;
        }
        if (d.kind == Kind::Chomp &&
            (data.paths[d.path].left.size() != data.paths[d.path].points.size() ||
             data.paths[d.path].right.size() != data.paths[d.path].points.size()))
            throw std::runtime_error("chomp source lanes");
        if (d.kind == Kind::Seagull || (d.kind == Kind::Penguin && d.subtype == 0)) {
            const auto &path = data.paths[d.path];
            if (path.left.size() != path.points.size())
                throw std::runtime_error("actor spline tangents");
        }
        if (d.kind == Kind::Penguin && (d.subtype > 14 || !d.clip_count[0] ||
                                        (d.subtype > 8 && (!d.clip_count[1] || !d.clip_count[2]))))
            throw std::runtime_error("penguin source clips");
        // Circular phase and visual direction are separate original fields.
        if (d.kind == Kind::Penguin && d.subtype > 0 && d.subtype <= 8)
            p.rotation[1] = 0;
        if (d.kind == Kind::Penguin && d.subtype > 8) {
            p.rotation[1] = a.angle + 0x8000;
            a.path_clock = 2;
        }
        if (d.kind == Kind::Ferry) {
            // spawn_course_vehicles makes one path update before publishing the actor.
            const auto before = p.position;
            ferry_step(data.paths[d.path], p.position, a.node, a.speed, scale_);
            p.rotation[1] = heading(sub(p.position, before));
        }
        reset_effect(i, d, data, state);
    }
}

void SceneSimulation::hit(unsigned id, Vec, netplay::CourseHazardState &state) noexcept {
    if (id >= state.count)
        return;
    // MK's star-only destruction of eggs/plants is deliberately not invented
    // for ordinary Road Rash impacts. The shared native impulse handles riders.
    if (actors_[id].kind == Kind::Kiwano && state.poses[id].active == 1) {
        actors_[id].stage = 2;
        actors_[id].timer = 30;
        actors_[id].velocity = {0, 0, 2.3f * scale_};
        state.poses[id].active = 2;
    }
}

bool SceneSimulation::target_relocated(Actor &a, const Racer &target) noexcept {
    const float speed = std::hypot(target.velocity[0], target.velocity[1], target.velocity[2]);
    // Native recovery is an explicit discontinuity. The same displacement
    // allowance as item collection also catches an unmarked correction: two
    // source-frame travel distances plus .25 world units of contact tolerance.
    const float allowance = 2 * (std::max(speed, a.target_speed) / 30.f + .25f);
    const bool moved = a.target_valid &&
        (target.recovery_count != a.target_recovery ||
         squared(sub(target.position, a.target_previous)) > allowance * allowance);
    a.target_previous = target.position;
    a.target_speed = speed;
    a.target_recovery = target.recovery_count;
    a.target_valid = true;
    return moved;
}

void SceneSimulation::advance(const Data &data, std::span<const Racer> racers,
                              netplay::CourseHazardState &state) {
    for (unsigned i = 0; i < state.count; ++i) {
        const auto &d = data.definitions[i];
        if (d.kind < Kind::Egg)
            continue;
        auto &a = actors_[i];
        auto &p = state.poses[i];
        const auto before = p.position;
        const auto generation = p.generation;
        unsigned clip = 0;
        bool model_assigned = false;
        switch (d.kind) {
        case Kind::Egg:
            // Actor update runs twice in the source 30Hz race frame.
            a.angle = static_cast<std::uint16_t>(a.angle + 2 * 0x5b);
            p.position = add(d.target, mul(forward(a.angle), 70 * scale_));
            p.rotation[1] = static_cast<std::uint16_t>(p.rotation[1] - 2 * 546);
            break;
        case Kind::Crossing: {
            bool triggered = false;
            const float crossing = d.subtype == 0 ? .42299348f : .72017354f;
            for (unsigned train = 0; train < state.count; ++train) {
                const auto &td = data.definitions[train];
                if (td.kind != Kind::Train || td.subtype != 6 || td.path >= data.paths.size())
                    continue;
                const auto &path = data.paths[td.path];
                unsigned node = 0;
                float distance = std::numeric_limits<float>::infinity();
                for (unsigned k = 0; k < path.points.size(); ++k) {
                    const float v = squared(sub(path.points[k], state.poses[train].position));
                    if (v < distance) {
                        distance = v;
                        node = k;
                    }
                }
                const float t = float(node) / path.points.size();
                if (t > crossing - .1f && t < crossing + .08f)
                    triggered = true;
            }
            if (triggered) {
                a.timer = (a.timer + 2) % 40;
                p.model = std::uint16_t(d.model + (a.timer < 20 ? 1 : 2));
            } else {
                a.timer = 0;
                p.model = std::uint16_t(d.model);
            }
            break;
        }
        case Kind::Ferry: {
            const auto &path = data.paths[d.path];
            ferry_step(path, p.position, a.node, a.speed, scale_);
            const auto yaw = heading(sub(path.points[(a.node + 5) % path.points.size()], before));
            const int difference = std::abs(int(static_cast<std::int16_t>(yaw - p.rotation[1])));
            if (difference >= 6001) {
                if (a.speed > .2f * scale_)
                    a.speed -= .04f * scale_;
            } else if (a.speed < 2.f * scale_)
                a.speed += .02f * scale_;
            p.rotation[1] = turn(p.rotation[1], yaw, difference >= 6001 ? 60 : 30);
            break;
        }
        case Kind::Chomp: {
            // func_80074344, then reverse-follow func_8000D940. Oscillation
            // is a fraction of authored left/right lanes, not a world offset.
            if (a.stage == 0) {
                a.phase = -.8f;
                a.stage = 1;
            } else if (a.stage == 1) {
                a.phase += .03f;
                if (a.phase >= .8f) {
                    a.phase = .8f;
                    a.stage = 2;
                }
            } else {
                a.phase -= .03f;
                if (a.phase <= -.8f) {
                    a.phase = -.8f;
                    a.stage = 1;
                }
            }
            const auto &path = data.paths[d.path];
            const unsigned n = unsigned(path.points.size());
            auto point = p.position;
            point[2] += 15 * scale_;
            a.node = closest(path, point, a.node, 3, 6, 160000.f * scale_ * scale_);
            if (a.node == 0 && point[1] < path.points[0][1])
                a.node = n - 1;
            else if (a.node == n - 1 && point[1] >= path.points[0][1])
                a.node = 0;
            const auto target = mul(add(lane_point(path, (a.node + n - 3) % n, a.phase),
                                        lane_point(path, (a.node + n - 4) % n, a.phase)),
                                    .5f);
            const auto delta = sub(target, point);
            const float distance = length(delta);
            if (distance > .01f * scale_)
                point = add(point, mul(delta, a.speed / distance));
            point[2] -= 15 * scale_;
            p.position = point;
            p.rotation[1] = heading(sub(point, before));
            break;
        }
        case Kind::Seagull: {
            // Offline samples retain the source cubic spline's integer timer.
            // left carries a direction vector, not an atlas-space endpoint.
            const auto &path = data.paths[d.path];
            const unsigned sample = a.path_clock++ % path.points.size();
            p.position = path.points[sample];
            p.rotation[1] = object_turn(p.rotation[1], heading(path.left[sample]));
            break;
        }
        case Kind::Penguin: {
            if (d.subtype == 0) {
                const auto &path = data.paths[d.path];
                const unsigned sample = a.path_clock++ % path.points.size();
                p.position = path.points[sample];
                p.rotation[1] = object_turn(p.rotation[1], heading(path.left[sample]));
            } else if (d.subtype <= 8) {
                // Source 80088038 circular swimmers; paired actors start half a turn apart.
                const int step = (d.subtype == 5 || d.subtype == 6)   ? -256
                                 : (d.subtype == 3 || d.subtype == 4) ? 256
                                                                      : 336;
                a.angle = static_cast<std::uint16_t>(a.angle + step);
                const float radius = (d.subtype <= 2 ? 100.f : 80.f) * scale_;
                p.position = add(d.position, mul(forward(a.angle), radius));
                p.rotation[1] = object_turn(p.rotation[1], heading(sub(p.position, before)));
            } else {
                // Source object animation is updated before motion. One-shot
                // clips hold their last frame until the next update clears bit2.
                const unsigned active_clip = a.cycle;
                if (a.age == 0)
                    a.age = 1;
                else if (a.age == 1) {
                    a.animation += active_clip ? 1 : a.path_clock;
                    const unsigned end = d.clip_count[active_clip] - 1;
                    if (a.animation > end) {
                        a.animation = active_clip ? end : 0;
                        if (active_clip)
                            a.age = 2;
                    }
                } else if (a.age == 2)
                    a.age = 3;
                if (a.stage == 0) {
                    p.rotation[1] = object_turn(p.rotation[1], a.angle);
                    if (p.rotation[1] == a.angle) {
                        a.stage = 1;
                        a.timer = 15;
                        a.speed = .4f * scale_;
                        a.path_clock = 4;
                    }
                } else if (a.stage == 1) {
                    a.speed = approach(a.speed, .8f * scale_, .02f * scale_);
                    if (a.timer-- == 0) {
                        a.stage = 2;
                        a.cycle = 1;
                        a.animation = 0;
                        a.age = 0;
                    }
                } else if (a.stage == 2) {
                    clip = 1;
                    a.speed = approach(a.speed,
                                       (d.subtype == 9    ? 1.f
                                        : d.subtype == 10 ? 1.5f
                                                          : 2.5f) *
                                           scale_,
                                       .15f * scale_);
                    if (a.age == 3 && a.speed == (d.subtype == 9    ? 1.f
                                                  : d.subtype == 10 ? 1.5f
                                                                    : 2.5f) *
                                                     scale_) {
                        a.stage = 3;
                        a.timer = 30;
                    }
                } else if (a.stage == 3) {
                    clip = 1;
                    if (a.timer-- == 0) {
                        a.stage = 4;
                        a.timer = 10;
                    }
                } else if (a.stage == 4) {
                    a.speed = approach(a.speed, .4f * scale_, .2f * scale_);
                    if (a.timer-- == 0) {
                        a.stage = 5;
                        a.cycle = 2;
                        a.animation = 0;
                        a.age = 0;
                    }
                } else if (a.stage == 5) {
                    clip = 2;
                    if (a.age == 3) {
                        a.angle += 0x8000;
                        a.stage = 0;
                        a.cycle = 0;
                        a.animation = 0;
                        a.age = 0;
                        a.path_clock = 1;
                    }
                }
                p.position = add(p.position, mul(forward(p.rotation[1]), a.speed));
            }
            if (d.subtype <= 8) {
                if (a.age == 0)
                    a.age = 1;
                else {
                    a.animation += d.subtype ? 2 : 1;
                    if (a.animation >= d.clip_count[0])
                        a.animation = 0;
                }
            }
            p.model = std::uint16_t(d.model + d.clip_start[a.cycle] + a.animation);
            model_assigned = true;
            break;
        }
        case Kind::Kiwano: {
            const Racer *target = nullptr;
            for (const auto &r : racers)
                if (r.human && r.slot == d.subtype) {
                    target = &r;
                    break;
                }
            if (!target || !target->source_grass) {
                p.active = 0;
                a.stage = 0;
                a.target_valid = false;
                break;
            }
            if (target_relocated(a, *target) && a.stage == 1)
                a.stage = 0; // Restart at the recovered target with a new generation.
            for (unsigned step = 0; step < 2; ++step) {
                if (a.stage == 0) {
                    a.stage = 1;
                    a.speed = 80 * scale_;
                    ++p.generation;
                }
                if (a.stage == 1) {
                    const auto &path = data.paths[d.path];
                    Vec anchor = path.points[0];
                    float best = std::numeric_limits<float>::infinity();
                    for (const auto &point : path.points) {
                        const float v = squared(sub(point, target->position));
                        if (v < best) {
                            best = v;
                            anchor = point;
                        }
                    }
                    const Vec dir = sub(target->position, anchor);
                    const float n = length(dir);
                    if (n < .0001f) {
                        p.active = 0;
                        break;
                    }
                    p.position = add(target->position, mul(dir, a.speed / n));
                    p.active = 1;
                    a.speed -= 2 * scale_;
                    if (a.speed <= 0) {
                        a.stage = 2;
                        a.timer = 30;
                        a.velocity = {0, 0, 2.3f * scale_};
                    }
                } else {
                    p.active = 2;
                    a.velocity[2] -= .3f * scale_;
                    p.position = add(p.position, a.velocity);
                    if (a.timer-- == 0) {
                        a.stage = 0;
                        p.active = 0;
                    }
                }
            }
            break;
        }
        case Kind::Balloon: {
            // Luigi's source vertical controller, once per object frame.
            if (a.stage == 0) {
                if (p.position[2] - d.position[2] <= 18 * scale_)
                    a.stage = 1;
            } else if (a.stage == 1) {
                a.velocity[2] = approach(a.velocity[2], 0, .05f * scale_);
                if (a.velocity[2] == 0) {
                    a.stage = 2;
                    a.timer = 1;
                }
            } else if (a.stage == 2) {
                if (a.timer-- == 0)
                    a.stage = 3;
            } else if (a.stage == 3) {
                a.velocity[2] = approach(a.velocity[2], scale_, .05f * scale_);
                if (a.velocity[2] == scale_) {
                    a.stage = 4;
                    a.timer = 90;
                }
            } else if (a.stage == 4) {
                if (a.timer-- == 0)
                    a.stage = 5;
            } else if (a.stage == 5) {
                a.velocity[2] = approach(a.velocity[2], 0, .05f * scale_);
                if (a.velocity[2] == 0)
                    a.stage = 6;
            } else if (a.stage == 6) {
                a.velocity[2] = approach(a.velocity[2], -scale_, .05f * scale_);
                if (a.velocity[2] == -scale_) {
                    a.stage = 7;
                    a.timer = 90;
                }
            } else if (a.stage == 7) {
                if (a.timer-- == 0) {
                    a.stage = 8;
                    a.timer = 90;
                }
            } else if (a.stage == 8) {
                a.velocity[2] = approach(a.velocity[2], 0, .05f * scale_);
                if (a.timer-- == 0)
                    a.stage = 3;
            }
            p.position[2] += a.velocity[2];
            p.rotation[1] += 0x100;
            break;
        }
        case Kind::Wheel:
            if (d.parent < state.count) {
                const auto &parent = state.poses[d.parent];
                p.position = add(parent.position, rotate_offset(d.target, parent.rotation[1]));
                p.rotation = parent.rotation;
                // Two 60Hz actor updates per shared source frame. Original
                // engine/passenger wheels -9deg, tender -7deg, paddle +5deg.
                const int step = d.subtype == 1 ? 1820 : d.subtype == 2 ? -2548 : -3276;
                p.rotation[0] = static_cast<std::uint16_t>(state.clock * step + d.rotation[0]);
            }
            break;
        case Kind::Sign:
            if (d.subtype == 0)
                p.rotation[1] += 364;
            break;
        case Kind::Neon:
            if (!d.visible_sequence.empty())
                p.active = d.visible_sequence[schedule_index(d, a.animation)] ? 2 : 0;
            break; // Exact authored palette and visibility schedule, including prelude.
        case Kind::Bat:
        case Kind::Fish:
        case Kind::Boo:
        case Kind::Flame:
        case Kind::Smoke:
            effect(i, d, data, racers, state);
            break;
        default:
            break;
        }
        if (!model_assigned && d.kind != Kind::Crossing && d.kind != Kind::Boo)
            p.model = std::uint16_t(frame(d, a.animation++, clip));
        p.velocity = p.generation == generation ? mul(sub(p.position, before), 30.f) : Vec{};
    }
}
} // namespace rr64::course_hazards
