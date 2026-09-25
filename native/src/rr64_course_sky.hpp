#pragma once

#ifdef __cplusplus
#include <array>
#include <cstdint>
#include <vector>

namespace rr64::course_sky {
struct Texture {
    unsigned width = 0, height = 0;
    std::vector<std::uint8_t> intensity4;
};
struct Sprite {
    unsigned yaw = 0, texture = 0;
    float elevation = 0, size = 1;
};
// Optional, authenticated private-pack data. No donor artwork is compiled in.
struct Definition {
    bool enabled = false, stars = false;
    unsigned identity = 1;
    std::array<float, 3> center{};
    float radius = 1;
    std::array<std::array<std::uint8_t, 3>, 4> colors{};
    std::vector<Texture> textures;
    std::vector<Sprite> sprites;
};
const Definition *current() noexcept;
void reset() noexcept;
bool draw(unsigned char *memory, const Definition &sky);
}
extern "C" {
#endif
void rr64_course_sky_draw(unsigned char *memory);
#ifdef __cplusplus
}
#endif
