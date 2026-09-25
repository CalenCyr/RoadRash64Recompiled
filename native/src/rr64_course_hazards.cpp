#include "rr64_course_hazards.hpp"
#include "rr64_highlights.hpp"
#include "rr64_course_hazard_motion.hpp"
#include "rr64_course_hazard_render.hpp"
#include "rr64_engine_layout.hpp"
#include "rr64_experimental_course.hpp"
#include "rr64_prediction_replay.hpp"
#include "rr64_netplay.hpp"
#include "rr64_online_flow.hpp"
#include <algorithm>
#include <cmath>
#include <cstdio>

namespace rr64::course_hazards {
namespace {
struct Runtime {
    Simulation simulation;
    netplay::CourseHazardState remote{}, previous{}, presented{};
    std::uint32_t round = 0;
    std::uint64_t tick = 0;
    float elapsed = -1;
    double remainder = 0;
    bool initialized = false, remote_valid = false;
} live;
} // namespace
void reset_runtime() noexcept {
    if (!prediction::active())
        live = {};
}
void notify_hit(unsigned id, Vec velocity) noexcept {
    const auto *course = data();
    if (!course || !live.initialized || live.remote_valid || prediction::active() ||
        id >= course->definitions.size())
        return;
    const auto rules = netplay::get_physics_rules();
    if (rules.active && (!rules.connected || !rules.authoritative || !rules.is_host ||
                         rules.phase != netplay::Phase::Race))
        return;
    const auto before = live.simulation.state();
    live.simulation.hit(id, velocity);
    const auto &after = live.simulation.state();
    // Retire the old contact pose immediately for all riders in this substep.
    // Unaffected actors retain the already interpolated presentation interval.
    for (unsigned i = 0; i < after.count; ++i)
        if (after.poses[i] != before.poses[i])
            live.previous.poses[i] = live.presented.poses[i] = after.poses[i];
}
netplay::CourseHazardState capture_state() noexcept {
    if (!experimental_course::active())
        return {};
    return live.remote_valid ? live.remote : live.presented;
}
bool apply_state(const netplay::CourseHazardState &state, std::uint32_t round,
                 std::uint64_t tick) noexcept {
    const auto *course = data();
    const auto count = course ? course->definitions.size() : 0;
    if (prediction::active() || !round || !tick || !netplay::valid_course_hazard_state(state) ||
        state.count != count)
        return false;
    for (unsigned i = 0; i < state.count; ++i) {
        const auto &d = course->definitions[i];
        if (d.kind == Kind::Thwomp ? state.poses[i].model > 5
                                   : (state.poses[i].model < d.model ||
                                      state.poses[i].model - d.model >= d.animation_frames))
            return false;
    }
    if (live.round == round) {
        if (tick < live.tick || (live.remote_valid && state.clock < live.remote.clock))
            return false;
        if (tick == live.tick)
            return state == live.remote;
    }
    live.remote = state;
    live.round = round;
    live.tick = tick;
    live.remote_valid = true;
    return true;
}
DynamicMotion resolve(std::span<const course_walls::Sphere> spheres, Vec displacement, Vec velocity,
                      float delta, bool predicted) noexcept {
    const auto *course = data();
    if (!course || !live.initialized || live.remote_valid || prediction::active()) {
        DynamicMotion unchanged;
        unchanged.displacement = displacement;
        unchanged.velocity = velocity;
        return unchanged;
    }
    if (!predicted)
        return resolve_state(*course, live.presented, spheres, displacement, velocity, delta);
    auto future = live.presented;
    for (unsigned i = 0; i < future.count; ++i)
        for (unsigned axis = 0; axis < 3; ++axis)
            future.poses[i].position[axis] += future.poses[i].velocity[axis] * delta;
    return resolve_state(*course, future, spheres, displacement, velocity, delta);
}
} // namespace rr64::course_hazards

