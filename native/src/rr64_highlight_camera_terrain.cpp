#include "rr64_highlight_camera_terrain.hpp"
#include "rr64_engine_layout.hpp"
#include "rr64_online_terrain.hpp"
#include "rr64_prediction_replay.hpp"
#ifdef RR64_EXPERIMENTAL_COURSE
#include "rr64_experimental_course.hpp"
#endif

#include <cmath>
#include <memory>
#include <mutex>
#include <span>

namespace recomp { std::span<const std::uint8_t> get_rom(); }
extern "C" void func_80014604(unsigned char*, recomp_context*);
extern "C" void func_80014DE4(unsigned char*, recomp_context*);

namespace rr64::highlight_camera::terrain {
namespace {
using namespace engine;
constexpr unsigned grid = 0x80100000, payload = 0x80200000;
constexpr unsigned query = 0x80600000, stack = 0x807f0000;
constexpr unsigned width = 70, side = 1000;
// The exact native 14604/14DE4 closure needs guest addressing, but touches only
// its query, stack, immutable constants, one descriptor and one source cell.
// Static storage has no per-frame allocation; a mutex serializes private use.
struct Scratch {
    std::mutex mutex;
    std::array<unsigned char, kRdramSize> memory{};
    std::shared_ptr<const void> owner;
    unsigned index = ~0u;
} scratch;

void copy_rom(unsigned char* destination, unsigned address,
              std::span<const std::uint8_t> source) {
    for (unsigned i = 0; i < source.size(); ++i)
        destination[((address - kRdramBegin) + i) ^ 3u] = source[i];
}

bool cell_index(const Vec3& point, unsigned& index) {
    unsigned indices[2]{};
    for (unsigned i = 0; i < 3; ++i)
        if (!std::isfinite(point[i]) || std::abs(point[i]) > 100000.f) return false;
    for (unsigned i = 0; i < 2; ++i) {
        const float raw = point[i] * 4.f;
        // Match original 146C8 arithmetic (including its stock edge rounding).
        // The existing imported hook uses the separately audited exact floor.
        double value = (raw + float(width * side / 2)) / float(side);
#ifdef RR64_EXPERIMENTAL_COURSE
        if (experimental_course::active()) value = std::floor(double(raw) / side) + width / 2;
#endif
        if (value < 0 || value >= width) return false;
        indices[i] = static_cast<unsigned>(value);
    }
    index = indices[0] * width + indices[1];
    return true;
}
}

bool floor(unsigned char* memory, const Vec3& point, Floor& output) noexcept {
    if (!memory || prediction::active()) return false;
    unsigned index = 0;
    if (!cell_index(point, index)) return false;
    std::span<const std::uint8_t> bytes;
    std::shared_ptr<const void> owner;
    if (!online_terrain::immutable_cell(memory, index, bytes, &owner) || !owner ||
        bytes.empty() || bytes.size() > 512u * 1024u) return false;
    // The supported executable's static segment maps ROM+0xC00 to guest RAM.
    // Fetch constants from that immutable source, never terminal actor state.
    const auto rom = recomp::get_rom();
    constexpr unsigned material_begin = 0x8009f2b0, material_bytes = 15 * 20;
    constexpr unsigned material_rom = material_begin - kRdramBegin + 0xc00;
    if (rom.size() < material_rom + material_bytes) return false;

    std::lock_guard lock(scratch.mutex);
    auto* m = scratch.memory.data();
    if (scratch.owner != owner) {
        copy_rom(m, 0x80000d00, rom.subspan(0x1900, 0xb0));
        copy_rom(m, material_begin, rom.subspan(material_rom, material_bytes));
        write_u32(m, globals::terrain_cell_grid, grid);
        write_u32(m, globals::terrain_map_width, width);
        write_u32(m, 0x800dacd0, side);
        write_u32(m, 0x800df080, width * side);
        write_u32(m, 0x800dea88, 3);
        scratch.owner = std::move(owner);
        scratch.index = ~0u;
    }
    if (scratch.index != index) {
        if (scratch.index < width * width) write_u32(m, grid + scratch.index * 16, 0);
        copy_rom(m, payload, bytes);
        write_u32(m, grid + index * 16, payload);
        write_s8(m, grid + index * 16 + 12, 5);
        scratch.index = index;
    }
    // Reset the whole query: 14604 alone deliberately preserves the previous
    // triangle chain. Our isolated eye has no historical physics cache.
    std::memset(m + query - kRdramBegin, 0, 0x6c);
    recomp_context call{};
    call.f_odd = &call.f0.u32h;
    call.r29 = guest_address(stack);
    call.r4 = guest_address(query);
    func_80014604(m, &call);
    for (unsigned i = 0; i < 3; ++i) write_float(m, query + i * 4, point[i] * 4.f);
    call.r4 = guest_address(query);
    func_80014DE4(m, &call);
    float height = 0;
    std::uint16_t surface = 0;
    if (!call.r2 || !read_float(m, query + 8, height) || !std::isfinite(height) ||
        !read_u16(m, query + 0x64, surface)) return false;
    output = {height * .25f, surface};
    return true;
}

bool clear_eye(unsigned char* memory, View& view) noexcept {
    Floor contact;
    if (!floor(memory, view.eye, contact)) return false;
    // Native camera 5E19C distinguishes a nearby penetration from a distant
    // surface with an eight-world-unit threshold. Keep that bound and a small
    // quarter-unit eye margin; never raise a rider or replace its target.
    constexpr float clearance = .25f, maximum_lift = 8.f;
    const float lift = contact.height + clearance - view.eye[2];
    if (!std::isfinite(lift) || lift <= 0 || lift > maximum_lift) return false;
    view.eye[2] = contact.height + clearance;
    return true;
}

void reset() noexcept {
    std::lock_guard lock(scratch.mutex);
    scratch.owner.reset();
    scratch.index = ~0u;
}
}
