#pragma once

#ifdef __cplusplus
#include <array>
#include <span>
#include <vector>

namespace rr64::course_boost {
using Vec = std::array<float, 3>; // Native rider-world x, z, height.
struct Pad {
    unsigned id = 0;
    std::vector<std::array<Vec, 3>> triangles;
    Vec direction{}; // Horizontal uphill direction; does not steer the bike.
    Vec lip{};
    float slope = 0;
    float minimum_speed = 0;
    float length = 0;
};
struct Data {
    std::vector<Pad> pads;
};
const Data *data() noexcept;
void validate(const Data &);
void reset_runtime() noexcept;
struct Result {
    bool applied = false;
    unsigned pad = 0;
    Vec delta{};
};
// Pure finite-surface query. A nearby wall, ramp back/underside or an airborne
// rider over the pad does not qualify. Only forward motion across its lip does.
Result launch(const Data &, Vec position, Vec velocity, float delta) noexcept;
} // namespace rr64::course_boost
extern "C" {
#endif
// Call before native3F580's walls_begin and34594. Updates mounted momentum only;
// native gravity, wheel contact, walls, crash damage and recovery remain in charge.
void rr64_course_boost_step(unsigned char *memory, unsigned bike);
#ifdef __cplusplus
}
#endif
