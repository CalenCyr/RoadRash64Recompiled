#pragma once
#include <array>
#include <cstdint>
#include <span>
#include <string>

namespace rr64::mk64_items {
inline constexpr unsigned texture_count = 43, mesh_count = 6;
inline constexpr unsigned maximum_asset_bytes = 128 * 1024;
struct TextureAsset {
    unsigned width = 0, height = 0, offset = 0, bytes = 0;
};
struct MeshAsset {
    unsigned vertices = 0, triangles = 0, texture = 0;
    unsigned vertex_offset = 0, triangle_offset = 0;
    std::array<float, 3> center{};
    float half_extent = 0;
};
struct AssetLayout {
    std::array<TextureAsset, texture_count> textures{};
    std::array<MeshAsset, mesh_count> meshes{};
};
// The catalogue authenticates the complete bank. This second boundary rejects
// invalid counts, offsets, geometry and trailing data before guest allocation.
bool parse_item_assets(std::span<const std::uint8_t>, AssetLayout &, std::string &);
} // namespace rr64::mk64_items
