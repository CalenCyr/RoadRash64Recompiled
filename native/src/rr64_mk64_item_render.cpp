#include "rr64_mk64_item_render.hpp"
#include "librecomp/addresses.hpp"
#include "rr64_actor_pose.hpp"
#include "rr64_mk64_item_assets.hpp"
#include "rr64_mk64_item_dimensions.hpp"
#include "rr64_mk64_item_lightning.hpp"
#include "rr64_mk64_items.hpp"
#include "rr64_world_frustum.hpp"
#include <algorithm>
#include <bit>
#include <cmath>
#include <cstring>
#include <mutex>

namespace rr64::mk64_items {
namespace {
using namespace rr64::engine;
using Matrix = std::array<float, 16>;
constexpr unsigned asset_region = maximum_asset_bytes;
constexpr unsigned command_capacity = 32768, matrix_capacity = 10240, hud_capacity = 1024;
constexpr unsigned bolt_vertices = 40, bolt_vertex_capacity = racer_capacity * bolt_vertices * 16;
constexpr unsigned view_bytes =
    command_capacity + matrix_capacity + hud_capacity + bolt_vertex_capacity;
constexpr unsigned allocation_bytes = asset_region + 4u * view_bytes + 16u;
constexpr unsigned guard = 0x49b0cafeu;
static_assert((object_capacity * 2u + racer_capacity) * 64u <= matrix_capacity);
struct Frame {
    unsigned base = 0;
    std::array<unsigned, 4> epoch{};
    std::array<bool, 4> issued{}, hud_issued{};
    std::array<unsigned, 4> hud_epoch{};
};
struct Cache {
    std::array<std::uint8_t, maximum_asset_bytes> asset{};
    unsigned asset_bytes = 0;
    AssetLayout layout{};
    bool installed = false;
    unsigned char *mapping = nullptr;
    std::array<Frame, 2> frames{};
    RenderStatistics stats{};
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
    for (unsigned i = 0; i < c.asset_bytes; ++i)
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
struct RenderPose {
    Vec position{};
    float source_to_world_scale = .05f;
};
// Original mtxf_rotate_zxy_translate basis, followed by MK(x,y,z)->RR(x,-z,y).
// Native source2 model units are10 times rider-world. Source scale belongs to
// this pack; positions are already world-scaled and must not be scaled twice.
Matrix root_matrix(const RenderPose &box, const Context &c,
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
    w.command(0xe300001fu, 0x00082c00u); // 1cycle,bilerp,perspective,noTLUT.
    w.command(0xe200001fu, render);      // also explicit no alpha-compare/pixelZ.
}
void texture(Writer &w, unsigned address, const TextureAsset &t, bool hud = false) {
    w.command(0xd7000002u, 0xffffffffu);
    w.command(0xfc127e24u, 0xfffff3f9u);
    w.command(0xe8000000u, 0);
    const unsigned line = (t.width * 2u + 7u) / 8u;
    w.command(0xf5100000u | (line << 9u), 0x00080200u); // clamp S/T, no masks.
    w.command(0xf2000000u, ((t.width - 1u) * 4u << 12u) | ((t.height - 1u) * 4u));
    w.command(0xfd100000u, address);
    w.command(0xe8000000u, 0);
    w.command(0xf5100000u, 0x07000000u);
    w.command(0xe6000000u, 0);
    const unsigned dxt = (2048u + line - 1u) / line;
    w.command(0xf3000000u, 0x07000000u | ((t.width * t.height - 1u) << 12u) | dxt);
    if (hud) {
        // Texture-only RGBA; no inherited world shade or fog in the HUD.
        w.command(0xfcffffffu, 0xfffcf279u);
    }
}
void push_state(Writer &w) {
    for (unsigned op : {0x19u, 0x1bu, 0x29u, 0x1du, 0x1fu, 0x21u, 0x23u, 0x25u, 0x27u})
        w.command(0x64000000u | op, 0);
}
void finish(Writer &w) {
    w.command(0xe7000000u, 0);
    w.command(0xd7000002u, 0x80008000u);
    for (unsigned op : {0x28u, 0x26u, 0x24u, 0x22u, 0x20u, 0x1eu, 0x2au, 0x1cu, 0x1au})
        w.command(0x64000000u | op, 0);
    w.command(0x6400002cu, 0);
    w.command(0xe0525464u, 0x20000000u);
    w.command(0xdf000000u, 0);
}
void publish(unsigned char *m, unsigned pointer, unsigned commands) {
    Writer bridge{m, pointer, pointer + 40u};
    bridge.command(0xe7000000u, 0);
    bridge.command(0xe0525464u, 0x10000064u);
    bridge.command(0x6400002cu, 1);
    bridge.command(0xde000000u, commands);
    bridge.command(0xe7000000u, 0);
    word(m, 0x800AC650u, bridge.p);
    write_u32(m, 0x8009DB2Cu, 0);
    write_u32(m, 0x800B1A20u, 0xffffffffu);
    write_u32(m, 0x8009DBECu, 0xffffffffu);
}
} // namespace

bool install_render_asset(std::span<const std::uint8_t> asset, std::string &error) {
    AssetLayout parsed{};
    if (!parse_item_assets(asset, parsed, error))
        return false;
    auto &c = cache();
    std::lock_guard lock(c.mutex);
    if (c.mapping) {
        error = "MK64 item installation requires session reset";
        return false;
    }
    std::copy(asset.begin(), asset.end(), c.asset.begin());
    c.asset_bytes = unsigned(asset.size());
    c.layout = parsed;
    c.installed = true;
    error.clear();
    return true;
}
bool render_asset_available() noexcept {
    auto &c = cache();
    std::lock_guard lock(c.mutex);
    return c.installed;
}
void clear_render_asset() {
    auto &c = cache();
    std::lock_guard lock(c.mutex);
    if (!c.mapping) {
        c.installed = false;
        c.asset_bytes = 0;
        c.layout = {};
    }
}
void reset_render_session() {
    auto &c = cache();
    std::lock_guard lock(c.mutex);
    c.mapping = nullptr;
    c.frames = {};
    c.stats = {};
}
RenderStatistics render_statistics() {
    auto &c = cache();
    std::lock_guard lock(c.mutex);
    return c.stats;
}
bool draw_world(unsigned char *m, const Snapshot &snapshot) {
    auto &c = cache();
    std::lock_guard lock(c.mutex);
    if (!c.installed || !snapshot.enabled)
        return false;
    const auto refuse = [&] {
        ++c.stats.refusals;
        return false;
    };
    if (!m || !valid(snapshot))
        return refuse();
    Context ctx{};
    if (!context(m, ctx))
        return refuse();
    auto &frame = c.frames[ctx.gfx];
    if (frame.issued[ctx.view] && frame.epoch[ctx.view] == ctx.epoch)
        return refuse();
    if (!ensure_frame(c, m, ctx.gfx))
        return refuse();
    const unsigned commands = frame.base + asset_region + ctx.view * view_bytes;
    const unsigned matrices = commands + command_capacity;
    const unsigned bolt_data = matrices + matrix_capacity + hud_capacity;
    Writer w{m, commands, matrices};
    unsigned matrix_count = 0, triangles = 0, visible = 0, strikes = 0;
    rr64::world::WorldFrustum frustum(ctx.modelview, ctx.projection);
    push_state(w);
    w.command(0xdb0e0000u, ctx.normalize);
    w.command(0xda380007u, 0x800B6668u + ctx.view * 0x180u + ctx.slot * 64u);
    w.command(0xda380005u, 0x800B6EE8u + ctx.view * 0x180u + ctx.slot * 64u);
    const auto mesh = [&](unsigned index, unsigned texture_id, const Matrix &matrix,
                          unsigned generation, unsigned part, unsigned render = 0) {
        if ((matrix_count + 1u) * 64u > matrix_capacity) {
            w.good = false;
            return;
        }
        const unsigned address = matrices + matrix_count++ * 64u;
        for (unsigned n = 0; n < 16; ++n)
            word(m, address + n * 4u, std::bit_cast<unsigned>(matrix[n]));
        // Generation rather than pool slot prevents interpolating a retired
        // projectile into its replacement. Distinct namespaces cover views.
        w.command(0x6400000cu,
                  0x53000000u | (ctx.view << 22u) | ((generation & 0xFFFFFu) << 2u) | part);
        w.command(0x02011555u, 0);
        w.command(0x64000030u, 2);
        w.command(0, address);
        const auto &geometry = c.layout.meshes[index];
        state(w, false, render ? render : texture_id == 0xFFFFFFFFu ? 0x00552078u : 0x00553078u);
        if (texture_id != 0xFFFFFFFFu) {
            const auto &image = c.layout.textures[texture_id];
            texture(w, frame.base + image.offset, image);
        } else {
            w.command(0xfcffffffu, 0xfffe793cu);
            w.command(0xd7000000u, 0x00010001u);
        }
        vertex(w, frame.base + geometry.vertex_offset, 0, geometry.vertices);
        for (unsigned n = 0; n < geometry.triangles; ++n) {
            const auto *t = c.asset.data() + geometry.triangle_offset + n * 4u;
            triangle(w, t[0], t[1], t[2]);
        }
        triangles += geometry.triangles;
        w.command(0xd8380002u, 64);
        w.command(0x6400000du, 1);
    };
    constexpr float to_angle = 65536.f / 6.2831853071795864769f;
    for (const auto &object : snapshot.objects) {
        if (!object.generation)
            continue;
        const unsigned base_mesh = shell(object.kind) ? 0u : object.kind == Item::Banana ? 2u : 5u;
        const auto &bounds = c.layout.meshes[base_mesh];
        RenderPose pose{object.position, object_radius(object.kind) / bounds.half_extent};
        std::array<std::uint16_t, 3> angle{};
        if (shell(object.kind)) {
            const float dx = ctx.camera[0] + ctx.sector[0] - object.position[0];
            const float dy = ctx.camera[1] + ctx.sector[1] - object.position[1];
            angle[1] = std::uint16_t(int(std::atan2(dx, -dy) * to_angle));
        } else if (object.kind == Item::FakeBox) {
            const unsigned age = snapshot.clock - object.born;
            angle = {std::uint16_t(age * 182u), std::uint16_t(age * 364u),
                     std::uint16_t(age * 182u)};
        }
        const auto centered = [&](std::array<std::uint16_t, 3> rotation) {
            auto matrix = root_matrix(pose, ctx, rotation, 1);
            for (unsigned axis = 0; axis < 3; ++axis)
                for (unsigned basis = 0; basis < 3; ++basis)
                    matrix[12 + axis] -= matrix[basis * 4 + axis] * bounds.center[basis];
            return matrix;
        };
        const auto transform = centered(angle);
        std::array<float, 3> low{}, high{};
        for (unsigned axis = 0; axis < 3; ++axis) {
            low[axis] = bounds.center[axis] - bounds.half_extent;
            high[axis] = bounds.center[axis] + bounds.half_extent;
        }
        if (!frustum.intersects(low, high, transform))
            continue;
        ++visible;
        if (shell(object.kind)) {
            // Original ping-pong sequence has 15 positions and uses mirrored
            // geometry for its second half; one revolution takes 15 ticks.
            const unsigned phase = (snapshot.clock - object.born) % 15u;
            const unsigned animation = phase < 8u ? phase : 15u - phase;
            const unsigned color = object.kind == Item::GreenShell ? 0
                                   : object.kind == Item::RedShell ? 1
                                                                   : 2;
            mesh(phase < 8u ? 0 : 1, 16u + color * 8u + animation, transform, object.generation, 0);
        } else if (object.kind == Item::Banana) {
            mesh(2, 40, transform, object.generation, 0);
        } else if (object.kind == Item::FakeBox) {
            // Original fake-box question is yaw-only and is drawn before the
            // translucent color shell. Preserve its authored vertex alpha and
            // signed-angle opacity cadence so the question remains visible.
            const auto yaw = std::int16_t(angle[1]);
            const bool opaque = (yaw > 0 && yaw < 2721) || (yaw > 150 * 182 && yaw < 165 * 182) ||
                                (yaw > 80 * 182 && yaw < 95 * 182);
            angle = {0, angle[1], 0};
            mesh(4, 42, centered(angle), object.generation, 1);
            w.command(0xf9000000u, 0xFFu);
            mesh(5, 0xFFFFFFFFu, transform, object.generation, 0,
                 opaque ? 0x00552078u : 0x00504B50u);
        }
    }
    // Code-drawn, camera-facing ribbons adapt the donor's per-racer bolt to
    // Road Rash world units. No donor pixels or additional ROM bank are used.
    for (unsigned slot = 0; slot < racer_capacity; ++slot) {
        const auto lightning = lightning_visual(snapshot.riders[slot], snapshot.clock);
        Vec anchor{};
        if (!lightning.bolt_alpha || !render_rider_anchor(m, slot, anchor))
            continue;
        bool finite = true;
        for (float coordinate : anchor)
            finite &= std::isfinite(coordinate);
        if (!finite)
            continue;
        const float dx = ctx.camera[0] + ctx.sector[0] - anchor[0];
        const float dy = ctx.camera[1] + ctx.sector[1] - anchor[1];
        const std::array<std::uint16_t, 3> angle{
            0, std::uint16_t(int(std::atan2(dx, -dy) * to_angle)), 0};
        const auto transform = root_matrix({anchor, .01f}, ctx, angle, 1);
        if (!frustum.intersects({-65, 0, -1}, {65, 400, 1}, transform))
            continue;
        if ((matrix_count + 1) * 64 > matrix_capacity) {
            w.good = false;
            break;
        }
        const unsigned matrix_address = matrices + matrix_count++ * 64;
        for (unsigned n = 0; n < 16; ++n)
            word(m, matrix_address + n * 4, std::bit_cast<unsigned>(transform[n]));
        // Skip interpolation: a new strike must never morph from an old
        // projectile/episode. Its anchors still use native per-view poses.
        w.command(0x6400000c, 0x57000000u | (ctx.view << 20) | slot);
        w.command(0x00000001, 0);
        w.command(0x64000030, 2);
        w.command(0, matrix_address);
        state(w, false, 0x00504B50);       // Depth test, alpha blend, no depth write.
        w.command(0xfcffffff, 0xfffe793c); // Vertex shade RGBA.
        w.command(0xd7000000, 0x00010001);
        constexpr std::array<int, 6> x{0, 38, -25, 28, -32, 0};
        constexpr std::array<int, 6> y{400, 320, 245, 170, 95, 0};
        const unsigned vertices_address = bolt_data + slot * bolt_vertices * 16;
        for (unsigned layer = 0; layer < 2; ++layer) {
            const int width = layer ? 4 : 11;
            const unsigned color = layer ? 0xffffff00u : 0xe8dc5000u;
            const unsigned alpha = layer ? lightning.bolt_alpha : lightning.bolt_alpha * 2 / 3;
            for (unsigned segment = 0; segment < 5; ++segment) {
                for (unsigned corner = 0; corner < 4; ++corner) {
                    const unsigned endpoint = segment + (corner >= 2);
                    const int side = (corner == 0 || corner == 3) ? -width : width;
                    const unsigned address =
                        vertices_address + (layer * 20 + segment * 4 + corner) * 16;
                    word(m, address,
                         (unsigned(std::uint16_t(x[endpoint] + side)) << 16) |
                             unsigned(std::uint16_t(y[endpoint])));
                    word(m, address + 4, 0);
                    word(m, address + 8, 0);
                    word(m, address + 12, color | alpha);
                }
            }
            vertex(w, vertices_address + layer * 20 * 16, 0, 20);
            for (unsigned segment = 0; segment < 5; ++segment) {
                const unsigned first = segment * 4;
                triangle(w, first, first + 1, first + 2);
                triangle(w, first, first + 2, first + 3);
            }
            triangles += 10;
        }
        w.command(0xd8380002, 64);
        w.command(0x6400000d, 1);
        ++strikes;
    }
    if (!visible && !strikes)
        return false;
    finish(w);
    if (!w.good)
        return refuse();
    publish(m, ctx.pointer, commands);
    frame.issued[ctx.view] = true;
    frame.epoch[ctx.view] = ctx.epoch;
    ++c.stats.world_draws;
    c.stats.objects = visible;
    c.stats.lightning_strikes = strikes;
    c.stats.triangles = triangles;
    c.stats.command_bytes = w.p - commands;
    return true;
}
bool draw_hud_rectangle(unsigned char *m, unsigned view, Item shown, float x, float y, float width,
                        float height) {
    auto &c = cache();
    std::lock_guard lock(c.mutex);
    if (!c.installed)
        return false;
    const auto refuse = [&] {
        ++c.stats.refusals;
        return false;
    };
    if (!m || view >= 4 || !valid_item(shown) || !std::isfinite(x) || !std::isfinite(y) ||
        !std::isfinite(width) || !std::isfinite(height) || width < 1 || height < 1 || width > 512 ||
        height > 512 || x < 0 || y < 0 || x + width > 1023 || y + height > 1023)
        return refuse();
    unsigned gfx = 0, epoch = 0, pointer = 0, base = 0, active = 0, count = 0;
    if (!read_u32(m, 0x8009CBA4u, gfx) || gfx > 1 || !read_u32(m, 0x800A1830u, epoch) ||
        !read_u32(m, 0x800AC650u, pointer) || !read_u32(m, 0x800AC658u + gfx * 4u, base) ||
        !read_u32(m, 0x8009CB90u, active) || base != active || !read_u32(m, 0x800BC9A0u, count) ||
        (count != 0x4650u && count != 0x36B0u) || !valid_guest_range(base, 0x140u + count * 8u) ||
        pointer < base + 0x148u || pointer > base + 0x140u + count * 8u - 1064u || (pointer & 7u))
        return refuse();
    auto &frame = c.frames[gfx];
    if (frame.hud_issued[view] && frame.hud_epoch[view] == epoch)
        return refuse();
    if (!ensure_frame(c, m, gfx))
        return refuse();
    const unsigned commands =
        frame.base + asset_region + view * view_bytes + command_capacity + matrix_capacity;
    Writer w{m, commands, commands + hud_capacity};
    push_state(w);
    w.command(0xe7000000u, 0);
    w.command(0xe300001fu, 0x00000C00u); // One cycle, bilinear, no perspective.
    w.command(0xe200001fu, 0x00504240u); // Translucent surface, no depth read/write.
    const auto &image = c.layout.textures[unsigned(shown)];
    texture(w, frame.base + image.offset, image, true);
    const unsigned left = unsigned(std::lround(x * 4)), top = unsigned(std::lround(y * 4));
    const unsigned right = unsigned(std::lround((x + width) * 4));
    const unsigned bottom = unsigned(std::lround((y + height) * 4));
    w.command(0xe4000000u | (right << 12u) | bottom, (left << 12u) | top);
    w.command(0xe1000000u, 0);
    const unsigned dx = unsigned(std::lround(1024.f * image.width / width));
    const unsigned dy = unsigned(std::lround(1024.f * image.height / height));
    w.command(0xf1000000u, (dx << 16u) | dy);
    finish(w);
    if (!w.good)
        return refuse();
    publish(m, pointer, commands);
    frame.hud_issued[view] = true;
    frame.hud_epoch[view] = epoch;
    ++c.stats.hud_draws;
    return true;
}
} // namespace rr64::mk64_items
