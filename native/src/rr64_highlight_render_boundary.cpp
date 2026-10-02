#include "rr64_highlight_render_boundary.hpp"
#include "rr64_engine_layout.hpp"

namespace {
struct DrawScope {
    rr64::highlight_render::Boundary boundary;
    unsigned char *memory = nullptr;
    unsigned marker = 0;
    bool emitted = false;
};
thread_local DrawScope scope;

bool cursor(unsigned char *m, unsigned bytes, unsigned reserve, unsigned &p) {
    using namespace rr64::engine;
    unsigned slot = 0, base = 0, active = 0, count = 0;
    if (!m || !read_u32(m, 0x8009cba4u, slot) || slot > 1 ||
        !read_u32(m, 0x800ac658u + slot * 4, base) ||
        !read_u32(m, 0x8009cb90u, active) || active != base ||
        !read_u32(m, 0x800bc9a0u, count) ||
        (count != 0x4650u && count != 0x36b0u) ||
        !read_u32(m, 0x800ac650u, p)) return false;
    const unsigned size = 0x140u + count * 8u;
    return valid_guest_range(base, size) && !(p & 7u) &&
        p >= base + 0x140u && p <= base + size - bytes - reserve;
}
void command(unsigned char *m, unsigned &p, unsigned a, unsigned b) {
    rr64::engine::write_u32(m, p, a);
    rr64::engine::write_u32(m, p + 4, b);
    p += 8;
}
}

extern "C" void rr64_highlight_render_begin(unsigned char *m, unsigned replay_key) {
    unsigned epoch = 0;
    scope.memory = m;
    scope.emitted = false;
    scope.marker = m && rr64::engine::read_u32(m, 0x800a1830u, epoch)
        ? scope.boundary.observe(m, epoch, replay_key) : 0u;
}
extern "C" void rr64_highlight_render_projection(unsigned char *m) {
    unsigned p = 0;
    if (scope.memory != m || !scope.marker || !cursor(m, 32, 1056, p)) return;
    command(m, p, 0xe0525464u, 0x10000064u);
    command(m, p, 0x6400000cu, scope.marker);
    // Simple camera interpolation, identical to RT64's ordinary AUTO-camera
    // policy. Only the identity changes at a cut. Capture is lazy, so this
    // scope must outlive 16A18 until the geometry using its matrices is read.
    constexpr unsigned components = (1u << 3) | (1u << 5) | (1u << 7) |
                                    (1u << 9) | (1u << 11);
    command(m, p, components | 2u, 0);
    command(m, p, 0xe0525464u, 0x20000000u);
    rr64::engine::write_u32(m, 0x800ac650u, p);
    scope.emitted = true;
}
extern "C" void rr64_highlight_render_end(unsigned char *m) {
    unsigned p = 0;
    if (scope.memory == m && scope.emitted && cursor(m, 32, 0, p)) {
        command(m, p, 0xe0525464u, 0x10000064u);
        command(m, p, 0x6400000cu, 0xffffffffu); // Restore ordinary AUTO camera.
        command(m, p, 2u, 0);
        command(m, p, 0xe0525464u, 0x20000000u);
        rr64::engine::write_u32(m, 0x800ac650u, p);
    }
    scope.memory = nullptr;
    scope.marker = 0;
    scope.emitted = false;
}
