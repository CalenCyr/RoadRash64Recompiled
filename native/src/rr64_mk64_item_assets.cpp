#include "rr64_mk64_item_assets.hpp"
#include <algorithm>
#include <cstring>

namespace rr64::mk64_items {
bool parse_item_assets(std::span<const std::uint8_t> bytes, AssetLayout &output,
                       std::string &error) {
    AssetLayout result{};
    const auto fail = [&] {
        error = "Invalid or incomplete original MK64 item asset bank";
        return false;
    };
    if (bytes.size() < 16 || bytes.size() > maximum_asset_bytes ||
        std::memcmp(bytes.data(), "R64ITEM1", 8))
        return fail();
    std::size_t at = 8;
    const auto word = [&]() {
        if (at + 4 > bytes.size()) {
            at = bytes.size() + 1;
            return 0xFFFFFFFFu;
        }
        const unsigned value = unsigned(bytes[at]) | unsigned(bytes[at + 1]) << 8 |
                               unsigned(bytes[at + 2]) << 16 | unsigned(bytes[at + 3]) << 24;
        at += 4;
        return value;
    };
    if (word() != texture_count || word() != mesh_count)
        return fail();
    for (unsigned index = 0; index < texture_count; ++index) {
        auto &texture = result.textures[index];
        if (word() != index)
            return fail();
        texture.width = word();
        texture.height = word();
        texture.bytes = word();
        const unsigned width = index < 16 ? 40 : index == 41 ? 64 : 32;
        const unsigned height = index == 42 ? 64 : 32;
        if (texture.width != width || texture.height != height ||
            texture.bytes != width * height * 2 || at + texture.bytes > bytes.size() || (at & 7))
            return fail();
        texture.offset = unsigned(at);
        at += texture.bytes;
    }
    constexpr std::array<unsigned, mesh_count> vertex_counts{4, 4, 5, 6, 4, 24};
    constexpr std::array<unsigned, mesh_count> triangle_counts{2, 2, 2, 2, 2, 8};
    constexpr std::array<unsigned, mesh_count> texture_ids{16, 16, 40, 41, 42, 0xFFFFFFFFu};
    for (unsigned index = 0; index < mesh_count; ++index) {
        auto &mesh = result.meshes[index];
        if (word() != index)
            return fail();
        mesh.vertices = word();
        mesh.triangles = word();
        mesh.texture = word();
        if (mesh.vertices != vertex_counts[index] || mesh.triangles != triangle_counts[index] ||
            mesh.texture != texture_ids[index] || (at & 7) ||
            at + mesh.vertices * 16 + mesh.triangles * 4 > bytes.size())
            return fail();
        mesh.vertex_offset = unsigned(at);
        std::array<float, 3> minimum{128, 128, 128}, maximum{-128, -128, -128};
        for (unsigned vertex = 0; vertex < mesh.vertices; ++vertex) {
            const auto *v = bytes.data() + at + vertex * 16;
            for (unsigned axis = 0; axis < 3; ++axis) {
                const auto value = std::int16_t(unsigned(v[axis * 2]) << 8 | v[axis * 2 + 1]);
                if (value < -128 || value > 128)
                    return fail();
                minimum[axis] = std::min(minimum[axis], float(value));
                maximum[axis] = std::max(maximum[axis], float(value));
            }
            if (v[6] || v[7])
                return fail();
        }
        for (unsigned axis = 0; axis < 3; ++axis) {
            mesh.center[axis] = (minimum[axis] + maximum[axis]) * .5f;
            mesh.half_extent = std::max(mesh.half_extent, (maximum[axis] - minimum[axis]) * .5f);
        }
        if (mesh.half_extent <= 0)
            return fail();
        at += mesh.vertices * 16;
        mesh.triangle_offset = unsigned(at);
        for (unsigned triangle = 0; triangle < mesh.triangles; ++triangle) {
            const auto *t = bytes.data() + at + triangle * 4;
            if (t[0] >= mesh.vertices || t[1] >= mesh.vertices || t[2] >= mesh.vertices || t[3])
                return fail();
        }
        at += mesh.triangles * 4;
        while (at & 7) {
            if (at >= bytes.size() || bytes[at++])
                return fail();
        }
    }
    if (at != bytes.size())
        return fail();
    output = result;
    error.clear();
    return true;
}
} // namespace rr64::mk64_items
