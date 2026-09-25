#pragma once
#include <array>
#include <cmath>
#include <cstdint>

namespace rr64::netplay {
// Matched course metadata owns shape/model permissions. Host poses travel with
// rider outcomes in one completed authority tick, never a separate event stream.
constexpr unsigned kMaximumCourseHazards = 128;
constexpr unsigned kMaximumCourseHazardModels = 2048;
struct CourseHazardPose {
    std::array<float, 3> position{}, velocity{}; // Rider world; third axis is height.
    std::array<std::uint16_t, 3> rotation{};     // Original MK64 model angles.
    std::uint16_t model = 0;
    std::uint32_t generation = 0, active = 0;
    // active:0 hidden,1 visible with authored collision,2 visible/non-solid.
    // Scale is an animation value (for example rebuilding snowman bodies).
    float visual_scale = 1;
    std::uint32_t opacity = 255;
    std::uint32_t tint = 0xffffff, environment_tint = 0;
    bool operator==(const CourseHazardPose &) const = default;
};
struct CourseHazardState {
    std::uint32_t clock = 0;
    std::uint16_t count = 0, reserved = 0;
    std::array<CourseHazardPose, kMaximumCourseHazards> poses{};
    bool operator==(const CourseHazardState &) const = default;
};
inline bool valid_course_hazard_pose(const CourseHazardPose &p) noexcept {
    if (p.active > 2 || p.opacity > 255 || p.tint > 0xffffff || p.environment_tint > 0xffffff ||
        p.model >= kMaximumCourseHazardModels || !std::isfinite(p.visual_scale) ||
        p.visual_scale <= 0 || p.visual_scale > 16)
        return false;
    for (unsigned i = 0; i < 3; ++i)
        if (!std::isfinite(p.position[i]) || std::abs(p.position[i]) > 100000 ||
            !std::isfinite(p.velocity[i]) || std::abs(p.velocity[i]) > 10000)
            return false;
    return true;
}
inline bool valid_course_hazard_state(const CourseHazardState &s) noexcept {
    if (s.count > kMaximumCourseHazards || s.reserved || (!s.count && s.clock))
        return false;
    for (unsigned i = 0; i < kMaximumCourseHazards; ++i)
        if (!valid_course_hazard_pose(s.poses[i]) ||
            (i >= s.count && s.poses[i] != CourseHazardPose{}))
            return false;
    return true;
}
} // namespace rr64::netplay
