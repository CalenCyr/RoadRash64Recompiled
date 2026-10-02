#pragma once

#ifdef __cplusplus
#include <bit>
#include <cstdint>

namespace rr64::terrain_graphics {
// Compiled world geometry owns visible distance. Keep its redundant native
// display-list cache at the first authored tier; this is not the camera clip.
inline unsigned streaming_range(unsigned bits, bool compiled) noexcept {
    const float range = std::bit_cast<float>(bits);
    return compiled && range > 999.0f ? std::bit_cast<unsigned>(999.0f) : bits;
}

struct PoolState {
    bool valid = false;
    unsigned base = 0, capacity = 0, blocks = 0, used = 0, free = 0, largest = 0;
    unsigned selected = 0;
};
// Read-only first-fit admission. Pool2 remains in the original low guest RAM,
// so historical online images retain identical allocation and free behavior.
PoolState inspect_pool(unsigned char* memory, unsigned requested) noexcept;
}
extern "C" {
#endif
#ifdef __cplusplus
void rr64_terrain_pool_require(unsigned char* memory, unsigned pool, unsigned requested) noexcept(false);
#else
void rr64_terrain_pool_require(unsigned char* memory, unsigned pool, unsigned requested);
#endif
unsigned rr64_terrain_streaming_range(unsigned char* memory, unsigned original_bits);
int rr64_terrain_streaming_bounded(unsigned char* memory);
#ifdef __cplusplus
}
#endif
