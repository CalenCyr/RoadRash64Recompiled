#include "rr64_course_sky.hpp"
#include "rr64_actor_pose.hpp"
#include "librecomp/addresses.hpp"
#include "librecomp/game.hpp"
#include <algorithm>
#include <bit>
#include <cmath>
#include <cstring>

namespace rr64::course_sky {
namespace {
using namespace rr64::engine;
constexpr unsigned command_bytes = 16384, vertex_bytes = 16384, texture_bytes = 8192;
constexpr unsigned view_bytes = command_bytes + vertex_bytes + texture_bytes + 128;
constexpr unsigned allocation_bytes = 4 * view_bytes + 16;
constexpr unsigned guard = 0x534b5938;
constexpr float pi = 3.14159265358979323846f, radius = 4096.f;
struct Frame {
    unsigned base = 0;
    std::array<unsigned, 4> epochs{};
    std::array<bool, 4> issued{};
};
std::array<Frame, 2> frames{};
unsigned char *mapping = nullptr;
void word(unsigned char *rdram, unsigned address, unsigned value) {
    MEM_W(0, guest_address(address)) = value;
}
unsigned word(unsigned char *rdram, unsigned address) {
    return unsigned(MEM_W(0, guest_address(address)));
}
struct Writer {
    unsigned char *m;
    unsigned p, end;
    bool good = true;
    void cmd(unsigned a, unsigned b) {
        if (p > end || end - p < 8) { good = false; return; }
        word(m, p, a); word(m, p + 4, b); p += 8;
    }
};
struct Context {
    unsigned view = 0, slot = 0, gfx = 0, epoch = 0, pointer = 0;
    Matrix4x4Snapshot projection{}, view_matrix{};
    std::array<float, 3> camera{}, sector{};
};
bool context(unsigned char *m, Context &c) {
    unsigned views = 0, width = 0, base = 0, active = 0, count = 0;
    if (!m || !read_u32(m, globals::active_viewport, c.view) || c.view >= 4 ||
        !read_u32(m, 0x8009DB88, views) || views < 1 || views > 4 || c.view >= views ||
        !read_u32(m, globals::terrain_map_width, width) || width != 70 ||
        !read_u32(m, globals::actor_render_buffer_slot, c.slot) || c.slot > 1 ||
        !read_u32(m, 0x8009CBA4, c.gfx) || c.gfx > 1 ||
        !read_u32(m, 0x800A1830, c.epoch) || !read_u32(m, 0x800AC650, c.pointer) ||
        !read_u32(m, 0x800AC658 + c.gfx * 4, base) ||
        !read_u32(m, 0x8009CB90, active) || base != active ||
        !read_u32(m, 0x800BC9A0, count) || (count != 0x4650 && count != 0x36b0))
        return false;
    const unsigned bytes = 0x140 + count * 8;
    if (!valid_guest_range(base, bytes) || c.pointer < base + 0x148 ||
        c.pointer > base + bytes - 1064 || (c.pointer & 7)) return false;
    if (!decode_n64_matrix(m, 0x800B6668 + c.view * 0x180 + c.slot * 64, c.projection) ||
        !decode_n64_matrix(m, 0x800B6EE8 + c.view * 0x180 + c.slot * 64, c.view_matrix))
        return false;
    for (auto a : {c.projection.values, c.view_matrix.values})
        for (float v : a) if (!std::isfinite(v)) return false;
    for (unsigned i = 0; i < 3; ++i)
        if (!read_float(m, 0x800D69F8 + i * 4, c.camera[i]) || !std::isfinite(c.camera[i]) ||
            !read_float(m, 0x800A4FDC + i * 4, c.sector[i]) || !std::isfinite(c.sector[i]))
            return false;
    return c.projection.values[0] != 0 && c.projection.values[5] != 0 &&
           c.projection.values[11] != 0 && c.projection.values[14] != 0 &&
           c.view_matrix.values[15] == 1;
}
bool allocate(unsigned char *m, Frame &f) {
    if (mapping && mapping != m) return false;
    if (!f.base) {
        auto *host = static_cast<unsigned char *>(recomp::alloc(m, allocation_bytes));
        if (!host) return false;
        const auto offset = host - m;
        if (offset < 0x800000 || (offset & 7) ||
            std::uint64_t(offset) + allocation_bytes > recomp::mem_size) {
            recomp::free(m, host); return false;
        }
        f.base = 0x80000000u + unsigned(offset); mapping = m;
        for (unsigned n = 0; n < 4; ++n)
            word(m, f.base + allocation_bytes - 16 + n * 4, guard);
    }
    for (unsigned n = 0; n < 4; ++n)
        if (word(m, f.base + allocation_bytes - 16 + n * 4) != guard) return false;
    return true;
}
bool valid(const Definition &d) {
    if (!d.enabled || d.identity < 1 || d.identity > 16 || d.textures.size() > 4 ||
        d.sprites.size() > 64 || !std::isfinite(d.radius) || d.radius <= 0 || d.radius > 100000)
        return false;
    for (float v : d.center) if (!std::isfinite(v) || std::abs(v) > 100000) return false;
    for (const auto &t : d.textures)
        if (!((t.width == 16 && t.height == 16) || (t.width == 64 && t.height == 32)) ||
            t.intensity4.size() != t.width * t.height / 2) return false;
    for (const auto &s : d.sprites)
        if (s.yaw > 65535 || s.texture >= d.textures.size() || !std::isfinite(s.elevation) ||
            std::abs(s.elevation) > 120 || !std::isfinite(s.size) || s.size <= 0 || s.size > 3)
            return false;
    return true;
}
using Position = std::array<float, 3>;
using Color = std::array<std::uint8_t, 3>;
Position spherical(float yaw, float latitude) {
    return {radius * std::cos(latitude) * std::sin(yaw),
            -radius * std::cos(latitude) * std::cos(yaw), radius * std::sin(latitude)};
}
void vertex(unsigned char *rdram, unsigned address, Position p, Color color,
            int s = 0, int t = 0, unsigned alpha = 255) {
    for (unsigned i = 0; i < 3; ++i)
        MEM_H(0, guest_address(address + i * 2)) = std::int16_t(std::lround(p[i]));
    MEM_H(0, guest_address(address + 6)) = 0;
    MEM_H(0, guest_address(address + 8)) = s;
    MEM_H(0, guest_address(address + 10)) = t;
    word(rdram, address + 12, unsigned(color[0]) << 24 | unsigned(color[1]) << 16 |
         unsigned(color[2]) << 8 | alpha);
}
}

void reset() noexcept {
    // The guest allocator is reset by its owner between sessions. As in the
    // other course renderers, never free pointers from a retired RDRAM mapping.
    frames = {}; mapping = nullptr;
}
bool draw(unsigned char *m, const Definition &d) {
    if (!valid(d)) return false;
    Context c;
    if (!context(m, c)) return false;
    auto &f = frames[c.gfx];
    if (f.issued[c.view] && f.epochs[c.view] == c.epoch) return false;
    if (!allocate(m, f)) return false;
    const unsigned commands = f.base + c.view * view_bytes;
    const unsigned vertices = commands + command_bytes;
    const unsigned textures = vertices + vertex_bytes;
    const unsigned matrices = textures + texture_bytes;
    Writer w{m, commands, vertices};
    unsigned next_vertex = vertices;
    // Existing viewport and scissor are deliberately untouched. This is a
    // world-background pass, not an extra HUD or full-window rectangle.
    for (unsigned op : {0x19u, 0x1bu, 0x29u, 0x1du, 0x1fu, 0x21u, 0x23u, 0x25u, 0x27u})
        w.cmd(0x64000000u | op, 0);
    auto p = c.projection.values;
    // Infinite far plane retains native near/FOV and a nonsingular projection.
    // The shell encloses the entire map and remains fixed as the camera moves.
    // No depth test or writes: road, scenery and riders always draw over this.
    p[10] = p[11];
    const float scale = d.radius * 10.f / radius;
    std::array<float, 16> root{scale,0,0,0, 0,scale,0,0, 0,0,scale,0, 0,0,0,1}, v{};
    for (unsigned i = 0; i < 3; ++i)
        root[12 + i] = (d.center[i] - c.camera[i] - c.sector[i]) * 10.f;
    for (unsigned row = 0; row < 4; ++row)
        for (unsigned column = 0; column < 4; ++column)
            for (unsigned k = 0; k < 4; ++k)
                v[row * 4 + column] += root[row * 4 + k] * c.view_matrix.values[k * 4 + column];
    for (unsigned i = 0; i < 16; ++i) {
        word(m, matrices + i * 4, std::bit_cast<unsigned>(p[i]));
        word(m, matrices + 64 + i * 4, std::bit_cast<unsigned>(v[i]));
    }
    w.cmd(0x6400000cu, 0); w.cmd(3, 0); // scoped projection group, no interpolation
    w.cmd(0x6400000cu, 0x53590000u | d.identity * 16u | c.view);
    w.cmd(0x2du, 0); // scoped decomposed modelview: interpolate position/rotation
    w.cmd(0x64000030u, 7); w.cmd(0, matrices);      // float projection LOAD
    w.cmd(0x64000030u, 2); w.cmd(0, matrices + 64); // F3DEX2 inverted PUSH bit
    w.cmd(0xe7000000u, 0);
    w.cmd(0xd9000000u, 0x00200004u); // shade+smooth, no lighting/fog/cull/Z
    w.cmd(0xe300001fu, 0x00082c00u);
    w.cmd(0xe200001fu, 0x00552048u); // opaque, no Z compare/update
    w.cmd(0xd7000000u, 0x00010001u);
    w.cmd(0xfcffffffu, 0xfffe793cu); // shade
    auto quad = [&](const std::array<Position, 4> &points, Color low, Color high,
                    unsigned width = 0, unsigned height = 0) {
        if (next_vertex > textures - 64) { w.good = false; return; }
        for (unsigned i = 0; i < 4; ++i)
            vertex(m, next_vertex + i * 16, points[i], i < 2 ? low : high,
                   (i == 1 || i == 2) ? int(width * 32) : 0,
                   i < 2 ? int(height * 32) : 0);
        w.cmd(0x01004008u, next_vertex);
        w.cmd(0x06000204u, 0x00000406u);
        next_vertex += 64;
    };
    // A closed surrounding shell (no joins between unrelated source textures).
    // Separate horizon rings preserve each donor's above/below-horizon colors.
    for (unsigned band = 0; band < 2; ++band) {
        const float bottom = band ? 0.f : -pi / 2;
        const float top = band ? pi / 2 : 0.f;
        const Color low = band ? d.colors[1] : d.colors[3];
        const Color high = band ? d.colors[0] : d.colors[2];
        for (unsigned i = 0; i < 32; ++i) {
            const float a = i * (2 * pi / 32), b = (i + 1) * (2 * pi / 32);
            quad({spherical(a, bottom), spherical(b, bottom),
                  spherical(b, top), spherical(a, top)}, low, high);
        }
    }
    std::array<unsigned, 4> texture_addresses{};
    unsigned next_texture = textures;
    for (unsigned i = 0; i < d.textures.size(); ++i) {
        texture_addresses[i] = next_texture;
        for (std::uint8_t byte : d.textures[i].intensity4)
            m[((next_texture++ & 0x1fffffffu) ^ 3u)] = byte;
    }
    w.cmd(0xe7000000u, 0);
    w.cmd(0xe200001fu, 0x00504240u); // G_RM_XLU_SURF|G_RM_XLU_SURF2, no Z
    w.cmd(0xfc121824u, 0xff33ffffu); // G_CC_MODULATEIA in both cycles
    w.cmd(0xd7000002u, 0xffffffffu);
    for (const auto &s : d.sprites) {
        const auto &t = d.textures[s.texture];
        const unsigned line = (t.width / 2 + 7) / 8;
        w.cmd(0xe7000000u, 0);
        w.cmd(0xfd900000u, texture_addresses[s.texture]); // I4 via16-bit load
        w.cmd(0xf5900000u, 0x07080200u);
        w.cmd(0xe6000000u, 0);
        w.cmd(0xf3000000u, 0x07000000u | ((t.width * t.height / 4 - 1) << 12) |
              ((2048 + t.width / 16 - 1) / (t.width / 16)));
        w.cmd(0xe7000000u, 0);
        w.cmd(0xf5800000u | (line << 9), 0x00080200u);
        w.cmd(0xf2000000u, ((t.width - 1) << 14) | ((t.height - 1) << 2));
        const float yaw = s.yaw * (2 * pi / 65536.f);
        // MK64 authors yaw and vertical screen offsets. Lift those onto a
        // surrounding cylindrical shell using the donor60-degree lens sizing.
        const float half_w = radius * (t.width * s.size * .5f / 207.8461f);
        const float half_h = radius * (t.height * s.size * .5f / 207.8461f);
        const Position center{radius * std::sin(yaw), -radius * std::cos(yaw),
                              radius * s.elevation / 207.8461f};
        std::array<Position, 4> points{};
        for (unsigned i = 0; i < 4; ++i) {
            const float side = (i == 1 || i == 2) ? half_w : -half_w;
            points[i] = {center[0] + std::cos(yaw) * side,
                         center[1] + std::sin(yaw) * side,
                         center[2] + (i < 2 ? -half_h : half_h)};
        }
        quad(points, {255, 255, 255}, {255, 255, 255}, t.width - 1, t.height - 1);
    }
    w.cmd(0xe7000000u, 0);
    w.cmd(0xd8380002u, 64); // balance modelview push
    w.cmd(0x6400000du, 1); w.cmd(0x6400000du, 0x101); // restore both matrix groups
    w.cmd(0xd7000002u, 0x80008000u);
    for (unsigned op : {0x28u, 0x26u, 0x24u, 0x22u, 0x20u, 0x1eu, 0x2au, 0x1cu, 0x1au})
        w.cmd(0x64000000u | op, 0);
    w.cmd(0x6400002cu, 0);
    w.cmd(0xe0525464u, 0x20000000u);
    w.cmd(0xdf000000u, 0);
    if (!w.good) return false;
    Writer bridge{m, c.pointer, c.pointer + 40};
    bridge.cmd(0xe7000000u, 0);
    bridge.cmd(0xe0525464u, 0x10000064u);
    bridge.cmd(0x6400002cu, 1);
    bridge.cmd(0xde000000u, commands);
    bridge.cmd(0xe7000000u, 0);
    word(m, 0x800AC650, bridge.p);
    // Texture/tile state has no stack. The next original material must reload.
    write_u32(m, 0x8009DB2C, 0);
    write_u32(m, 0x800B1A20, 0xffffffffu);
    write_u32(m, 0x8009DBEC, 0xffffffffu);
    f.issued[c.view] = true; f.epochs[c.view] = c.epoch;
    return true;
}
}
extern "C" void rr64_course_sky_draw(unsigned char *m) {
    if (const auto *sky = rr64::course_sky::current()) rr64::course_sky::draw(m, *sky);
}