extern "C" void rr64_course_hazards_step(unsigned char *memory) {
    using namespace rr64;
    using namespace course_hazards;
    if (!memory || prediction::active() || !experimental_course::active())
        return;
    std::uint16_t pause = 0;
    if (!engine::read_u16(memory, engine::globals::gameplay_pause_state, pause) || pause)
        return;
    const auto *course = data();
    if (!course || course->definitions.empty())
        return;
    const auto status = netplay::get_physics_rules();
    if (status.active && (!status.connected || !status.authoritative || !status.is_host ||
                          status.phase != netplay::Phase::Race))
        return;
    float elapsed = 0, dt = 0;
    if (!engine::read_float(memory, 0x800D7670, elapsed) || !std::isfinite(elapsed) ||
        elapsed < 0 || elapsed > 1000000)
        return;
    if (!engine::read_float(memory, engine::globals::physics_delta, dt) || !std::isfinite(dt) ||
        dt <= 0 || dt > .25f)
        return;
    if (!live.initialized || elapsed < live.elapsed) {
        reset_runtime();
        live.simulation.reset(*course);
        live.initialized = true;
        live.previous = live.presented = live.simulation.state();
        std::array<unsigned, static_cast<unsigned>(Kind::Sign) + 1> families{};
        for (const auto &definition : course->definitions)
            ++families[static_cast<unsigned>(definition.kind)];
        unsigned kinds = 0;
        for (const auto count : families)
            kinds += count != 0;
        std::fprintf(stderr,
                     "[course-actors] initialized actors=%zu kinds=%u shared-clock=30Hz "
                     "moles=%u penguins=%u chomps=%u particles=%u world-scale=%.6f\n",
                     course->definitions.size(), kinds, families[static_cast<unsigned>(Kind::Mole)],
                     families[static_cast<unsigned>(Kind::Penguin)],
                     families[static_cast<unsigned>(Kind::Chomp)],
                     families[static_cast<unsigned>(Kind::Smoke)], course->source_to_world_scale);
    }
    live.elapsed = elapsed;
    live.remainder += dt;
    std::array<Racer, 14> racers{};
    const auto *surfaces = course_walls::surface_world();
    unsigned size = 0, count = 0;
    if (engine::read_u32(memory, 0x800A656C, count) && count <= racers.size())
        for (unsigned i = 0; i < count; ++i) {
            const unsigned actor = 0x800D8570 + i * 0x118;
            unsigned bike = 0, route = 0;
            std::uint16_t active = 0, busted = 0, finished = 0;
            if (!engine::read_u16(memory, actor + 0x24, active) || !active ||
                !engine::read_u32(memory, actor + 0xE0, bike) ||
                !engine::valid_guest_range(bike, engine::bike::stride) ||
                !engine::read_u32(memory, actor + 0xE8, route) ||
                !engine::valid_guest_range(route, 0x64) ||
                !engine::read_u16(memory, route + 0x4C, busted) || busted ||
                !engine::read_u16(memory, route + 0x52, finished) || finished)
                continue;
            Racer rider;
            engine::read_u32(memory, route + 0x40, rider.recovery_count);
            unsigned controller = ~0u;
            engine::read_u32(memory, actor + 8, controller);
            rider.slot = status.active ? online_flow::mapped_slot(i, status.local_slot,
                                                                  status.replicated_riders)
                                       : i;
            rider.human = status.active
                              ? (rider.slot < 14 && (status.authority_humans & (1u << rider.slot)))
                              : controller < 4;
            bool valid = true;
            for (unsigned axis = 0; axis < 3; ++axis)
                valid &=
                    engine::read_float(memory, bike + 0x16C + 4 * axis, rider.position[axis]) &&
                    std::isfinite(rider.position[axis]) &&
                    engine::read_float(memory, bike + 0x178 + 4 * axis, rider.velocity[axis]) &&
                    std::isfinite(rider.velocity[axis]);
            if (valid && rider.human && surfaces && !course->grass_triangles.empty()) {
                // DK's Kiwano is a grass trigger, not an arbitrary off-road
                // radius. Query the original material under this rider only.
                auto above = rider.position;
                above[2] += 2 * course->source_to_world_scale;
                const auto ground = course_walls::sweep_sphere(
                    *surfaces, {above, .01f}, {0, 0, -12 * course->source_to_world_scale});
                rider.source_grass =
                    ground.hit && ground.normal[2] > .3f &&
                    std::binary_search(course->grass_triangles.begin(),
                                       course->grass_triangles.end(), ground.triangle_id);
            }
            if (valid)
                racers[size++] = rider;
        }
    // Source motion has one clock, independent of render FPS and split count.
    // Pauses and isolated prediction never advance it or mutate its RNG/state.
    const auto before = live.presented;
    while (live.remainder + 1e-9 >= 1.0 / 30.0) {
        live.previous = live.simulation.state();
        live.simulation.advance(*course, std::span(racers).first(size), surfaces);
        live.remainder = std::max(0.0, live.remainder - 1.0 / 30.0);
    }
    // One source-frame presentation delay provides a continuous kinematic
    // trajectory at native substep rates. Every rider queries this exact same
    // interval; two physics substeps cannot collide with one repeated interval.
    live.presented = live.simulation.state();
    const float alpha = static_cast<float>(live.remainder * 30);
    for (unsigned i = 0; i < live.presented.count; ++i) {
        auto &p = live.presented.poses[i];
        const auto &a = live.previous.poses[i];
        if (p.generation != a.generation || p.generation != before.poses[i].generation) {
            p.velocity = {};
            continue;
        }
        for (unsigned axis = 0; axis < 3; ++axis) {
            p.position[axis] = a.position[axis] + (p.position[axis] - a.position[axis]) * alpha;
            p.velocity[axis] = (p.position[axis] - before.poses[i].position[axis]) / dt;
            const auto turn = static_cast<std::int16_t>(p.rotation[axis] - a.rotation[axis]);
            p.rotation[axis] =
                static_cast<std::uint16_t>(a.rotation[axis] + static_cast<int>(turn * alpha));
        }
        p.visual_scale = a.visual_scale + (p.visual_scale - a.visual_scale) * alpha;
    }
}

