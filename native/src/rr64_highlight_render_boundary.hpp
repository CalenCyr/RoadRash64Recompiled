#pragma once

#ifdef __cplusplus
#include <cstdint>

namespace rr64::highlight_render {
// Reserved projection identities, carried by the actual display list rather
// than a live global which could change before the renderer consumes the task.
constexpr std::uint32_t marker_prefix = 0x484c0000u;
class Boundary {
    const void *mapping_ = nullptr;
    std::uint32_t epoch_ = 0, key_ = 0, serial_ = 0;
    bool armed_ = false;
    void advance() noexcept { if (++serial_ > 0xffffu) serial_ = 1; }
public:
    std::uint32_t observe(const void *mapping, std::uint32_t epoch,
                          std::uint32_t replay_key) noexcept {
        if (mapping != mapping_ || epoch < epoch_) {
            mapping_ = mapping;
            key_ = 0;
            armed_ = false;
            advance();
        }
        epoch_ = epoch;
        if (replay_key != key_) {
            key_ = replay_key;
            armed_ = true;
            advance();
        }
        return armed_ ? marker_prefix | serial_ : 0u;
    }
};
}

extern "C" {
#endif
// Graphics-worker-only scope around one native 6A638 draw. Zero is ordinary
// racing/results; a nonzero key identifies the replay clip and camera angle.
void rr64_highlight_render_begin(unsigned char *memory, unsigned replay_key);
void rr64_highlight_render_projection(unsigned char *memory);
void rr64_highlight_render_end(unsigned char *memory);
#ifdef __cplusplus
}
#endif
