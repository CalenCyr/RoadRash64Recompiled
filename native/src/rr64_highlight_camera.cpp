#include "rr64_highlight_camera.hpp"
#include "rr64_highlight_camera_terrain.hpp"
#include "rr64_engine_layout.hpp"
#include "rr64_prediction_replay.hpp"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <mutex>

namespace rr64::highlight_camera {
namespace {
using namespace engine;
struct Region { unsigned address, bytes; };
// Camera-control arrays, current world camera, native sector/cell indices,
// per-view look-at inputs and per-bank scaled inputs. Fixed matrices referenced
// by the submitted display list are deliberately not restored here.
constexpr std::array regions{
    Region{0x800a4fa0, 0x328}, Region{0x800d6880, 0x1b4},
    Region{0x800dde80, 0x14}, Region{0x800b7418, 0x90},
    Region{0x800b6b68, 0x270}, Region{0x8009dba4, 4}
};
constexpr unsigned saved_bytes = 0x328 + 0x1b4 + 0x14 + 0x90 + 0x270 + 4;
struct Scope {
    unsigned char* memory = nullptr;
    bool viewport_emitted = false;
    View view{};
    std::array<unsigned char, saved_bytes> saved{};
};
thread_local Scope scope;

struct OrdinaryView {
    View view{};
    Vec3 unbanked_up{}, direction{};
    bool valid = false, armed = false, held = false;
    unsigned held_epoch = 0, rider = 0;
};
// Preparation can run on the game thread and playback on the graphics worker.
// Only this small handoff is shared; native camera calculations stay scoped to
// their calling thread and task-owned matrices are never copied back.
struct ReturnCamera {
    std::mutex mutex;
    unsigned char* memory = nullptr;
    unsigned grid = 0, width = 0, layout = 0, epoch = 0, mode = 0;
    unsigned diagnostic_budget = 0;
    bool replay = false;
    std::array<OrdinaryView, 4> views{};
} returning;
struct Preparation {
    unsigned char* memory = nullptr;
    unsigned view = 0, grid = 0, width = 0, layout = 0, epoch = 0, mode = 0;
    unsigned queries = 0, missing = 0, oldest_request = ~0u, rider = 0;
    float arm = 0;
};
thread_local Preparation preparation;

unsigned word(unsigned char* m, unsigned address) {
    unsigned value = 0;
    read_u32(m, address, value);
    return value;
}
void clear_returning() {
    returning.memory = nullptr;
    returning.grid = returning.width = returning.layout = returning.epoch = returning.mode = 0;
    returning.diagnostic_budget = 0;
    returning.replay = false;
    returning.views = {};
}
bool diagnostics() {
    static const bool enabled = [] {
        const char* value = std::getenv("RR64_DIAGNOSTICS");
        const char* course = std::getenv("RR64_COURSE_PHYSICS_TRACE");
        return (value && value[0] && value[0] != '0') ||
               (course && course[0] && course[0] != '0');
    }();
    return enabled;
}
void trace(const char* event, const Preparation& p) {
    // Reserve one release observation for each view even after a long refill.
    if (diagnostics() && returning.diagnostic_budget &&
        (event[0] != 'h' || returning.diagnostic_budget > 4)) {
        --returning.diagnostic_budget;
        std::fprintf(stderr, "[highlights-camera] %s epoch=%u view=%u pending=%u queries=%u\n",
                     event, p.epoch, p.view, p.missing, p.queries);
    }
}

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
Vec3 vector(unsigned char* m, unsigned address) {
    Vec3 value{};
    for (unsigned i = 0; i < 3; ++i) read_float(m, address + 4 * i, value[i]);
    return value;
}
bool ordinary_valid(const View& v) {
    if (!finite(v.eye) || !finite(v.target) || !finite(v.up)) return false;
    Vec3 direction{};
    float length = 0, side = 0;
    for (unsigned i = 0; i < 3; ++i) {
        direction[i] = v.target[i] - v.eye[i];
        length += direction[i] * direction[i];
    }
    for (unsigned i = 0; i < 3; ++i) {
        const float c = direction[(i + 1) % 3] * v.up[(i + 2) % 3] -
                        direction[(i + 2) % 3] * v.up[(i + 1) % 3];
        side += c * c;
    }
    // Ordinary cameras are not constrained by the alternate replay shot radius.
    return std::isfinite(length) && std::isfinite(side) && length >= 0.01f && side >= 0.01f;
}
void set_view(unsigned char* m, const View& v) {
    vector(m, 0x800d6a28, v.eye);
    vector(m, 0x800d69f8, v.target);
    vector(m, 0x800d6a08, v.up);
    vector(m, 0x800d6a18, v.up);
    Vec3 direction{};
    for (unsigned i = 0; i < 3; ++i) direction[i] = v.target[i] - v.eye[i];
    vector(m, 0x800d69e8, direction);
}
bool same_target_cell(unsigned char* m, const Preparation& p, const Vec3& a, const Vec3& b) {
    float scale = 0;
    const unsigned side = word(m, 0x800dacd0);
    if (!read_float(m, 0x80005f44, scale) || !std::isfinite(scale) || scale <= 0 ||
        side == 0 || side > 0x100000u) return false;
    const float center = float((p.width * side) >> 1);
    for (unsigned i = 0; i < 2; ++i) {
        const float x = (a[i] * scale + center) / side;
        const float y = (b[i] * scale + center) / side;
        if (!std::isfinite(x) || !std::isfinite(y) || x < 0 || y < 0 ||
            x >= p.width || y >= p.width || unsigned(x) != unsigned(y)) return false;
    }
    return true;
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
    View adjusted = view;
    terrain::clear_eye(memory, adjusted);
    if (!valid(adjusted)) return false;
    for (const auto region : regions)
        if (!valid_guest_range(region.address, region.bytes)) return false;
    unsigned offset = 0;
    for (const auto region : regions) {
        std::memcpy(scope.saved.data() + offset, memory + region.address - kRdramBegin, region.bytes);
        offset += region.bytes;
    }
    scope.view = adjusted;
    scope.memory = memory;
    scope.viewport_emitted = false;
    {
        std::lock_guard lock(returning.mutex);
        const unsigned mode = word(memory, globals::main_mode);
        const unsigned epoch = word(memory, 0x800a1830);
        if (returning.memory == memory && returning.grid == word(memory, 0x800ddea4) &&
            epoch >= returning.epoch && is_race_results_mode(mode) &&
            word(memory, globals::pending_mode) == mode) {
            if (!returning.replay) {
                for (auto& camera : returning.views) {
                    camera.armed = camera.valid;
                    camera.held = false;
                }
                returning.diagnostic_budget = 12;
            }
            returning.replay = true;
            returning.mode = mode;
        }
    }
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

void reset(unsigned char* memory) noexcept {
    terrain::reset();
    std::lock_guard lock(returning.mutex);
    if (!memory || returning.memory == memory) clear_returning();
    if (!memory || preparation.memory == memory) preparation = {};
}

void prepare(unsigned char* memory, void* context) {
    preparation = {};
    if (!memory || !context || scope.memory || prediction::active()) return;
    const auto& ctx = *static_cast<const recomp_context*>(context);
    Preparation p;
    p.memory = memory;
    p.view = static_cast<unsigned>(ctx.r4);
    p.grid = word(memory, 0x800ddea4);
    p.width = word(memory, 0x800dea8c);
    p.layout = word(memory, 0x800a4f24);
    p.epoch = word(memory, 0x800a1830);
    p.mode = word(memory, globals::main_mode);
    if (p.view < returning.views.size()) p.rider = word(memory, 0x800a657c + p.view * 4);
    const bool race = is_live_race_mode(p.mode) || is_race_results_mode(p.mode);
    std::lock_guard lock(returning.mutex);
    if (!race || word(memory, globals::pending_mode) != p.mode ||
        p.view >= returning.views.size() || p.rider >= kMaximumRacers ||
        p.width == 0 || p.width > 255 ||
        !valid_guest_range(p.grid, p.width * p.width * 16) ||
        !read_float(memory, 0x800a52b8 + p.view * 4, p.arm) || !std::isfinite(p.arm)) {
        clear_returning();
        return;
    }
    const bool changed = returning.memory != memory || returning.grid != p.grid ||
        returning.width != p.width || returning.layout != p.layout || p.epoch < returning.epoch;
    const bool leave_results = returning.replay && !is_race_results_mode(p.mode);
    const bool mode_change = returning.mode && returning.mode != p.mode &&
        is_race_results_mode(returning.mode);
    if (changed || leave_results || mode_change) clear_returning();
    returning.memory = memory;
    returning.grid = p.grid;
    returning.width = p.width;
    returning.layout = p.layout;
    returning.epoch = p.epoch;
    returning.mode = p.mode;
    returning.replay = false;
    preparation = p;
}

void floor_cell(unsigned char* memory, void* context) {
    if (!memory || !context || preparation.memory != memory || scope.memory || prediction::active()) return;
    const auto& ctx = *static_cast<const recomp_context*>(context);
    const unsigned cell = static_cast<unsigned>(ctx.r4);
    const unsigned state = static_cast<unsigned>(ctx.r5);
    if (cell < preparation.grid || (cell - preparation.grid) % 16 ||
        cell - preparation.grid >= preparation.width * preparation.width * 16) return;
    ++preparation.queries;
    // A real empty/out-of-bounds floor is not a streaming failure. Respect
    // online terrain lookup's final state: a supplied ready cell needs no hold.
    unsigned short units = 0;
    if ((state == 1 || state == 3 || state == 4) && word(memory, cell + 4) != 0 &&
        read_u16(memory, cell + 14, units) && units != 0) {
        ++preparation.missing;
        const unsigned requested = word(memory, cell + 8);
        if (requested < preparation.oldest_request) preparation.oldest_request = requested;
    }
}

unsigned viewport_inset(unsigned original) noexcept {
    if (!scope.memory)
        return original;
    scope.viewport_emitted = true;
    return 0;
}

void apply(unsigned char* memory) {
    if (!memory || prediction::active()) return;
    if (scope.memory == memory) {
        set_view(memory, scope.view);
        return;
    }
    if (preparation.memory != memory) return;
    const Preparation p = preparation;
    preparation = {}; // Later actor/terrain lookups are not camera probes.
    const View current{vector(memory, 0x800d6a28), vector(memory, 0x800d69f8),
                       vector(memory, 0x800d6a08)};
    std::lock_guard lock(returning.mutex);
    if (returning.memory != memory || returning.grid != p.grid || returning.width != p.width ||
        returning.layout != p.layout || returning.mode != p.mode || returning.epoch != p.epoch) return;
    auto& camera = returning.views[p.view];
    if (camera.armed) {
        const bool target_matches = p.rider == camera.rider &&
            same_target_cell(memory, p, current.target, camera.view.target);
        // The native draw stamps every selected cell. If the held view never
        // selected a failed probe, stop holding rather than wait on a region it
        // cannot load. This is actual streaming progress, not a frame timeout.
        const bool selected = !camera.held || p.epoch == camera.held_epoch ||
                              p.oldest_request >= camera.held_epoch;
        if (camera.valid && is_race_results_mode(p.mode) && p.missing && target_matches && selected) {
            set_view(memory, camera.view);
            vector(memory, 0x800d6a18, camera.unbanked_up);
            vector(memory, 0x800d69e8, camera.direction);
            write_float(memory, 0x800a52b8 + p.view * 4, p.arm);
            camera.held = true;
            camera.held_epoch = p.epoch;
            trace("hold", p);
            return;
        }
        trace(!target_matches ? "release-target-change" : !selected ? "release-unselected-cell" :
              p.missing ? "release-invalid" : "release-ready", p);
        camera.armed = camera.held = false;
    }
    const Vec3 unbanked_up = vector(memory, 0x800d6a18);
    const Vec3 direction = vector(memory, 0x800d69e8);
    if (p.queries && !p.missing && ordinary_valid(current) && finite(unbanked_up) && finite(direction)) {
        camera.view = current;
        camera.unbanked_up = unbanked_up;
        camera.direction = direction;
        camera.valid = true;
        camera.rider = p.rider;
    }
}
}

extern "C" void rr64_highlight_camera_apply(unsigned char* memory) {
    rr64::highlight_camera::apply(memory);
}

extern "C" void rr64_highlight_camera_prepare(unsigned char* memory, void* context) {
    rr64::highlight_camera::prepare(memory, context);
}

extern "C" void rr64_highlight_camera_floor_cell(unsigned char* memory, void* context) {
    rr64::highlight_camera::floor_cell(memory, context);
}

extern "C" unsigned rr64_highlights_viewport_inset(unsigned original) {
    // 6A638's single-camera path requests a two-pixel border. RT64 uses the
    // resulting scissor/viewport intersection to decide whether to expand a
    // 3D view across widescreen; an inset fails its full-width test. Keep the
    // stock border everywhere outside this reversible cinematic draw scope.
    return rr64::highlight_camera::viewport_inset(original);
}
