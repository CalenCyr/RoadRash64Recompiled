#include "rr64_course_item_render.hpp"
#include "rr64_actor_pose.hpp"
#include "rr64_world_frustum.hpp"
#include "librecomp/addresses.hpp"
#include "librecomp/game.hpp"
#include <algorithm>
#include <bit>
#include <cmath>
#include <cstring>
#include <mutex>

namespace rr64::course_items {
namespace {
using namespace rr64::engine;
using Matrix = std::array<float, 16>;
constexpr unsigned asset_bytes = 4608, asset_region = 8192;
constexpr unsigned command_capacity = 65536, matrix_capacity = 32768;
constexpr unsigned view_bytes = command_capacity + matrix_capacity;
constexpr unsigned allocation_bytes = asset_region + 4u * view_bytes + 16u;
constexpr unsigned guard = 0x49b0cafeu;
static_assert(maximum_render_boxes * 6u * 64u <= matrix_capacity);
// Every fragment includes12 commands; question+shell is smaller. Leave state
// scope overhead inside an independently checked writer, never a native scratch.
static_assert(maximum_render_boxes * 6u * 128u + 512u <= command_capacity);
struct Frame {
    unsigned base = 0;
    std::array<unsigned, 4> epoch{};
    std::array<bool, 4> issued{};
};
struct Cache {
    std::array<std::uint8_t, asset_bytes> asset{};
    bool installed = false;
    unsigned char *mapping = nullptr;
    std::array<Frame, 2> frames{};
    ItemRenderStatistics stats{};
    std::mutex mutex;
};
Cache &cache() {
    static Cache c;
    return c;
}
void word(unsigned char *rdram, unsigned address, unsigned value) {
    MEM_W(0, guest_address(address)) = value;
}
unsigned getword(unsigned char *rdram, unsigned address) {
    return unsigned(MEM_W(0, guest_address(address)));
}
struct Writer {
    unsigned char *memory;
    unsigned p, end;
    bool good = true;
    void command(unsigned a, unsigned b) {
        if (!good || end - p < 8u) {
            good = false;
            return;
        }
        word(memory, p, a);
        word(memory, p + 4u, b);
        p += 8u;
    }
};
bool ensure_frame(Cache &c, unsigned char *m, unsigned gfx) {
    if (c.mapping && c.mapping != m)
        return false; // explicit session reset is required on mapping changes.
    auto &f = c.frames[gfx];
    if (f.base) {
        for (unsigned n = 0; n < 4; ++n)
            if (getword(m, f.base + allocation_bytes - 16u + n * 4u) != guard)
                return false;
        return true;
    }
    auto *host = static_cast<unsigned char *>(recomp::alloc(m, allocation_bytes));
    if (!host)
        return false;
    const auto offset = host - m;
    if (offset < 0x800000 || (offset & 7) ||
        std::uint64_t(offset) + allocation_bytes > recomp::mem_size) {
        recomp::free(m, host);
        return false;
    }
    f.base = 0x80000000u + unsigned(offset);
    c.mapping = m;
    for (unsigned i = 0; i < asset_bytes; ++i)
        m[(unsigned(offset) + i) ^ 3u] = c.asset[i];
    for (unsigned n = 0; n < 4; ++n)
        word(m, f.base + allocation_bytes - 16u + n * 4u, guard);
    return true;
}
struct Context {
    unsigned slot = 0, gfx = 0, epoch = 0, pointer = 0, view = 0;
    std::uint16_t normalize = 0;
    std::array<float, 3> camera{}, sector{};
    Matrix projection{}, modelview{};
};
bool context(unsigned char *m, Context &c) {
    unsigned base = 0, active = 0, count = 0, views = 0, scale = 0, width = 0;
    if (!read_u32(m, globals::active_viewport, c.view) || c.view >= 4u ||
        !read_u32(m, 0x8009DB88u, views) || views < 1u || views > 4u || c.view >= views ||
        !read_u32(m, globals::terrain_map_width, width) || width != 70u ||
        !read_u32(m, globals::actor_render_buffer_slot, c.slot) || c.slot > 1u ||
        !read_u32(m, 0x8009CBA4u, c.gfx) || c.gfx > 1u || !read_u32(m, 0x800A1830u, c.epoch) ||
        !read_u32(m, 0x800AC650u, c.pointer) || !read_u32(m, 0x800AC658u + c.gfx * 4u, base) ||
        !read_u32(m, 0x8009CB90u, active) || base != active || !read_u32(m, 0x800BC9A0u, count) ||
        (count != 0x4650u && count != 0x36b0u) || !read_u32(m, 0x8009DBB4u, scale) ||
        scale != std::bit_cast<unsigned>(10.f) ||
        !read_u16(m, 0x800B73F0u + c.view * 12u + c.slot * 2u, c.normalize) || !c.normalize)
        return false;
    const unsigned size = 0x140u + count * 8u;
    if (!valid_guest_range(base, size) || c.pointer < base + 0x148u ||
        c.pointer > base + size - 1064u || (c.pointer & 7u))
        return false;
    for (unsigned i = 0; i < 3; ++i)
        if (!read_float(m, 0x800D69F8u + i * 4u, c.camera[i]) || !std::isfinite(c.camera[i]) ||
            !read_float(m, 0x800A4FDCu + i * 4u, c.sector[i]) || !std::isfinite(c.sector[i]))
            return false;
    Matrix4x4Snapshot p{}, v{};
    if (!decode_n64_matrix(m, 0x800B6668u + c.view * 0x180u + c.slot * 64u, p) ||
        !decode_n64_matrix(m, 0x800B6EE8u + c.view * 0x180u + c.slot * 64u, v))
        return false;
    c.projection = p.values;
    c.modelview = v.values;
    return p.values[0] != 0 && p.values[5] != 0 && p.values[11] != 0 && v.values[15] == 1;
}
// Original mtxf_rotate_zxy_translate basis, followed by MK(x,y,z)->RR(x,-z,y).
// Native source2 model units are10 times rider-world. Source scale belongs to
// this pack; positions are already world-scaled and must not be scaled twice.
Matrix root_matrix(const ItemBoxDrawState &box, const Context &c,
                   const std::array<std::uint16_t, 3> &angle, float shrink,
                   std::array<float, 3> offset = {}) {
    constexpr float radians = 6.2831853071795864769f / 65536.f;
    const float x = (angle[0] & 0xfff0u) * radians, y = (angle[1] & 0xfff0u) * radians,
                z = (angle[2] & 0xfff0u) * radians;
    const float sx = std::sin(x), cx = std::cos(x), sy = std::sin(y), cy = std::cos(y),
                sz = std::sin(z), cz = std::cos(z);
    Matrix m{cy * cz + sx * sy * sz,
             cx * sz,
             -sy * cz + sx * cy * sz,
             0,
             -cy * sz + sx * sy * cz,
             cx * cz,
             sy * sz + sx * cy * cz,
             0,
             cx * sy,
             -sx,
             cx * cy,
             0,
             0,
             0,
             0,
             1};
    const float model_scale = 10.f * box.source_to_world_scale * shrink;
    for (unsigned r = 0; r < 3; ++r) {
        const auto old_y = m[r * 4 + 1];
        m[r * 4] *= model_scale;
        m[r * 4 + 1] = -m[r * 4 + 2] * model_scale;
        m[r * 4 + 2] = old_y * model_scale;
    }
    const std::array<float, 3> delta{offset[0] * box.source_to_world_scale,
                                     -offset[2] * box.source_to_world_scale,
                                     offset[1] * box.source_to_world_scale};
    for (unsigned i = 0; i < 3; ++i)
        m[12 + i] = ((box.position[i] + delta[i]) - c.camera[i] - c.sector[i]) * 10.f;
    return m;
}
void vertex(Writer &w, unsigned base, unsigned first, unsigned count) {
    w.command(0x01000000u | (count << 12u) | (count << 1u), base + first * 16u);
}
void triangle(Writer &w, unsigned a, unsigned b, unsigned c) {
    w.command(0x05000000u | (a << 17u) | (b << 9u) | (c << 1u), 0);
}
void state(Writer &w, bool cull, unsigned render) {
    w.command(0xe7000000u, 0);
    w.command(0xd9000000u, 0x00200005u | (cull ? 0x400u : 0u));
    w.command(0xe300001fu, 0x00082c00u); //1cycle,bilerp,perspective,noTLUT.
    w.command(0xe200001fu, render);      //also explicit no alpha-compare/pixelZ.
}
bool opaque_angle(std::uint16_t y) {
    // Source signed s16 comparisons are intentional, including unreachable
    // 280-degree window; do not silently change its original opacity cadence.
    const auto v = std::int16_t(y);
    return (v > 0 && v < 2721) || (v > 150 * 182 && v < 165 * 182) ||
           (v > 80 * 182 && v < 95 * 182);
}
}

bool install_render_asset(std::span<const std::uint8_t> asset, std::string &error) {
    auto &c = cache();
    std::lock_guard lock(c.mutex);
    if (c.mapping) {
        error = "Item asset installation requires session reset";
        return false;
    }
    if (asset.size() != 4616u || std::memcmp(asset.data(), "MKIBOX01", 8) != 0) {
        error = "Invalid original item-box asset header or size";
        return false;
    }
    // Reject malformed geometry even if a caller omitted catalogue validation.
    for (unsigned i = 0; i < 32; ++i) {
        const auto *v = asset.data() + 8 + i * 16u;
        for (unsigned j = 0; j < 3; ++j) {
            const auto p = std::int16_t((unsigned(v[j * 2]) << 8) | v[j * 2 + 1]);
            if (p < -7 || p > 7) {
                error = "Item-box vertex outside original bounds";
                return false;
            }
        }
        if (v[6] || v[7]) {
            error = "Unsupported item vertex flags";
            return false;
        }
    }
    std::copy(asset.begin() + 8, asset.end(), c.asset.begin());
    c.installed = true;
    error.clear();
    return true;
}
void clear_render_asset() {
    auto &c = cache();
    std::lock_guard lock(c.mutex);
    // Cannot overwrite vertices/texture consumed by already queued frames.
    if (!c.mapping) {
        c.installed = false;
        c.asset = {};
    }
}
void reset_render_session() {
    auto &c = cache();
    std::lock_guard lock(c.mutex);
    c.mapping = nullptr;
    c.frames = {};
    c.stats = {};
}
ItemRenderStatistics render_statistics() {
    auto &c = cache();
    std::lock_guard lock(c.mutex);
    return c.stats;
}
bool draw_boxes(unsigned char *m, std::span<const ItemBoxDrawState> boxes) {
    auto &c = cache();
    std::lock_guard lock(c.mutex);
    if (!c.installed || boxes.empty())
        return false;
    const auto refuse = [&] {
        ++c.stats.refusals;
        return false;
    };
    if (!m || boxes.size() > maximum_render_boxes)
        return refuse();
    // Fully validate before guest allocation or emission. No partial publication.
    std::array<bool, maximum_render_boxes> ids{};
    for (const auto &b : boxes) {
        if (b.id >= maximum_render_boxes || ids[b.id] ||
            !std::isfinite(b.source_to_world_scale) || b.source_to_world_scale <= 0 ||
            b.source_to_world_scale > 1 ||
            (b.state != 2 && b.state != 3 && b.state != 5) || !std::isfinite(b.break_age) ||
            b.break_age < 0 || b.break_age > 20 ||
            (!std::isnan(b.ground_height) &&
             (!std::isfinite(b.ground_height) || std::abs(b.ground_height) > 100000.f)))
            return refuse();
        ids[b.id] = true;
        for (float p : b.position)
            if (!std::isfinite(p) || std::abs(p) > 100000.f)
                return refuse();
    }
    Context ctx{};
    if (!context(m, ctx))
        return refuse();
    auto &frame = c.frames[ctx.gfx];
    if (frame.issued[ctx.view] && frame.epoch[ctx.view] == ctx.epoch)
        return refuse();
    if (!ensure_frame(c, m, ctx.gfx))
        return refuse();
    c.stats.boxes = c.stats.triangles = c.stats.command_bytes = c.stats.matrix_bytes = 0;
    const unsigned commands = frame.base + asset_region + ctx.view * view_bytes;
    const unsigned matrices = commands + command_capacity;
    Writer w{m, commands, matrices};
    unsigned matrix_count = 0, triangles = 0, visible = 0;
    rr64::world::WorldFrustum frustum(ctx.modelview, ctx.projection);
    for (unsigned op : {0x19u, 0x1bu, 0x29u, 0x1du, 0x1fu, 0x21u, 0x23u, 0x25u, 0x27u})
        w.command(0x64000000u | op, 0);
    w.command(0xdb0e0000u, ctx.normalize);
    w.command(0xda380007u, 0x800B6668u + ctx.view * 0x180u + ctx.slot * 64u);
    w.command(0xda380005u, 0x800B6EE8u + ctx.view * 0x180u + ctx.slot * 64u);
    const auto begin_part = [&](const Matrix &matrix, unsigned id, unsigned part) {
        if ((matrix_count + 1u) * 64u > matrix_capacity) {
            w.good = false;
            return;
        }
        const unsigned address = matrices + matrix_count++ * 64u;
        for (unsigned i = 0; i < 16; ++i)
            word(m, address + i * 4u, std::bit_cast<unsigned>(matrix[i]));
        w.command(0x6400000cu, 0x52550000u + ctx.view * 0x1000u + id * 16u + part);
        w.command(0x02011555u, 0);
        w.command(0x64000030u, 2);
        w.command(0, address);
    };
    const auto end_part = [&] {
        w.command(0xd8380002u, 64);
        w.command(0x6400000du, 1);
    };
    for (const auto &b : boxes) {
        const auto root = root_matrix(b, ctx, b.rotation, 1);
        // Break offsets reach46MK units; include them in the conservative cull.
        const float extent = b.state == 3 ? 60.f : 8.f;
        if (!frustum.intersects({-extent, -extent, -extent}, {extent, extent, extent}, root))
            continue;
        ++visible;
        if (b.state == 3) {
            const float t = b.break_age, shrink = t < 10 ? 1.f : 1.f - (t - 10) * .1f;
            const std::array<std::array<float, 3>, 6> offsets{{{0, 2 * t, t},
                                                               {.8f * t, 2.3f * t, .5f * t},
                                                               {.8f * t, 1.2f * t, -.5f * t},
                                                               {0, 1.8f * t, -t},
                                                               {-.8f * t, .6f * t, -.5f * t},
                                                               {-.8f * t, 2 * t, .5f * t}}};
            constexpr std::array<unsigned, 6> first{14, 20, 11, 23, 17, 8};
            for (unsigned part = 0; part < 6; ++part) {
                begin_part(root_matrix(b, ctx, b.rotation, shrink, offsets[part]), b.id, part + 2u);
                const bool opaque = (unsigned(t) & 1u) != (part >= 3u);
                state(w, false, opaque ? 0x00552078u : 0x004045d8u);
                w.command(0xfcffffffu, 0xfffe793cu);
                w.command(0xd7000000u, 0x00010001u);
                vertex(w, frame.base, first[part], 3);
                triangle(w, 0, 1, 2);
                ++triangles;
                end_part();
            }
        } else {
            if (b.state == 2 && std::isfinite(b.ground_height)) {
                auto shadow = b;
                shadow.position[2] = b.ground_height + .1f;
                begin_part(root_matrix(shadow, ctx, {0, b.rotation[1], 0}, 1), b.id, 8);
                state(w, false, 0x00504b50u);
                w.command(0xfcffffffu, 0xfffe793cu);
                w.command(0xd7000000u, 0x00010001u);
                vertex(w, frame.base, 0, 4);
                w.command(0x06000204u, 0x00000406u);
                triangles += 2;
                end_part();
            }
            auto question_angle = b.rotation;
            if (b.state == 2)
                question_angle = {0, std::uint16_t(b.rotation[1] * 2u), 0};
            begin_part(root_matrix(b, ctx, question_angle, 1), b.id, 0);
            state(w, false, 0x00553078u);
            w.command(0xd7000002u, 0xffffffffu);
            w.command(0xfc127e24u, 0xfffff3f9u);
            w.command(0xe8000000u, 0);
            w.command(0xf5101000u, 0x00098250u);
            w.command(0xf2000000u, 0x0007c0fcu);
            w.command(0xfd100000u, frame.base + 512u);
            w.command(0xe8000000u, 0);
            w.command(0xf5100000u, 0x07000000u);
            w.command(0xe6000000u, 0);
            w.command(0xf3000000u, 0x077ff100u);
            vertex(w, frame.base, 4, 4);
            w.command(0x06000204u, 0x00000406u);
            triangles += 2;
            end_part();
            begin_part(root, b.id, 1);
            state(w, true, opaque_angle(b.rotation[1]) ? 0x00552078u : 0x00504b50u);
            w.command(0xfcffffffu, 0xfffe793cu);
            w.command(0xd7000000u, 0x00010001u);
            vertex(w, frame.base, 8, 24);
            for (unsigned first : {9u, 6u, 3u, 0u, 12u, 15u, 18u, 21u})
                triangle(w, first, first + 1u, first + 2u);
            triangles += 8;
            end_part();
        }
    }
    if (!visible)
        return false;
    w.command(0xe7000000u, 0);
    // Tile/image state has no extended stack. Restore native texture scale and
    // force the next original material to reload its complete tile state.
    w.command(0xd7000002u, 0x80008000u);
    for (unsigned op : {0x28u, 0x26u, 0x24u, 0x22u, 0x20u, 0x1eu, 0x2au, 0x1cu, 0x1au})
        w.command(0x64000000u | op, 0);
    w.command(0x6400002cu, 0);
    w.command(0xe0525464u, 0x20000000u);
    w.command(0xdf000000u, 0);
    if (!w.good)
        return refuse();
    Writer bridge{m, ctx.pointer, ctx.pointer + 40u};
    bridge.command(0xe7000000u, 0);
    bridge.command(0xe0525464u, 0x10000064u);
    bridge.command(0x6400002cu, 1);
    bridge.command(0xde000000u, commands);
    bridge.command(0xe7000000u, 0);
    word(m, 0x800AC650u, bridge.p);
    write_u32(m, 0x8009DB2Cu, 0);
    write_u32(m, 0x800B1A20u, 0xffffffffu);
    write_u32(m, 0x8009DBECu, 0xffffffffu);
    frame.issued[ctx.view] = true;
    frame.epoch[ctx.view] = ctx.epoch;
    ++c.stats.draws;
    c.stats.boxes = visible;
    c.stats.triangles = triangles;
    c.stats.command_bytes = w.p - commands;
    c.stats.matrix_bytes = matrix_count * 64u;
    return true;
}
}
