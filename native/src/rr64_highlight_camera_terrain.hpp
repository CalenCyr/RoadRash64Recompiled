#pragma once
#include "rr64_highlight_camera.hpp"

namespace rr64::highlight_camera::terrain {
struct Floor {
    float height = 0;
    unsigned surface = 0;
};
// Executes the original floor closure in private memory using the prepared
// immutable source cell. No live query caches, physics or streaming are used.
bool floor(unsigned char* memory, const Vec3& point, Floor& output) noexcept;
// Adjusts only a cinematic eye with a nearby real floor. A missing floor or
// distant surface is not a substitute ground plane; the recorded target stays.
bool clear_eye(unsigned char* memory, View& view) noexcept;
void reset() noexcept;
}
