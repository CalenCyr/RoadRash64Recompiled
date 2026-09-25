#pragma once
#ifdef __cplusplus
#include "rr64_course_hazard_state.hpp"
#include "rr64_course_walls.hpp"
#include <vector>

namespace rr64::course_hazards {
using Vec = std::array<float, 3>;
enum class Kind {
    Thwomp,
    Rock,
    Train,
    Traffic,
    Mole,
    Crab,
    Hedgehog,
    Plant,
    Snowman,
    Egg,
    Penguin,
    Chomp,
    Kiwano,
    Crossing,
    Ferry,
    Bat,
    Boo,
    Fish,
    Flame,
    Smoke,
    Wheel,
    Neon,
    Balloon,
    Seagull,
    Sign
};
enum class Billboard { None, Yaw, Full };
struct Definition {
    unsigned id = 0, model = 0, subtype = 0, phase = 0, path = 0, node = 0;
    Kind kind = Kind::Thwomp;
    Vec position{}, offset{}, half_extent{};
    float scale = 1, speed = 0, minimum_height = -100, lane = 0;
    // Optional source actor animation. Models are a contiguous immutable clip;
    // frame choice and interaction belong to the shared authoritative clock.
    unsigned animation_frames = 1, frame_ticks = 1, parent = ~0u;
    unsigned collision_model = ~0u; // Original whole train when wheels draw separately.
    Vec target{};                   // Authored patrol endpoint in rider world, not collider offset.
    Billboard billboard = Billboard::None;
    bool solid = true;
    float collision_radius = 0; // Source sphere, world-scaled; zero retains box.
    std::array<std::uint16_t, 3> rotation{};
    // Optional authored frame schedule and separate skeletal clips.
    std::vector<unsigned> frame_sequence;
    std::vector<bool> visible_sequence;
    unsigned animation_loop_start = 0;
    std::array<unsigned, 3> clip_start{}, clip_count{};
};
struct Path {
    std::vector<Vec> points, left, right;
    // Optional source B-spline control durations; local effect paths carry no
    // atlas translation, unlike absolute vehicle paths.
    std::vector<unsigned> durations;
};
struct Data {
    std::vector<Definition> definitions;
    std::vector<Path> paths;
    // Definitions/path coordinates are already world-scaled. This factor is
    // only for source-local model sizes and source-authored motion constants.
    float source_to_world_scale = .05f;
    std::vector<unsigned> grass_triangles; // Sorted donor surface IDs for Kiwano triggers.
};
const Data *data() noexcept; // Immutable, selected course; loader owns lifetime.
void reset_runtime() noexcept;
void notify_hit(unsigned id, Vec velocity) noexcept;
netplay::CourseHazardState capture_state() noexcept;
bool apply_state(const netplay::CourseHazardState &, std::uint32_t round,
                 std::uint64_t tick) noexcept;
struct DynamicMotion {
    Vec displacement{}, velocity{}, normal{}, point{}, surface_velocity{};
    float normal_speed = 0, penetration = 0;
    unsigned contacts = 0, id = 0;
};
// Pure geometric query. Caller decides authority and applies native impulse.
// delta is the current physics substep, not the 30Hz source-animation step.
DynamicMotion resolve(std::span<const course_walls::Sphere>, Vec displacement, Vec velocity,
                      float delta, bool predicted = false) noexcept;
} // namespace rr64::course_hazards
extern "C" {
#endif
void rr64_course_hazards_step(unsigned char *memory);
void rr64_course_hazards_draw_before_terrain(unsigned char *memory);
void rr64_course_hazards_draw(unsigned char *memory);
#ifdef __cplusplus
}
#endif