namespace {
void draw_course_hazards(unsigned char *memory, bool before_terrain) {
    using namespace rr64;
    using namespace course_hazards;
    if (!memory || prediction::active() || !experimental_course::active())
        return;
    const auto *course = data();
    if (!course)
        return;
    // Rainbow's neon course draws its actors before its translucent road in
    // MK64 (render_object -> func_8029122C). The road does not write depth, so
    // drawing these actors afterward paints the signs over foreground road.
    // Keep the whole source actor pass together, including the Chomps. This
    // also preserves one display list per view: a second pass would overwrite
    // the renderer's frame scratch. Other courses retain their original stage.
    const bool neon_course = std::any_of(course->definitions.begin(), course->definitions.end(),
                                        [](const auto &d) { return d.kind == Kind::Neon; });
    if (before_terrain != neon_course)
        return;
    const auto *recorded = rr64::highlights::render_hazards();
    const auto state = recorded ? *recorded : capture_state();
    if (state.count != course->definitions.size())
        return;
    std::array<HazardDrawState, netplay::kMaximumCourseHazards> draw{};
    for (unsigned i = 0; i < state.count; ++i) {
        const auto &p = state.poses[i];
        auto &out = draw[i];
        out.id = i;
        out.model = p.model;
        out.position = p.position;
        out.rotation = p.rotation;
        out.scale = course->definitions[i].scale * p.visual_scale;
        out.billboard = static_cast<unsigned>(course->definitions[i].billboard);
        out.opacity = p.opacity;
        out.tint = p.tint;
        out.environment_tint = p.environment_tint;
        out.source_to_world_scale = course->source_to_world_scale;
        out.visible = p.active != 0;
    }
    draw_hazards(memory, std::span(draw).first(state.count));
}
} // namespace

extern "C" void rr64_course_hazards_draw_before_terrain(unsigned char *memory) {
    draw_course_hazards(memory, true);
}

extern "C" void rr64_course_hazards_draw(unsigned char *memory) {
    draw_course_hazards(memory, false);
}
