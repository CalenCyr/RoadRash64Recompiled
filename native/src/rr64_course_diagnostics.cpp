#include "rr64_course_diagnostics.hpp"
#include "rr64_actor_pose.hpp"
#include "rr64_engine_layout.hpp"
#include "rr64_experimental_course.hpp"
#include "rr64_race_pack.hpp"
#include <array>
#include <cstdio>
#include <cstdlib>
#include <limits>

namespace {
using namespace rr64::engine;
bool diagnostics_enabled() {
    static const bool enabled = [] {
        const char *option = std::getenv("RR64_COURSE_DIAGNOSTICS");
        return option && option[0] == '1' && option[1] == '\0';
    }();
    return enabled;
}
struct Submissions {
    std::array<unsigned, 32> cells{};
    unsigned count = 0;
    bool truncated = false;
};
thread_local Submissions submissions;
unsigned word(unsigned char *m, unsigned address) {
    unsigned result = 0;
    read_u32(m, address, result);
    return result;
}
float real(unsigned char *m, unsigned address) {
    float result = std::numeric_limits<float>::quiet_NaN();
    read_float(m, address, result);
    return result;
}
void vector(unsigned char *m, const char *name, unsigned address) {
    std::fprintf(stderr, " %s=(%.9g,%.9g,%.9g)", name,
                 real(m, address), real(m, address + 4), real(m, address + 8));
}
void matrix(unsigned char *m, const char *name, unsigned address) {
    Matrix4x4Snapshot value{};
    std::fprintf(stderr, "[RR64-COURSE-DRAW] matrix=%s address=%08X valid=%u values=", name,
                 address, unsigned(decode_n64_matrix(m, address, value)));
    for (unsigned i = 0; i < value.values.size(); ++i)
        std::fprintf(stderr, "%s%.9g", i ? "," : "", value.values[i]);
    std::fputc('\n', stderr);
}
}

extern "C" void rr64_course_diagnostics_begin() {
    if (diagnostics_enabled()) submissions = {};
}
extern "C" void rr64_course_diagnostics_observe(unsigned char *m, unsigned record) {
    if (!diagnostics_enabled() || !m || !rr64::experimental_course::installed()) return;
    const unsigned grid = word(m, globals::terrain_cell_grid);
    if (!valid_guest_range(grid, 4900u * 16u) || record < grid ||
        (record - grid) % 16u || (record - grid) / 16u >= 4900u) return;
    if (submissions.count < submissions.cells.size())
        submissions.cells[submissions.count++] = (record - grid) / 16u;
    else submissions.truncated = true;
}
extern "C" void rr64_course_diagnostics_draw(unsigned char *m) {
    if (!diagnostics_enabled() || !m || !rr64::experimental_course::installed()) return;
    const auto selected = rr64::race_pack::selected_course();
    const unsigned mode = word(m, globals::main_mode), pending = word(m, globals::pending_mode);
    if (!selected || !is_live_race_transition(mode, pending)) return;
    const bool active = rr64::experimental_course::active();
    const unsigned view = word(m, globals::active_viewport);
    const unsigned slot = word(m, globals::actor_render_buffer_slot);
    const unsigned route = word(m, 0x800A6544);
    if (view >= 4 || slot >= 2 || !valid_guest_range(route, 16)) return;
    struct Session {
        unsigned char *owner = nullptr;
        unsigned route = 0;
        std::size_t selected = 0;
        unsigned mode = 0, pending = 0;
        bool active = false;
        std::array<unsigned, 4> calls{};
    };
    static thread_local Session session;
    // Countdown and riding can reuse the same allocation and course identity.
    // Each needs its own short sample, otherwise countdown exhausts the budget.
    if (session.owner != m || session.route != route || session.selected != *selected ||
        session.mode != mode || session.pending != pending || session.active != active)
        session = {m, route, *selected, mode, pending, active, {}};
    auto &counter = session.calls[view];
    // Cap even the counter itself: diagnostics cannot grow during a long race.
    if (counter > 120) return;
    const unsigned sample = counter++;
    if (sample != 0 && sample != 1 && sample != 2 && sample != 5 && sample != 30 && sample != 120) return;
    std::fprintf(stderr, "[RR64-COURSE-DRAW] sample=%u view=%u slot=%u route=%08X mode=%u pending=%u selected=%zu active=%u",
                 sample, view, slot, route, mode, pending, *selected, unsigned(active));
    // Native 7B50C has already subtracted sector XY from both camera vectors.
    vector(m, "camera_target_local", 0x800D69F8);
    vector(m, "camera_eye_local", 0x800D6A28);
    vector(m, "sector", 0x800A4FDC);
    std::fputc('\n', stderr);
    const auto *loaded_route = rr64::experimental_course::route_data();
    std::fprintf(stderr, "[RR64-COURSE-DRAW] native_route_records=%u imported_route_records=%u\n",
                 word(m, 0x800A6540), loaded_route ? loaded_route->record_count : 0u);
    std::fprintf(stderr, "[RR64-COURSE-DRAW] native_cells=");
    for (unsigned i = 0; i < submissions.count; ++i)
        std::fprintf(stderr, "%s%u:%u", i ? "," : "", submissions.cells[i],
                     unsigned(rr64::experimental_course::cell_allowed(submissions.cells[i])));
    std::fprintf(stderr, " truncated=%u\n", unsigned(submissions.truncated));
    for (unsigned actor_slot = 0; actor_slot < 4; ++actor_slot) {
        const unsigned actor = 0x800D8570 + actor_slot * 0x118;
        const unsigned bike = word(m, actor + 0xE0), rider = word(m, actor + 0xE4);
        if (!valid_guest_range(bike, 0x868) || !valid_guest_range(rider, 0x590)) continue;
        std::fprintf(stderr, "[RR64-COURSE-DRAW] actor=%u bike=%08X rider=%08X", actor_slot, bike, rider);
        vector(m, "bike_position", bike + 0x16C);
        vector(m, "bike_velocity", bike + 0x178);
        vector(m, "rider_position", rider + 0x8C);
        std::fputc('\n', stderr);
    }
    const unsigned bank = view * 0x180 + slot * 64;
    matrix(m, "terrain_projection", 0x800B6568 + bank);
    matrix(m, "terrain_view", 0x800B6DE8 + bank);
    matrix(m, "actor_projection", 0x800B6668 + bank);
    matrix(m, "actor_view", 0x800B6EE8 + bank);
}
