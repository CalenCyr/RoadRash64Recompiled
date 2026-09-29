#pragma once
#include "rr64_mk64_item_state.hpp"
#include <span>
#include <string>

namespace rr64::mk64_items {
// Install only between sessions, after reset_render_session. The importer pack
// authenticates its user-ROM bank; this boundary also validates every record.
bool install_render_asset(std::span<const std::uint8_t>, std::string &error);
bool render_asset_available() noexcept;
void clear_render_asset();
void reset_render_session();
// Current native viewport, source2 projection and depth buffer. Positions are
// authoritative rider-world units; only original local model vertices scale.
bool draw_world(unsigned char *memory, const Snapshot &);
// Explicit native view and logical framebuffer coordinates. Inventory values
// remain untouched; the HUD adapter supplies the original weapon rectangle.
bool draw_hud_rectangle(unsigned char *memory, unsigned view, Item shown, float x, float y,
                        float width, float height);
struct RenderStatistics {
    std::uint64_t world_draws = 0, hud_draws = 0, refusals = 0;
    unsigned objects = 0, lightning_strikes = 0, triangles = 0, command_bytes = 0;
};
RenderStatistics render_statistics();
} // namespace rr64::mk64_items
