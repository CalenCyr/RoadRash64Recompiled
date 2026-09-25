#include "rr64_highlight_camera.hpp"
#include "rr64_engine_layout.hpp"
#include "rr64_prediction_replay.hpp"

#include <cmath>
#include <cstring>

namespace rr64::highlight_camera {
namespace {
using namespace engine;
struct Region { unsigned address, bytes; };
// Camera-control arrays, current world camera, native sector/cell indices,
// per-view look-at inputs and per-bank scaled inputs. Fixed matrices referenced
// by the submitted display list are deliberately not restored here.
constexpr std::array regions{
    Region{0x800a4fa0, 0x320}, Region{0x800d6880, 0x1b4},
    Region{0x800dde80, 0x14}, Region{0x800b7418, 0x90},
    Region{0x800b6b68, 0x270}, Region{0x8009dba4, 4}
};
constexpr unsigned saved_bytes = 0x320 + 0x1b4 + 0x14 + 0x90 + 0x270 + 4;
struct Scope {
    unsigned char* memory = nullptr;
    bool viewport_emitted = false;
    View view{};
    std::array<unsigned char, saved_bytes> saved{};
};
thread_local Scope scope;

bool finite(const Vec3& v) {
    for (float value : v)
        if (!std::isfinite(value) || std::abs(value) > 100000) return false;
    return true;
}
bool valid(const View& v) {
    if (!finite(v.eye) || !finite(v.target) || !finite(v.up)) return false;
    Vec3 direction{}, cross{};
    float length = 0, side = 0;
    for (unsigned i = 0; i < 3; ++i) {
        direction[i] = v.target[i] - v.eye[i];
        length += direction[i] * direction[i];
    }
    for (unsigned i = 0; i < 3; ++i) {
        cross[i] = direction[(i + 1) % 3] * v.up[(i + 2) % 3] -
                   direction[(i + 2) % 3] * v.up[(i + 1) % 3];
        side += cross[i] * cross[i];
    }
    return length >= 0.01f && length <= 4096 && side >= 0.01f;
}
void vector(unsigned char* m, unsigned address, const Vec3& v) {
    for (unsigned i = 0; i < 3; ++i) write_float(m, address + 4 * i, v[i]);
}
}

bool make_view(const Vec3& subject, const Vec3& forward, unsigned shot, View& output) noexcept {
    if (!finite(subject) || !finite(forward)) return false;
    float x = forward[0], y = forward[1];
    const float length = std::hypot(x, y);
    if (length > 0.0001f) { x /= length; y /= length; }
    else { x = 0; y = 1; }
    // Relative forward/right/height offsets keep a stopped or detached subject
    // visible without changing their recorded motion or using a live camera.
    constexpr std::array<Vec3, 3> offsets{{{-7, 4, 3}, {0, -8, 3.5f}, {6, 4, 4}}};
    const auto offset = offsets[shot % offsets.size()];
    View view;
    view.target = {subject[0], subject[1], subject[2] + 1};
    view.eye = {subject[0] + x * offset[0] + y * offset[1],
                subject[1] + y * offset[0] - x * offset[1],
                subject[2] + offset[2]};
    if (!valid(view)) return false;
    output = view;
    return true;
}

bool begin(unsigned char* memory, const View& view) noexcept {
    if (!memory || scope.memory || prediction::active() || !valid(view)) return false;
    for (const auto region : regions)
        if (!valid_guest_range(region.address, region.bytes)) return false;
    unsigned offset = 0;
    for (const auto region : regions) {
        std::memcpy(scope.saved.data() + offset, memory + region.address - kRdramBegin, region.bytes);
        offset += region.bytes;
    }
    scope.view = view;
    scope.memory = memory;
    scope.viewport_emitted = false;
    // Native 15C10/15CFC cache layout and view indices, but not framebuffer
    // dimensions. Results can change video mode while both indices remain 0.
    // Refresh their native viewport/scissor calculations for this replay draw;
    // leave emitted task-owned Vp data intact when restoring camera inputs.
    write_u16(memory, 0x8009dba4, 1);
    return true;
}

void end(unsigned char* memory) noexcept {
    if (!memory || scope.memory != memory) return;
    unsigned offset = 0;
    for (const auto region : regions) {
        std::memcpy(memory + region.address - kRdramBegin, scope.saved.data() + offset, region.bytes);
        offset += region.bytes;
    }
    // 15CFC caches its border together with the view index. The next ordinary
    // view may reuse index zero, so make it recompute its original inset after
    // a replay viewport was actually emitted. Task-owned Vp/DL bytes survive.
    if (scope.viewport_emitted)
        write_u16(memory, 0x8009dba4, 1);
    scope.memory = nullptr;
}

bool active() noexcept { return scope.memory != nullptr; }

unsigned viewport_inset(unsigned original) noexcept {
    if (!scope.memory)
        return original;
    scope.viewport_emitted = true;
    return 0;
}

void apply(unsigned char* memory) {
    if (!memory || scope.memory != memory || prediction::active()) return;
    vector(memory, 0x800d6a28, scope.view.eye);
    vector(memory, 0x800d69f8, scope.view.target);
    vector(memory, 0x800d6a08, scope.view.up);
    vector(memory, 0x800d6a18, scope.view.up);
    Vec3 direction;
    for (unsigned i = 0; i < 3; ++i) direction[i] = scope.view.target[i] - scope.view.eye[i];
    vector(memory, 0x800d69e8, direction);
}
}

extern "C" void rr64_highlight_camera_apply(unsigned char* memory) {
    rr64::highlight_camera::apply(memory);
}

extern "C" unsigned rr64_highlights_viewport_inset(unsigned original) {
    // 6A638's single-camera path requests a two-pixel border. RT64 uses the
    // resulting scissor/viewport intersection to decide whether to expand a
    // 3D view across widescreen; an inset fails its full-width test. Keep the
    // stock border everywhere outside this reversible cinematic draw scope.
    return rr64::highlight_camera::viewport_inset(original);
}
