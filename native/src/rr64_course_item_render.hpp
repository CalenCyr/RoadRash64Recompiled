#pragma once
#include <array>
#include <cstdint>
#include <limits>
#include <span>
#include <string>

namespace rr64::course_items {
inline constexpr unsigned maximum_render_boxes = 64;
struct ItemBoxDrawState {
    std::uint32_t id = 0;
    // Settled center in RR rider-world units, including atlas translation.
    std::array<float, 3> position{};
    // Original MK64 x/y/z angle units, modulo65536. Host authority owns time.
    std::array<std::uint16_t, 3> rotation{};
    std::uint8_t state = 2; // 2 available, 3 shattering, 5 elevated special.
    float break_age = 0;    // Original update units, finite0..20.
    // Optional native floor query result in rider-world Z; NaN omits shadow.
    float ground_height = std::numeric_limits<float>::quiet_NaN();
    // Source-local vertices/break offsets only; position is already world-space.
    float source_to_world_scale = .05f;
};
// Private asset: "MKIBOX01",512 original big-endian vertex bytes,4096 RGBA16
// bytes (32x64). Caller authenticates its catalogue digest before installation.
// Install/clear only between sessions, after reset_render_session().
bool install_render_asset(std::span<const std::uint8_t> asset, std::string &error);
void clear_render_asset();
// Called when the guest allocator resets, before any draw. Old guest pointers
// are abandoned, never freed through a new guest heap. Host asset is retained.
void reset_render_session();
// At func8007C524 before7C7D0, after existing world-object presentation. Uses
// current viewport's native source2 matrices; independent of MaxViewDistance.
bool draw_boxes(unsigned char *rdram, std::span<const ItemBoxDrawState> boxes);
struct ItemRenderStatistics {
    std::uint64_t draws = 0, refusals = 0;
    unsigned boxes = 0, triangles = 0, command_bytes = 0, matrix_bytes = 0;
};
ItemRenderStatistics render_statistics();
}
