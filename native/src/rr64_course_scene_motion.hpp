#pragma once
#include "rr64_course_hazards.hpp"

namespace rr64::course_hazards {
struct Racer;
// Original course scenery and articulated actors share the same host clock as
// the collision families. Rendering never advances their timers or RNG.
class SceneSimulation {
  public:
    void reset(const Data &, netplay::CourseHazardState &);
    void advance(const Data &, std::span<const Racer>, netplay::CourseHazardState &);
    void hit(unsigned id, Vec velocity, netplay::CourseHazardState &) noexcept;

  private:
    struct Actor {
        Vec origin{}, velocity{};
        unsigned stage = 0, node = 0, timer = 0, animation = 0;
        std::uint16_t angle = 0;
        float speed = 0, phase = 0;
        bool triggered = false;
        Kind kind = Kind::Thwomp;
        Vec jitter{};
        unsigned age = 0, cycle = 0, target_slot = ~0u, path_clock = 0, seen_generation = 0;
        std::uint32_t random = 1;
        Vec target_previous{};
        float target_speed = 0;
        std::uint32_t target_recovery = 0;
        bool target_valid = false;
    };
    std::array<Actor, netplay::kMaximumCourseHazards> actors_{};
    float scale_ = .05f;
    static bool target_relocated(Actor &, const Racer &) noexcept;
    void reset_effect(unsigned, const Definition &, const Data &, netplay::CourseHazardState &);
    void effect(unsigned, const Definition &, const Data &, std::span<const Racer>,
                netplay::CourseHazardState &);
};
} // namespace rr64::course_hazards
