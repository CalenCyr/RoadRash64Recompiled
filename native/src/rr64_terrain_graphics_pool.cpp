#include "rr64_terrain_graphics_pool.hpp"
#include "rr64_engine_layout.hpp"

#include <algorithm>
#include <cstdio>
#include <stdexcept>

namespace rr64::terrain_graphics {
PoolState inspect_pool(unsigned char* memory, unsigned requested) noexcept {
    using namespace engine;
    PoolState out;
    if (!memory || !requested || requested > kRdramSize - 7u ||
        !read_u32(memory, 0x800bbd08u, out.base) ||
        !read_u32(memory, 0x800bbd48u, out.capacity) || out.capacity < 16u ||
        !valid_guest_range(out.base, out.capacity) || (out.base & 7u))
        return out;
    requested = (requested + 7u) & ~7u;
    const unsigned sentinel = out.base + out.capacity - 8u;
    unsigned cursor = out.base;
    // Every block advances by at least its eight-byte header. The bound is
    // independent of corrupt link data and requires the real end sentinel.
    for (unsigned count = 0; count < out.capacity / 8u; ++count) {
        unsigned size = 0;
        std::uint16_t alignment = 0;
        std::uint8_t busy = 0, padding = 0;
        if (cursor > sentinel || (cursor & 7u) ||
            !read_u32(memory, cursor, size) || !read_u16(memory, cursor + 4, alignment) ||
            !read_u8(memory, cursor + 6, busy) || !read_u8(memory, cursor + 7, padding) ||
            alignment != 8u || padding != 0u || busy > 1u)
            return out;
        if (!size) {
            out.valid = cursor == sentinel && busy == 1u;
            return out;
        }
        if (cursor == sentinel || (size & 7u) || size > sentinel - cursor - 8u)
            return out;
        ++out.blocks;
        if (busy) out.used += size;
        else {
            out.free += size;
            out.largest = std::max(out.largest, size);
            if (!out.selected && size >= requested) out.selected = cursor + 8u;
        }
        cursor += size + 8u;
    }
    return out;
}
}

extern "C" void rr64_terrain_pool_require(unsigned char* memory, unsigned pool,
                                            unsigned requested) noexcept(false) {
    if (pool != 2u) return;
    const auto state = rr64::terrain_graphics::inspect_pool(memory, requested);
    if (state.valid && state.selected) return;
    char message[224];
    std::snprintf(message, sizeof(message),
        "Terrain display-list pool %s: request=%u, free=%u, largest=%u, capacity=%u. "
        "Stopped before the original allocator's infinite wait.",
        state.valid ? "exhausted" : "invalid", requested, state.free,
        state.largest, state.capacity);
    // The guest graphics thread's terminal handler does not retain what().
    // Preserve this diagnostic on refusal only; successful frames do no I/O.
    std::fprintf(stderr, "[terrain-pool] %s\n", message);
    throw std::runtime_error(message);
}
