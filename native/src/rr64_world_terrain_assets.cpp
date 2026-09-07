#include "rr64_world_packet_compiler.hpp"
#include "rr64_world_terrain_assets.hpp"

#include <algorithm>
#include <bit>
#include <cmath>
#include <cstring>
#include <limits>
#include <stdexcept>
#include <utility>

#include "recomp.h"

extern "C" void func_80010FD0(std::uint8_t*, recomp_context*);

namespace rr64::world {
namespace {
constexpr std::size_t table_header = 0x18d380u;
constexpr std::size_t table = table_header + 0x18u;
constexpr std::size_t texture_count = 926u;
constexpr std::size_t cell_count = 4900u;
constexpr std::size_t maximum_blob = 64u * 1024u * 1024u;
constexpr std::size_t maximum_texture = 128u * 1024u;
constexpr std::uint32_t scratch_texture = 0x80100000u;
constexpr std::uint32_t scratch_submesh = 0x80200000u;
constexpr std::uint32_t scratch_table = 0x80300000u;
constexpr std::uint32_t scratch_output = 0x803f0000u;
constexpr std::uint32_t scratch_commands = 0x80400000u;
constexpr std::uint32_t scratch_stack = 0x807ff000u;

void require(bool condition, const char* message) {
    if (!condition) { throw std::runtime_error(message); }
}

struct Reader {
    std::span<const std::uint8_t> bytes;
    void range(std::size_t offset, std::size_t count) const {
        require(offset <= bytes.size() && count <= bytes.size() - offset,
            "terrain ROM reference exceeds its containing record");
    }
    std::uint16_t half(std::size_t p) const {
        range(p, 2); return std::uint16_t((std::uint32_t(bytes[p]) << 8) | bytes[p + 1]);
    }
    std::uint32_t word(std::size_t p) const {
        range(p, 4); return (std::uint32_t(bytes[p]) << 24) |
            (std::uint32_t(bytes[p + 1]) << 16) |
            (std::uint32_t(bytes[p + 2]) << 8) | bytes[p + 3];
    }
    float number(std::size_t p) const {
        const auto value = std::bit_cast<float>(word(p));
        require(std::isfinite(value), "nonfinite terrain root or bound"); return value;
    }
    Reader record(std::size_t p, std::size_t n) const {
        range(p, n); return {bytes.subspan(p, n)};
    }
    Reader reference(std::size_t p) const {
        range(p, 12); const auto relative = word(p); const auto size = word(p + 4);
        require(relative <= bytes.size() - p, "terrain relative reference overflows");
        require(relative != 0 && size != 0, "empty referenced terrain record");
        return record(p + relative, size);
    }
};

void append_word(std::vector<std::uint8_t>& bytes, std::uint32_t value) {
    require(bytes.size() <= maximum_blob - 4u, "terrain upload exceeds fixed budget");
    for (int shift = 24; shift >= 0; shift -= 8) {
        bytes.push_back(std::uint8_t(value >> shift));
    }
}

std::uint32_t append_bytes(TerrainAssets& out, std::span<const std::uint8_t> data) {
    const auto padded = (out.bytes.size() + 7u) & ~std::size_t(7u);
    require(padded <= maximum_blob && data.size() <= maximum_blob - padded,
        "terrain upload exceeds fixed budget");
    out.bytes.resize(padded, 0);
    const auto offset = static_cast<std::uint32_t>(padded);
    out.bytes.insert(out.bytes.end(), data.begin(), data.end()); return offset;
}

void memory_word(std::vector<std::uint8_t>& memory, std::uint32_t address, std::uint32_t value) {
    std::memcpy(memory.data() + (address & 0x7fffffu), &value, sizeof(value));
}
std::uint32_t memory_word(const std::vector<std::uint8_t>& memory, std::uint32_t address) {
    std::uint32_t value; std::memcpy(&value, memory.data() + (address & 0x7fffffu), sizeof(value)); return value;
}
void memory_bytes(std::vector<std::uint8_t>& memory, std::uint32_t address,
    std::span<const std::uint8_t> bytes) {
    for (std::size_t i = 0; i < bytes.size(); ++i) {
        memory[((address & 0x7fffffu) + i) ^ 3u] = bytes[i];
    }
}
gpr guest(std::uint32_t address) { return static_cast<gpr>(static_cast<std::int32_t>(address)); }

using Commands = std::vector<std::array<std::uint32_t, 2>>;

Commands original_material(std::vector<std::uint8_t>& memory, const Reader* texture,
    std::uint16_t index) {
    // The resident pointer makes E65C's loader branch unreachable. All global
    // cache markers and allocation bookkeeping writes address only this vector.
    if (texture) { memory_bytes(memory, scratch_texture, texture->bytes); }
    memory_word(memory, 0x800dea84u, scratch_table);
    if (texture) { memory_word(memory, scratch_table + index * 16u, scratch_texture); }
    memory_word(memory, scratch_submesh + 0x10u, index);
    memory_word(memory, scratch_output, scratch_commands);
    memory_word(memory, scratch_output + 4u, 0u);
    memory_word(memory, 0x8009db20u, 0u);
    memory_word(memory, 0x8009db2cu, 0u);
    memory_word(memory, 0x800b1a20u, 0xffffffffu);
    memory_word(memory, scratch_stack + 0x10u, scratch_output + 4u);
    std::fill(memory.begin() + (scratch_commands & 0x7fffffu),
        memory.begin() + (scratch_commands & 0x7fffffu) + 4096u, 0xa5u);
    recomp_context context{};
    context.r4 = guest(scratch_submesh); context.r5 = 0x40000000u;
    context.r7 = guest(scratch_output); context.r29 = guest(scratch_stack);
    context.f_odd = &context.f0.u32h;
    func_80010FD0(memory.data(), &context);
    const auto end = memory_word(memory, scratch_output);
    require(end >= scratch_commands && end <= scratch_commands + 1024u &&
        ((end - scratch_commands) & 7u) == 0u, "original terrain material exceeds command bound");
    require(std::all_of(memory.begin() + (scratch_commands & 0x7fffffu) + 1024u,
        memory.begin() + (scratch_commands & 0x7fffffu) + 4096u,
        [](std::uint8_t value) { return value == 0xa5u; }), "terrain material command guard changed");
    Commands result;
    for (auto p = scratch_commands; p < end; p += 8u) {
        result.push_back({memory_word(memory, p), memory_word(memory, p + 4u)});
    }
    return result;
}

std::uint32_t triangle_word(std::uint16_t packed) {
    return ((std::uint32_t(packed) << 7u) & 0x3e0000u) |
        ((std::uint32_t(packed) << 4u) & 0x3e00u) |
        ((std::uint32_t(packed) << 1u) & 0x3eu);
}

TerrainTextureAsset validate_texture(const Reader& record) {
    record.range(0, 0x40u);
    const auto signature = record.word(0);
    const auto width = record.word(0x2cu), height = record.word(0x30u);
    const auto bits = record.word(0x34u);
    const auto flags = record.half(0x24u);
    const auto frames = record.half(0x20u);
    const auto stride = record.word(0x3cu);
    require((signature == 0x16u || signature == 0x17u) && record.word(4) == record.bytes.size() &&
        record.word(8) == 0xffffffffu, "invalid terrain texture signature or declared size");
    require(record.bytes.size() <= maximum_texture && width > 0u && width <= 256u &&
        height > 0u && height <= 256u && (bits == 4u || bits == 8u) &&
        frames >= 1u && frames <= 64u && stride > 0u,
        "terrain texture dimensions or frame count exceed locked format");
    const auto texels = (width * height * bits + 7u) / 8u;
    const auto palette = (flags & 0x8000u) ? 0u : (bits == 4u ? 32u : 512u);
    // Header+22 is not a byte-capacity certificate: real texture207 is CI8
    // with value22, while its frame stores the full512-byte TLUT consumed by
    // 10640. Bound the actual fixed-size palette in each frame instead.
    require(record.half(0x26u) == texels &&
        texels + palette <= stride && 0x40ull + std::uint64_t(stride) * frames <= record.bytes.size(),
        "terrain texture pixels or palette exceed frame bounds");
    return {0u, static_cast<std::uint32_t>(record.bytes.size()), stride, frames, flags};
}

void build(const Reader& rom, TerrainAssets& out, bool batch_triangles) {
    require(rom.bytes.size() == 0x2000000u && rom.word(0) == 0x80371240u,
        "terrain builder requires the loaded big-endian 32MiB ROM");
    require(rom.word(table_header) == 0x3eu && rom.half(table_header + 0x10u) == 1000u &&
        rom.half(table_header + 0x12u) == cell_count && rom.half(table_header + 0x14u) == texture_count,
        "terrain table dimensions differ from the supported revision");
    rom.range(table, (texture_count + cell_count) * 12u);
    out.textures.reserve(texture_count); out.cells.reserve(cell_count);
    std::vector<Commands> materials;
    materials.reserve(texture_count + 1u);
    std::vector<std::uint8_t> scratch(8u * 1024u * 1024u);
    for (std::size_t i = 0; i < texture_count; ++i) {
        const auto record = rom.reference(table + i * 12u);
        auto metadata = validate_texture(record);
        metadata.raw_offset = append_bytes(out, record.bytes);
        out.textures.push_back(metadata);
        materials.push_back(original_material(scratch, &record, static_cast<std::uint16_t>(i)));
    }
    materials.push_back(original_material(scratch, nullptr, 0xffffu));
    for (std::size_t i = 0; i < cell_count; ++i) {
        const auto entry = table + (texture_count + i) * 12u;
        if (rom.word(entry) == 0u) {
            require(rom.word(entry + 4u) == 0u, "empty terrain cell has nonempty size"); continue;
        }
        const auto partition = rom.reference(entry);
        partition.range(0, 0xf0u);
        require(partition.word(0) == 0x3fu && partition.word(4) == partition.bytes.size(),
            "invalid terrain partition header");
        const auto submeshes = partition.half(0xcu);
        partition.range(0xf0u, submeshes * 12u);
        TerrainCellAsset cell;
        cell.cell_index = static_cast<std::uint32_t>(i); cell.partition_id = partition.word(8);
        for (std::size_t axis = 0; axis < 2u; ++axis) { cell.authored_origin[axis] = partition.number(0x1cu + axis * 4u); }
        for (std::size_t axis = 0; axis < 4u; ++axis) {
            cell.root_quaternion[axis] = partition.number(0x24u + axis * 4u);
            cell.original_bounds[axis] = partition.number(0x40u + axis * 4u);
        }
        const auto q = cell.root_quaternion;
        const auto norm = q[0]*q[0]+q[1]*q[1]+q[2]*q[2]+q[3]*q[3];
        require(std::abs(norm - 1.0f) < 0.001f, "terrain root quaternion is not normalized");
        cell.minimum.fill(std::numeric_limits<float>::infinity());
        cell.maximum.fill(-std::numeric_limits<float>::infinity());
        Commands commands;
        std::vector<TerrainAssetRelocation> local_relocations;
        for (std::size_t s = 0; s < submeshes; ++s) {
            const auto submesh = partition.reference(0xf0u + s * 12u);
            submesh.range(0, 0x18u);
            require(submesh.word(0) == 0x3du && submesh.word(4) == submesh.bytes.size(),
                "invalid terrain submesh header");
            const auto all_packets = submesh.half(0xcu), render_packets = submesh.half(0xeu);
            const auto material = submesh.half(0x12u);
            require(render_packets <= all_packets && (material == 0xffffu || material < texture_count),
                "terrain render count or material index is invalid");
            if (render_packets != 0u) {
                if (material != 0xffffu) {
                    local_relocations.push_back({static_cast<std::uint32_t>(commands.size() * 8u + 4u),
                        out.textures[material].raw_offset + 0x40u, material});
                    commands.push_back({0xdb06001cu, 0u});
                }
                const auto& state = materials[material == 0xffffu ? texture_count : material];
                commands.insert(commands.end(), state.begin(), state.end());
            }
            std::size_t p = 0x18u;
            for (std::size_t packet = 0; packet < all_packets; ++packet) {
                submesh.range(p, 8u);
                const auto triangles = submesh.half(p), vertices = submesh.half(p + 2u);
                const auto packet_bytes = submesh.half(p + 4u), vertex_bytes = submesh.half(p + 6u);
                require(vertices <= 32u && packet_bytes >= 8u && vertex_bytes >= vertices * 16u &&
                    8u + std::size_t(vertex_bytes) + triangles * 2u <= packet_bytes,
                    "terrain geometry packet bounds are invalid");
                submesh.range(p, packet_bytes);
                const auto triangles_begin = p + 8u + vertex_bytes;
                for (std::size_t t = 0; t < triangles; ++t) {
                    const auto packed = submesh.half(triangles_begin + t * 2u);
                    require(((packed >> 10u) & 31u) < vertices && ((packed >> 5u) & 31u) < vertices &&
                        (packed & 31u) < vertices, "terrain triangle references a missing vertex");
                }
                if (packet < render_packets) {
                    const auto vertex_offset = append_bytes(out, submesh.bytes.subspan(p + 8u, vertices * 16u));
                    local_relocations.push_back({static_cast<std::uint32_t>(commands.size() * 8u + 4u), vertex_offset});
                    commands.push_back({0x01000000u | (std::uint32_t(vertices) << 12u) | (vertices * 2u), 0u});
                    for (std::size_t v = 0; v < vertices; ++v) {
                        for (std::size_t axis = 0; axis < 3u; ++axis) {
                            const float value = std::bit_cast<std::int16_t>(submesh.half(p + 8u + v * 16u + axis * 2u));
                            cell.minimum[axis] = std::min(cell.minimum[axis], value);
                            cell.maximum[axis] = std::max(cell.maximum[axis], value);
                        }
                    }
                    // The renderer consumes only this packet's original TRI1/
                    // TRI2 commands; vertex and material state stay outside it.
                    if (batch_triangles && triangles != 0u) {
                        commands.push_back({0x64000034u, (std::uint32_t(triangles) + 1u) / 2u});
                    }
                    std::size_t t = 0;
                    if (triangles & 1u) {
                        commands.push_back({0x05000000u | triangle_word(submesh.half(triangles_begin)), 0u}); ++t;
                    }
                    for (; t < triangles; t += 2u) {
                        commands.push_back({0x06000000u | triangle_word(submesh.half(triangles_begin + t * 2u)),
                            triangle_word(submesh.half(triangles_begin + (t + 1u) * 2u))});
                    }
                    cell.vertices += vertices; cell.triangles += triangles; ++out.packets;
                }
                else { ++out.non_render_packets; }
                p += packet_bytes;
            }
        }
        if (cell.vertices == 0u) { cell.minimum.fill(0.0f); cell.maximum.fill(0.0f); }
        commands.push_back({0xdf000000u, 0u});
        cell.display_list_offset = append_bytes(out, {});
        for (const auto& command : commands) { append_word(out.bytes, command[0]); append_word(out.bytes, command[1]); }
        cell.display_list_size = static_cast<std::uint32_t>(commands.size() * 8u);
        for (auto relocation : local_relocations) {
            relocation.word_offset += cell.display_list_offset; out.relocations.push_back(relocation);
        }
        out.vertices += cell.vertices; out.triangles += cell.triangles; out.cells.push_back(cell);
    }
}
} // namespace

bool build_terrain_assets(std::span<const std::uint8_t> rom, TerrainAssets& output,
    std::string& error, bool batch_triangles, bool compile_packets) noexcept {
    try {
        TerrainAssets candidate; build({rom}, candidate, batch_triangles);
        if(compile_packets && batch_triangles){ PacketCompiler compiler; compiler.compile(candidate,candidate.cells); }
        
        output = std::move(candidate); error.clear(); return true;
    }
    catch (const std::exception& exception) { error = exception.what(); return false; }
    catch (...) { error = "unknown terrain asset construction failure"; return false; }
}
bool build_terrain_assets(std::span<const std::uint8_t> rom, TerrainAssets& output,
    std::string& error) noexcept {
    return build_terrain_assets(rom, output, error, false);
}
bool build_terrain_assets(std::span<const std::uint8_t> rom,TerrainAssets& output,std::string& error,bool batch_triangles) noexcept {
    return build_terrain_assets(rom,output,error,batch_triangles,false);
}
} // namespace rr64::world

