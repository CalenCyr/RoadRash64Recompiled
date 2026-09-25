// Headless floor-query helper for ROM import. Input cells are generated locally;
// this executable contains query instructions and no course or ROM assets.
#include "native_contact.h"
#include "rr64_experimental_course.hpp"
#include "rr64_race_pack.hpp"
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <iterator>
#include <map>
#include <set>
#include <stdexcept>
#include <string>
#include <vector>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <crtdbg.h>
#endif

namespace {
bool course_active = false;
std::array<bool, 4900> selected_cells{};
constexpr unsigned grid = 0x80100000, cell = 0x80200000, query = 0x80600000;
gpr guest(unsigned address) { return static_cast<gpr>(static_cast<std::int32_t>(address)); }
void require(bool ok, const char* message) { if (!ok) throw std::runtime_error(message); }
void wf(std::uint8_t* memory, unsigned address, float value) { std::memcpy(memory + (address & 0x7fffff), &value, 4); }
float rf(std::uint8_t* memory, unsigned address) { float value; std::memcpy(&value, memory + (address & 0x7fffff), 4); return value; }
recomp_context context() { recomp_context c{}; c.r29 = S32(0x807f0000); return c; }
void reset(std::uint8_t* memory) { auto c = context(); c.f_odd = &c.f0.u32h; c.r4 = guest(query); func_80014604(memory, &c); }
bool contact(std::uint8_t* memory, float x, float y, float z) {
    wf(memory, query, x); wf(memory, query + 4, y); wf(memory, query + 8, z);
    auto c = context(); c.f_odd = &c.f0.u32h; c.r4 = guest(query); func_80014DE4(memory, &c);
    return c.r2 != 0;
}
struct Probe { unsigned curve, step; float x, z, height; };
struct Result { unsigned misses = 0, above = 0, worst_curve = 0, worst_step = 0; float maximum = 0; std::map<unsigned, unsigned> surfaces; };

int run(const std::filesystem::path& manifest, bool all) {
    std::ifstream input(manifest);
    require(bool(input), "Cannot open the probe manifest");
    std::vector<std::uint8_t> memory(8 * 1024 * 1024); auto* rdram = memory.data();
    for (auto a : {0xd04, 0xd0c, 0xd40, 0xd80, 0xda8}) wf(rdram, 0x80000000u + a, 1);
    for (auto a : {0xd44, 0xd48, 0xd4c}) wf(rdram, 0x80000000u + a, 2147483648.f);
    for (auto a : {0xd84, 0xd9c}) MEM_W(0, guest(0x80000000u + a)) = 0x7f7fffff;
    wf(rdram, 0x80000d88, -12000); wf(rdram, 0x80000d8c, 12000);
    wf(rdram, 0x80000d90, .5f); wf(rdram, 0x80000d94, -.14f); wf(rdram, 0x80000d98, 4);
    wf(rdram, 0x80000da0, .5f); wf(rdram, 0x80000da4, 4);
    MEM_W(0, guest(0x800ddea4)) = grid; MEM_W(0, guest(0x800dea8c)) = 70;
    MEM_W(0, guest(0x800dacd0)) = 1000; MEM_W(0, guest(0x800df080)) = 70000;
    MEM_W(0, guest(0x800dea88)) = 3;
    unsigned address = cell, count = 0;
    input >> count;
    require(bool(input) && count > 0 && count <= 256, "Invalid native cell count");
    std::set<unsigned> used;
    for (unsigned i = 0; i < count; ++i) {
        unsigned index = 0; std::string name;
        input >> index >> std::quoted(name);
        require(bool(input) && index < 4900 && used.insert(index).second, "Invalid native cell record");
        selected_cells[index] = true;
        const std::u8string utf8_name(reinterpret_cast<const char8_t*>(name.data()), name.size());
        std::ifstream file(std::filesystem::path(utf8_name), std::ios::binary | std::ios::ate);
        require(bool(file), "Cannot open a generated cell");
        const auto size = file.tellg();
        require(size > 0 && size < 0x80000 && address + size < query, "Generated cells exceed the query memory budget");
        std::vector<unsigned char> bytes(static_cast<std::size_t>(size)); file.seekg(0);
        require(bool(file.read(reinterpret_cast<char*>(bytes.data()), bytes.size())), "Incomplete cell file");
        for (unsigned j = 0; j < bytes.size(); ++j) rdram[((address & 0x7fffff) + j) ^ 3] = bytes[j];
        MEM_W(0, guest(grid + index * 16)) = address; MEM_B(12, guest(grid + index * 16)) = 5;
        address = (address + unsigned(bytes.size()) + 15) & ~15u;
    }
    input >> count;
    require(bool(input) && count > 0 && count <= 1000000, "Invalid probe count");
    std::vector<Probe> probes; probes.reserve(count);
    for (unsigned i = 0; i < count; ++i) {
        Probe p{}; input >> p.curve >> p.step >> p.x >> p.z >> p.height;
        require(bool(input) && std::isfinite(p.x) && std::isfinite(p.z) && std::isfinite(p.height) &&
                std::abs(p.x) < 20000 && std::abs(p.z) < 20000 && std::abs(p.height) < 20000, "Invalid probe coordinate");
        probes.push_back(p);
    }
    auto report = manifest; report += ".failures.csv";
    std::ofstream failures(report);
    require(bool(failures), "Cannot write probe results");
    failures << "pass,curve,step,x,z,h,hit,ground,error,cache_changed\n";
    Result results[2]; std::vector<float> first; unsigned changed = 0; float maximum_cache_difference = 0;
    struct Special { unsigned id, surface; float ground, error; bool hit; };
    std::vector<Special> special;
    for (unsigned pass = 0; pass < 2; ++pass) {
        reset(rdram);
        for (unsigned i = 0; i < probes.size(); ++i) {
            const auto& p = probes[i]; auto& r = results[pass];
            if (pass == 0) reset(rdram);
            const bool hit = contact(rdram, p.x * 4, p.z * 4, (p.height + 1) * 4);
            const float ground = rf(rdram, query + 8) * .25f, error = std::abs(ground - p.height);
            if (!hit) ++r.misses;
            else {
                ++r.surfaces[MEM_HU(0x64, guest(query))];
                if (error > r.maximum) { r.maximum = error; r.worst_curve = p.curve; r.worst_step = p.step; }
                if (error > 1.f) ++r.above;
            }
            bool cache_changed = false;
            if (pass == 0) {
                first.push_back(ground);
                if (p.curve >= 100000) special.push_back({p.curve, unsigned(MEM_HU(0x64, guest(query))), ground, error, hit});
            }
            else {
                const float difference = std::abs(ground - first[i]);
                maximum_cache_difference = (std::max)(maximum_cache_difference, difference);
                cache_changed = difference > .0001f;
                if (cache_changed) ++changed;
            }
            if (all || !hit || error > 1 || cache_changed)
                failures << pass << ',' << p.curve << ',' << p.step << ',' << p.x << ',' << p.z << ',' << p.height << ',' << hit << ',' << ground << ',' << error << ',' << cache_changed << '\n';
        }
    }
    require(bool(failures), "Incomplete probe results");
    std::printf("{\"passes\":[");
    for (unsigned pass = 0; pass < 2; ++pass) {
        const auto& r = results[pass];
        std::printf("%s{\"fresh_query\":%s,\"samples\":%zu,\"misses\":%u,\"max_reference_height_error\":%.9g,\"reference_height_error_over_1\":%u,\"worst_curve\":%u,\"worst_step\":%u}",
                    pass ? "," : "", pass ? "false" : "true", probes.size(), r.misses, r.maximum, r.above, r.worst_curve, r.worst_step);
    }
    const bool passed = !results[0].misses && !results[1].misses && !results[0].above && !results[1].above && maximum_cache_difference <= .1251f;
    std::printf("],\"special\":[");
    for (unsigned i = 0; i < special.size(); ++i) {
        const auto& s = special[i];
        std::printf("%s{\"probe_id\":%u,\"hit\":%s,\"surface\":%u,\"ground\":%.9g,\"reference_error\":%.9g}", i ? "," : "", s.id, s.hit ? "true" : "false", s.surface, s.ground, s.error);
    }
    std::printf("],\"cached_height_mismatches\":%u,\"maximum_cached_height_difference\":%.9g,\"passed\":%s,\"actual_native_contact\":true}\n", changed, maximum_cache_difference, passed ? "true" : "false");
    return passed ? 0 : 3; // A probe miss is data, not converter failure.
}
}
extern "C" void _nsqrtf(std::uint8_t*, recomp_context* c) { c->f0.fl = std::sqrt(c->f12.fl); }
extern "C" void do_break(std::uint32_t) { std::abort(); }
extern "C" void rr64_course_progress_log(const char*, ...) {}
namespace rr64::experimental_course {
bool active() noexcept { return course_active; }
bool installed() noexcept { return course_active; }
bool cell_allowed(unsigned index) noexcept { return index < selected_cells.size() && selected_cells[index]; }
void restore_descriptor(unsigned char*) noexcept {}
void load_selection() noexcept {}
void load_stock() noexcept {}
}
namespace rr64::race_pack {
std::optional<std::size_t> selected_course() noexcept { return {}; }
std::span<const CourseMenuEntry> menu_courses() noexcept { return {}; }
}
#ifdef _WIN32
int wmain(int argc, wchar_t** argv) {
    SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX);
    _set_abort_behavior(0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT);
#else
int main(int argc, char** argv) {
#endif
    try {
        if (argc < 2 || argc > 4) return 2;
        bool all = false;
        for (int i = 2; i < argc; ++i) {
            const std::filesystem::path arg(argv[i]);
            if (arg == "--course-active") course_active = true;
            else if (arg == "all" || arg == "--all") all = true;
            else return 2;
        }
        return run(argv[1], all);
    } catch (const std::exception& e) { std::cerr << e.what() << '\n'; return 2; }
}
