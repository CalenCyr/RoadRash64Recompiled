#pragma once
#include "rr64_actor_render_snapshot.hpp"

namespace rr64::video {
// Select the original full-height mode writer. Its normal VI/framebuffer path
// still owns sizing and allocation. Never persist a synthetic console mode.
inline unsigned combined_callback(unsigned char* memory, unsigned original, bool enabled) noexcept {
    if (!enabled || !lod::supported_scene(memory)) return original;
    std::uint32_t width = 0, height = 0, lock = 0, callback = 0;
    if (!engine::read_u32(memory, 0x800bc9c4u, width) || width < 512u ||
        !engine::read_u32(memory, 0x800bc9ccu, height) || height < 240u ||
        !engine::read_u32(memory, 0x800a4f24u, lock) || lock != 0u ||
        !engine::read_u32(memory, 0x8009cc68u, callback) || callback != 0x8000a3a8u)
        return original;
    return callback;
}
}
