#pragma once
#include <array>
#include <cmath>
#include <cstdint>

namespace rr64::prediction {
// Pointer-free state for 34594/348D8; not a complete collision/race snapshot.
struct IntegratorState {
    std::array<float,7> translation{}; // +64: position, velocity, speed
    std::array<float,6> force{};       // +F4: force and accumulated impulse
    std::array<float,28> rotation{};   // +10C: angles, basis, quaternion,
                                     // angular motion, torque and impulse
    std::uint16_t flags=0;            // +60: includes the native resting bit
    bool operator==(const IntegratorState&) const = default;
};
inline bool valid_integrator(const IntegratorState &s) {
    for(float f:s.translation)if(!std::isfinite(f))return false;
    for(float f:s.force)if(!std::isfinite(f))return false;
    for(float f:s.rotation)if(!std::isfinite(f))return false;
    return true;
}
}
