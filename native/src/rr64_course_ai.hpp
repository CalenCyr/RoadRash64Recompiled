#pragma once
#ifdef __cplusplus
#include "rr64_course_hazards.hpp"
namespace rr64::course_ai {
struct Decision {
    bool brake = false;
    float horizon = 0;
    unsigned wall = 0, hazard = 0;
    // A safe alternative remains a steering request, never a pose correction.
    float lateral = 0;
    bool steer = false;
};
// Read-only, bounded lookahead. Positive turn_rate is radians/second from +x
// toward +y; zero preserves linear prediction. No route or actor is moved.
Decision inspect(const course_walls::World *, const course_hazards::Data *,
                 const netplay::CourseHazardState &, std::span<const course_walls::Sphere>,
                 course_walls::Vec velocity, course_walls::Vec forward, float braking_acceleration,
                 float turn_rate = 0) noexcept;
// Prefer a swept, supported side route over stopping in front of a finite
// obstacle. The caller supplies its native lateral normal and usable offsets.
// An absent floor world permits fixture-only geometric queries; production
// always supplies the selected course's immutable surface world.
Decision choose(const course_walls::World *, const course_walls::World *floor,
                const course_hazards::Data *, const netplay::CourseHazardState &,
                std::span<const course_walls::Sphere>, course_walls::Vec velocity,
                course_walls::Vec forward, course_walls::Vec lateral_normal,
                float braking_acceleration, float left, float right, float preferred_side = 0,
                float turn_rate = 0) noexcept;
}
extern "C" {
#endif
// func8004EB6C, before8004F060: s3=actor; sp+48=held, sp+64=engine target.
void rr64_course_ai_avoid(unsigned char *memory, void *context);
// Fit temporary f2 clearance to native lateral bounds, preserving normal AI.
// kind0: 5384C before53B88; kind1: 52BFC before537E8 (lookahead clamp).
void rr64_course_ai_fit_corridor(unsigned char *memory, void *context, unsigned kind);
// 4EB6C before4EE50: f12=lateral error; s3=actor,s4=AI,s2=projection.
// Consume the current target pass: unobstructed route interiors are free lines.
// Preserve native direction, damping, edge recovery and obstacle-selected targets.
void rr64_course_ai_relax_line(unsigned char *memory, void *context);
// 5A9DC before5ABC0: fp=actor,v0=native curve or -1. Reacquire only
// failed imported guidance; native remount, world pose and scoring are intact.
void rr64_course_ai_reacquire(unsigned char *memory, void *context);
//4EB6C before4ED04: v0=projection result; s3=actor,s2=projection,s5=rider.
// Retry failed imported reacquisition once; original progress checks follow.
void rr64_course_ai_retry_projection(unsigned char *memory, void *context);
#ifdef __cplusplus
}
#endif
