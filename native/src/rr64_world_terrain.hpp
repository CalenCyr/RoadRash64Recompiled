#pragma once
#include "rr64_world_terrain_assets.hpp"
#include <array>

namespace rr64::world {
using Matrix = std::array<float, 16>;
// Identity-quaternion 7D814 convention: Z basis is halved, translation is raw.
Matrix terrain_matrix(const TerrainCellAsset& cell, float origin_x, float origin_y) noexcept;
bool terrain_in_frustum(const TerrainCellAsset& cell, const Matrix& model,
    const Matrix& view, const Matrix& projection) noexcept;
struct TerrainStatistics {
    unsigned cached_cells = 0, cached_triangles = 0, cached_bytes = 0;
    unsigned visible_cells = 0, stock_cells = 0, drawn_triangles = 0;
    unsigned course_excluded_cells = 0;
    unsigned stock_course_excluded = 0;
    unsigned long long frames = 0, refusals = 0;
};
TerrainStatistics terrain_statistics() noexcept;
// Called by the existing runtime immediately before it reinitializes its heap.
void terrain_reset_session() noexcept;
}
extern "C" {
void rr64_world_terrain_begin(unsigned char* rdram);
unsigned rr64_world_terrain_stock_state(unsigned char* rdram,unsigned record,unsigned state);
void rr64_world_terrain_observe(unsigned char* rdram, unsigned record);
void rr64_world_terrain_draw(unsigned char* rdram);
}
