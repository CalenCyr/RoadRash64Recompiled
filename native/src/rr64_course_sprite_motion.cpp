#include "rr64_course_hazard_motion.hpp"
#include <algorithm>
#include <cmath>

namespace rr64::course_hazards {
namespace {
constexpr float hz = 30.f;
std::uint32_t random_next(std::uint32_t &value) noexcept {
    value ^= value << 13;
    value ^= value >> 17;
    value ^= value << 5;
    return value;
}
Vec difference(Vec a, const Vec &b) noexcept {
    for (unsigned k = 0; k < 3; ++k)
        a[k] -= b[k];
    return a;
}
float horizontal_length(const Vec &v) noexcept { return std::hypot(v[0], v[1]); }
bool approach(float &value, float target, float step) noexcept {
    value = value < target ? std::min(value + step, target) : std::max(value - step, target);
    if (std::abs(value - target) <= step * .001f)
        value = target;
    return value == target;
}
void frame(netplay::CourseHazardPose &p, const Definition &d, unsigned value) noexcept {
    p.model =
        static_cast<std::uint16_t>(d.model + std::min(value, std::max(1u, d.animation_frames) - 1));
}
bool move_towards(Vec &position, const Vec &target, float step) noexcept {
    const Vec delta = difference(target, position);
    const float distance = horizontal_length(delta);
    if (distance <= step * 1.001f) {
        position[0] = target[0];
        position[1] = target[1];
        return true;
    }
    for (unsigned k = 0; k < 2; ++k)
        position[k] += delta[k] * (step / distance);
    return false;
}
void grounded(netplay::CourseHazardPose &p, const course_walls::World *surfaces, float source_scale,
              float above_ground) noexcept {
    if (!surfaces)
        return;
    Vec start = p.position;
    start[2] += 20 * source_scale;
    const float radius = .01f * source_scale;
    const auto contact =
        course_walls::sweep_sphere(*surfaces, {start, radius}, {0, 0, -60 * source_scale});
    if (contact.hit && contact.normal[2] > .25f)
        p.position[2] = contact.point[2] + above_ground * source_scale;
}
} // namespace

void Simulation::reset_sprite(unsigned i, const Definition &d) {
    auto &a = actors_[i];
    auto &p = state_.poses[i];
    switch (d.kind) {
    case Kind::Mole:
        p.active = 0;
        break;
    case Kind::Snowman:
        if (d.subtype == 1)
            p.active = 2; // Only the original body supplies the collision test.
        break;
    case Kind::Crab:
    case Kind::Hedgehog: {
        const auto delta = difference(d.target, d.position);
        const float distance = horizontal_length(delta);
        if (distance > 0)
            a.velocity = {delta[0] / distance, delta[1] / distance, 0};
        break;
    }
    default:
        break;
    }
}

void Simulation::spawn_moles(const Data &data, std::span<const Racer> racers) {
    // The original 100cc pools are 5/8/8 across 8/11/12 holes. Camera-section
    // gates become nearby canonical racers; a spectator cannot spawn a hazard.
    constexpr std::array<unsigned, 3> limits{5, 8, 8};
    for (unsigned group = 1; group <= limits.size(); ++group) {
        unsigned occupied = 0, candidates = 0;
        std::array<unsigned, netplay::kMaximumCourseHazards> available{};
        for (unsigned i = 0; i < state_.count; ++i) {
            const auto &d = data.definitions[i];
            if (d.kind != Kind::Mole || d.subtype != group)
                continue;
            if (actors_[i].stage) {
                ++occupied;
                continue;
            }
            bool near = false;
            for (const auto &r : racers)
                if (horizontal_length(difference(r.position, d.position)) < 500 * scale) {
                    near = true;
                    break;
                }
            if (near)
                available[candidates++] = i;
        }
        while (occupied < limits[group - 1] && candidates) {
            const unsigned choice = random_next(random_) % candidates;
            const unsigned i = available[choice];
            available[choice] = available[--candidates];
            auto &a = actors_[i];
            auto &p = state_.poses[i];
            a.stage = 1;
            a.timer = int(random_next(random_) % 30) + 5;
            a.offset = a.velocity = {};
            a.animation = 0;
            p.position = data.definitions[i].position;
            p.rotation = data.definitions[i].rotation;
            p.active = 2;
            ++p.generation;
            frame(p, data.definitions[i], 0);
            ++occupied;
        }
    }
}

void Simulation::mole(unsigned i, const Definition &d) {
    auto &a = actors_[i];
    auto &p = state_.poses[i];
    auto finish = [&] {
        a.stage = 0;
        p.active = 0;
        ++p.generation;
    };
    switch (a.stage) {
    case 0:
        return;
    case 1:
        if (--a.timer < 0) {
            a.stage = 2;
            p.active = 1;
        }
        break;
    case 2:
        if (approach(a.offset[2], 9 * scale, .7f * scale)) {
            a.stage = 3;
            a.timer = 10;
        }
        break;
    case 3:
        if (--a.timer < 0)
            a.stage = 4;
        break;
    case 4:
        if (approach(a.offset[2], 3 * scale, scale)) {
            a.stage = 5;
            a.velocity[2] = 3.6f * scale;
            a.animation = 0;
        }
        break;
    case 5:
        a.velocity[2] -= .25f * scale;
        a.offset[2] += a.velocity[2];
        frame(p, d, 1 + std::min(5u, a.animation++ / 2));
        if (a.offset[2] <= 0) {
            a.offset[2] = 0;
            finish();
        }
        break;
    case 10:
        a.velocity[2] -= .184f * scale;
        for (unsigned k = 0; k < 3; ++k)
            a.offset[k] += a.velocity[k];
        p.rotation[2] += 0x1000;
        frame(p, d, 1 + a.animation++ % 6);
        // Source destroys the thrown mole at worldY=-10. The imported hole
        // supplies the local baseline, preserving the ten-unit ground offset.
        if (a.offset[2] <= -scale || ++a.timer > 300)
            finish();
        break;
    default:
        finish();
        break;
    }
    for (unsigned k = 0; k < 3; ++k)
        p.position[k] = d.position[k] + a.offset[k];
}

void Simulation::patrol(unsigned i, const Definition &d, const course_walls::World *surfaces) {
    auto &a = actors_[i];
    auto &p = state_.poses[i];
    if (d.kind == Kind::Hedgehog) {
        const float speed = d.speed > 0 ? d.speed : (.5f + (d.phase % 6) * .1f) * scale;
        if (a.stage == 0 || a.stage == 2) {
            if (move_towards(p.position, a.stage == 0 ? d.target : d.position, speed)) {
                ++a.stage;
                a.timer = 60;
            }
        } else if (--a.timer < 0) {
            a.stage = a.stage == 1 ? 2 : 0;
        }
        frame(p, d, (a.animation++ / 5) % 2);
        grounded(p, surfaces, scale, 6);
        return;
    }
    const unsigned stage_before = a.stage;
    switch (a.stage) {
    case 0:
        if (move_towards(p.position, d.target, 1.5f * scale)) {
            a.stage = 1;
            a.timer = int(random_next(a.random) % 60);
        }
        break;
    case 1:
    case 3:
        if (--a.timer < 0) {
            ++a.stage;
            a.timer = 60;
        }
        break;
    case 2:
    case 4:
        if (a.timer-- > 0) {
            const float speed = a.stage == 2 ? -.8f * scale : .8f * scale;
            for (unsigned k = 0; k < 2; ++k)
                p.position[k] += a.velocity[k] * speed;
        } else {
            a.stage = a.stage == 2 ? 3 : 1;
            a.timer = int(random_next(a.random) % 60);
        }
        break;
    default:
        a.stage = 0;
        break;
    }
    if (a.stage != stage_before)
        a.animation = 0;
    const bool waiting = a.stage == 1 || a.stage == 3;
    frame(p, d, (waiting ? 4 : 0) + (a.animation++ / 2) % (waiting ? 3 : 4));
    grounded(p, surfaces, scale, 2.5f);
}

void Simulation::plant(unsigned i, const Definition &d, std::span<const Racer>) {
    auto &a = actors_[i];
    auto &p = state_.poses[i];
    // Unlike update_object sprites, plants are racing actors. NTSC1P updates
    // them twice per nominal30Hz drawn frame. Retain that cadence, but not the
    // donor's distant frame-zero shortcut: imported courses stay visible beyond
    // that range. Advance the shared pose even without a nearby racer, so views
    // and online peers never see a plant freeze or restart as they approach.
    for (unsigned step = 0; step < 2; ++step)
        if (++a.animation > 60)
            a.animation = 6;
    frame(p, d, std::min(8u, a.animation / 6));
}

void Simulation::snowman(unsigned i, const Definition &d) {
    auto &a = actors_[i];
    auto &p = state_.poses[i];
    if (d.subtype == 1) {
        // The fixed0x8000 sprite roll is baked into the imported model. Only
        // the original +/-0x1000 animated delta belongs in the live pose.
        const unsigned phase = (state_.clock / 2 + d.phase) % 16;
        p.rotation[2] = static_cast<std::uint16_t>(phase < 8 ? int(phase) * 1024 - 4096
                                                             : 12288 - int(phase) * 1024);
        if (a.stage == 10) {
            if (a.timer-- > 0) {
                a.velocity[2] -= .5f * scale;
                a.offset[2] += a.velocity[2];
            } else {
                a.stage = 11;
                a.velocity[2] = 0;
            }
        } else if (a.stage == 11) {
            a.velocity[2] -= .2f * scale;
            a.offset[2] = std::max(-7 * scale, a.offset[2] + a.velocity[2]);
            if (a.offset[2] <= -7 * scale)
                a.stage = 12;
        } else if (a.stage == 20) {
            if (approach(a.offset[2], 0.f, .2f * scale))
                a.stage = 0;
        }
        p.position[2] = d.position[2] + a.offset[2];
        return;
    }
    if (a.stage == 10 && --a.timer < 0) {
        a.stage = 11;
        a.timer = 10;
        for (unsigned head = 0; head < state_.count; ++head)
            if (actors_[head].kind == Kind::Snowman && actors_[head].parent == d.id)
                actors_[head].stage = 20;
    } else if (a.stage == 11 && --a.timer < 0) {
        a.stage = 12;
        p.active = 2;
        p.visual_scale = .01f;
    } else if (a.stage == 12) {
        p.visual_scale = std::min(1.f, p.visual_scale + .025f);
        if (p.visual_scale >= .99999f) {
            p.visual_scale = 1;
            p.active = 1;
            a.stage = 0;
            ++p.generation;
        }
    }
}

void Simulation::hit(unsigned id, Vec racer_velocity) noexcept {
    scenery_.hit(id, racer_velocity, state_);
    for (float v : racer_velocity)
        if (!std::isfinite(v))
            return;
    for (unsigned i = 0; i < state_.count; ++i) {
        auto &a = actors_[i];
        auto &p = state_.poses[i];
        if (a.id != id || p.active != 1)
            continue;
        if (a.kind == Kind::Mole && a.stage != 10) {
            const float speed = horizontal_length(racer_velocity) / hz;
            a.stage = 10;
            a.timer = 0;
            a.animation = 0;
            a.velocity = {};
            if (speed > 0)
                for (unsigned k = 0; k < 2; ++k)
                    a.velocity[k] = racer_velocity[k] / hz * ((speed + scale) / speed);
            a.velocity[2] = std::min(speed * .5f + 3 * scale, 5 * scale);
            if (speed + scale >= 4 * scale)
                a.velocity[2] = 4 * scale; // Original81D34's vertical cap branch.
            p.active = 2;
            ++p.generation;
        } else if (a.kind == Kind::Snowman && a.parent == ~0u && !a.stage) {
            a.stage = 10;
            a.timer = 300;
            p.active = 0;
            ++p.generation;
            for (unsigned head = 0; head < state_.count; ++head) {
                auto &h = actors_[head];
                if (h.kind == Kind::Snowman && h.parent == id) {
                    h.stage = 10;
                    h.timer = 10;
                    h.velocity = {0, 0, 10 * scale};
                    ++state_.poses[head].generation;
                }
            }
        }
        return;
    }
}
} // namespace rr64::course_hazards
