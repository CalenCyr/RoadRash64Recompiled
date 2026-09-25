#pragma once
#include "rr64_course_hazards.hpp"
#include "rr64_course_scene_motion.hpp"

namespace rr64::course_hazards {
struct Racer {
    Vec position{}, velocity{};
    unsigned slot = 0;
    bool human = false, source_grass = false;
    Vec route_position{};
    std::uint32_t recovery_count = 0;
};
// Explicit source-frame state, independent of rendering, network arrival and
// guest memory. This also makes complete multi-minute motion traces testable.
class Simulation {
  public:
    void reset(const Data &);
    void advance(const Data &, std::span<const Racer>, const course_walls::World *surfaces);
    // Called only for an actual host/offline collision, never for a nearby
    // camera or a client presentation overlap. IDs are immutable pack IDs.
    void hit(unsigned id, Vec racer_velocity) noexcept;
    const netplay::CourseHazardState &state() const noexcept { return state_; }

  private:
    struct Actor {
        Vec offset{}, velocity{};
        unsigned stage = 0, node = 0, leg = 0;
        int timer = 0;
        float lane = 0;
        Kind kind = Kind::Thwomp;
        unsigned id = 0, parent = ~0u, animation = 0;
        std::uint32_t random = 1;
    };
    netplay::CourseHazardState state_{};
    std::array<Actor, netplay::kMaximumCourseHazards> actors_{};
    SceneSimulation scenery_;
    float scale = .05f;
    std::uint32_t random_ = 1;
    void thwomp(unsigned, const Definition &, std::span<const Racer>);
    void rock(unsigned, const Definition &, const course_walls::World *);
    void vehicle(unsigned, const Definition &, const Path &);
    void reset_sprite(unsigned, const Definition &);
    void spawn_moles(const Data &, std::span<const Racer>);
    void mole(unsigned, const Definition &);
    void patrol(unsigned, const Definition &, const course_walls::World *);
    void plant(unsigned, const Definition &, std::span<const Racer>);
    void snowman(unsigned, const Definition &);
};
// Shared by live integration and standalone collision fixtures.
DynamicMotion resolve_state(const Data &, const netplay::CourseHazardState &,
                            std::span<const course_walls::Sphere>, Vec displacement, Vec velocity,
                            float delta) noexcept;
} // namespace rr64::course_hazards
