#include "rr64_world_camera.hpp"
#include "rr64_world_render.hpp"
#include "rr64_actor_render_snapshot.hpp"
#include <cmath>

namespace {
struct Stamp { unsigned char* mapping = nullptr; unsigned epoch = 0; bool valid = false; };
thread_local Stamp stamps[3][2];
bool camera(unsigned char* m, const recomp_context* c, unsigned& view, unsigned& source, unsigned& slot) {
    if (!c || !rr64_world_distance_enabled() || !rr64::lod::supported_scene(m)) return false;
    source = unsigned(c->r18); slot = unsigned(c->r22);
    return source < 3u && slot < 2u &&
        rr64::engine::read_u32(m, 0x8009db84u, view) && view == 0u;
}
}
extern "C" void rr64_world_camera_far(unsigned char* m, void* context) {
    auto* c = static_cast<recomp_context*>(context);
    unsigned view = 0, source = 0, slot = 0;
    if (!camera(m, c, view, source, slot)) return;
    stamps[source][slot].valid = false;
    constexpr float scales[]{4.0f, 100.0f, 10.0f};
    // Hook 16888 follows the legacy 32767 clamp, while f4 still holds the
    // source scale. All three banks use the same world near/far convention.
    if (c->f4.fl != scales[source] || !std::isfinite(c->f8.fl) || c->f8.fl <= 0.0f ||
        c->f8.fl >= rr64::world::far_distance * scales[source]) return;
    c->f6.fl = rr64::world::far_distance * scales[source];
}
extern "C" void rr64_world_camera_normalization(unsigned char* m, void* context) {
    auto* c = static_cast<recomp_context*>(context);
    unsigned view = 0, source = 0, slot = 0;
    if (!camera(m, c, view, source, slot)) return;
    // guPerspective's uint16 reciprocal truncates to zero for large scaled
    // far planes. One is its smallest usable normalization. Only accept the
    // exact argument produced by our preceding hook, in this same call.
    float far = 0.0f;
    constexpr float scales[]{4.0f, 100.0f, 10.0f};
    if (!rr64::engine::read_float(m, unsigned(c->r29) + 0x14u, far) ||
        far != rr64::world::far_distance * scales[source]) return;
    const unsigned address = 0x800b73e8u + view * 12u + source * 4u + slot * 2u;
    std::uint16_t norm = 0;
    if (rr64::engine::read_u16(m, address, norm) && norm == 0u)
        rr64::engine::write_u16(m, address, 1u);
    unsigned epoch = 0;
    if (rr64::engine::read_u32(m, 0x800a1830u, epoch))
        stamps[source][slot] = {m, epoch, true};
}
extern "C" int rr64_world_camera_ready(unsigned char* m, unsigned source, unsigned slot) {
    unsigned epoch = 0;
    if (source >= 3u || slot >= 2u || !rr64_world_distance_enabled() ||
        !rr64::lod::supported_scene(m) || !rr64::engine::read_u32(m, 0x800a1830u, epoch)) return 0;
    const auto& stamp = stamps[source][slot];
    return stamp.valid && stamp.mapping == m && stamp.epoch == epoch;
}
