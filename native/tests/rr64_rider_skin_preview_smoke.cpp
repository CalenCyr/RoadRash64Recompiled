#include "rr64_engine_layout.hpp"
#include "rr64_rider_skin_fixture.hpp"
#include "rr64_rider_skin_menu.hpp"
#include "rr64_rider_skin_render.hpp"
#include "rr64_rider_skin_diagnostics.hpp"
#include "rr64_netplay.hpp"
#include <algorithm>
#include <array>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

extern "C" void fixture_selection_preview_call(unsigned char *, recomp_context *);
extern "C" void func_8000FF64(unsigned char *, recomp_context *);
extern "C" void func_8000F9E8(unsigned char *, recomp_context *);
namespace skin = rr64::rider_skins;
namespace engine = rr64::engine;
using Command = std::pair<unsigned, unsigned>;
namespace {
unsigned checks = 0, arena = 0x1200000, epoch = 0;
unsigned packed_matrices = 0;
bool online = false;
std::vector<unsigned char> memory(64 * 1024 * 1024);
auto *m = memory.data();
constexpr unsigned main_base = 0x80400000, start = main_base + 0x148;
constexpr unsigned pool = 0x80200000, selected = 0x8009f670;
constexpr unsigned graph_base = 0x80310000, mesh_base = 0x80320000;
unsigned rider_node(unsigned slot) { return 0x80300000 + slot * 0x100; }
unsigned bike_node(unsigned slot) { return 0x80301000 + slot * 0x100; }
unsigned graph(unsigned slot, bool bike = false) {
    return graph_base + slot * 0x100 + (bike ? 0x1000 : 0);
}
void check(bool value, const char *why) {
    ++checks;
    if (!value) { std::fprintf(stderr, "FAIL %s (%u)\n", why, checks); std::exit(1); }
}
unsigned word(unsigned address) {
    unsigned value; std::memcpy(&value, m + (address & 0x7fffffff), 4); return value;
}
void put(unsigned address, unsigned value) {
    std::memcpy(m + (address & 0x7fffffff), &value, 4);
}
void half(unsigned address, unsigned value) { engine::write_u16(m, address, std::uint16_t(value)); }
unsigned header(unsigned donor, unsigned slot) {
    return 0x80500000 + donor * 0x4000 + slot * 0x1000;
}
void configure() {
    ++epoch;
    put(0x8009cba4, epoch & 1); put(0x800a1830, epoch);
    put(0x800ac658 + (epoch & 1) * 4, main_base); put(0x8009cb90, main_base);
    put(0x800bc9a0, 0x4650); put(0x800ac650, start); put(0x8009db2c, 0);
    put(0x800b1a20, ~0u); // Native material-mode cache.
    put(0x800b6550 + (epoch & 1) * 4, 0x80600000); put(0x800b0810, 0);
    put(0x8009dbec, ~0u); put(0x8009db84, 0); put(0x8009dbd4, 0);
    put(0x8009dbe4, 0x3f800000); half(0x800b73e8, 1);
    put(0x80000c84, 0x3f800000); put(0x80000c74, 0x3f800000);
    put(0x80000de0, 0x3f800000); put(0x80000e10, 0x4f000000);
    packed_matrices = 0;
}
void mode(unsigned handler) {
    // These leaf selectors are dispatched by native 2F14C. The outer mode
    // record contains shared menu callbacks, never the leaf function itself.
    const unsigned mode = handler == 0x800256c0 ? 33 : handler == 0x80027a90 ? 35 :
                          handler == 0x8002dc30 ? 45 : 46;
    put(engine::globals::main_mode, mode);
    put(engine::globals::mode_records + mode * engine::kModeRecordSize + 8, 0x80072704);
    put(engine::globals::mode_records + mode * engine::kModeRecordSize + 12, 0x8007273c);
    put(engine::local_race::menu_humans, 4);
}
void make_graph(unsigned address, unsigned mesh, unsigned donor, unsigned count,
                bool embedded_material = false) {
    for (unsigned i = 0; i < count; ++i) {
        const unsigned record = address + i * 32;
        // F958 sets bit29: E65C resolves model - half(model+0xE)*8.
        // FF64 instead passes graph+0x10 as the explicit material header.
        const unsigned model = embedded_material ? header(donor, i) + 0xb00 : mesh + i * 0x100;
        half(record + 4, 0x10); half(record + 8, i + 1 == count ? 0 : 4);
        half(record + 10, 0); put(record + 0x10, header(donor, i)); put(record + 0x14, model);
        half(model + 0x0e, embedded_material ? 0x160 : 0);
        // One native F7F4 batch: six vertices and one triangle.
        half(model + 0x0c, 1);
        const unsigned batch = model + 0x28;
        half(batch, 1); half(batch + 2, 0x0606); half(batch + 4, 0x80);
        half(batch + 6, 0x68); half(batch + 0x70, 0x22);
    }
}
void setup_slot(unsigned slot, unsigned appearance) {
    const unsigned donor = skin::native_donor(appearance), n = rider_node(slot), b = bike_node(slot);
    skin::set_menu_selection(slot, appearance); put(selected + slot * 4, donor);
    put(n, 2); put(n + 4, pool + slot * engine::rider::stride); put(n + 0x40, slot);
    put(n + 0x28, graph(slot)); put(n + 0x3c, slot == 3 ? 0 : rider_node(slot + 1));
    put(b, 1); put(b + 0x28, graph(slot, true));
    // The bike intentionally shares a donor header: graph authentication must
    // prevent a same-texture bike or weapon from inheriting the rider's skin.
    make_graph(graph(slot), mesh_base + slot * 0x400, donor, 2);
    make_graph(graph(slot, true), mesh_base + 0x2000 + slot * 0x400, donor, 1, true);
}
void visible_matrices(unsigned slot, bool bike) {
    const unsigned g = graph(slot, bike), donor = word(selected + slot * 4);
    const unsigned transform = g + 0xa0, metadata = g + 0xc0;
    make_graph(g + 64, mesh_base + slot * 0x400, donor, bike ? 1 : 2, bike);
    half(g + 4, 0x13); half(g + 8, 4); half(g + 10, 0);
    put(g + 12, transform); put(g + 20, metadata); half(metadata + 0x12, 0);
    half(g + 32 + 4, 0x12); half(g + 32 + 8, 4); half(g + 32 + 10, 0);
    put(g + 32 + 12, transform);
    // Position zero, identity quaternion. Native15A90 computes both matrices.
    for (unsigned i = 0; i < 7; ++i) put(transform + i * 4, i == 6 ? 0x3f800000 : 0);
    half(g + 64 + (bike ? 0 : 32) + 10, 0x4000); // Native matrix pop.
}
recomp_context context() {
    recomp_context c{}; c.r29 = engine::guest_address(0x807ff000); return c;
}
void showroom(unsigned slot) {
    auto c = context(); c.r18 = engine::guest_address(bike_node(slot));
    c.r19 = engine::guest_address(rider_node(slot)); c.r20 = slot;
    fixture_selection_preview_call(m, &c);
    check(unsigned(c.r29) == 0x807ff000 && unsigned(c.r20) == slot + 1,
          "native callsite and both graph functions preserve their stack and native slot step");
}
void draw(unsigned root, bool rider) {
    auto c = context(); c.r4 = engine::guest_address(root);
    (rider ? func_8000FF64 : func_8000F9E8)(m, &c);
    check(unsigned(c.r29) == 0x807ff000, "native graph return restores stack");
}
void flatten(unsigned begin, unsigned end, std::vector<Command> &out, unsigned depth = 0) {
    check(depth < 2, "bounded per-preview branch");
    for (unsigned p = begin, count = 0; p < end && count < 4096; p += 8, ++count) {
        Command c{word(p), word(p + 4)};
        // Native F7F4 emits TRI1 without writing its unused second word.
        if ((c.first >> 24) == 5) c.second = 0;
        if (c.first == 0xdf000000) return;
        if (c.first == 0xde000000) {
            check(c.second >= 0x80800000, "custom branch owns extended memory");
            flatten(c.second, c.second + 32768, out, depth + 1);
        } else if ((c.first >> 24) != 0xe0 && (c.first >> 24) != 0x64) out.push_back(c);
    }
}
std::vector<Command> output() {
    std::vector<Command> out; flatten(start, word(0x800ac650), out); return out;
}
std::vector<unsigned> images(const std::vector<Command> &commands) {
    std::vector<unsigned> out;
    for (const auto &c : commands) if (c.first == 0xfd500000) out.push_back(c.second);
    return out;
}
void verify_colors(const std::vector<Command> &commands, unsigned appearance, bool bike) {
    const auto refs = images(commands);
    check(refs.size() == (bike ? 3u : 2u), "native graph visits both rider materials");
    if (bike) check(refs[0] == header(skin::native_donor(appearance), 0) + 64,
                    "bike with identical donor header remains native");
    for (unsigned i = 0; i < 2; ++i) {
        const unsigned p = refs[i + unsigned(bike)];
        check(p >= 0x80800000, "native showroom rider actually receives skin image");
        check(m[((p & 0x7fffffff)) ^ 3] == 17 + (appearance - 1) * 32 + i,
              "native showroom image matches selected appearance and material");
    }
    unsigned palettes = 0;
    for (const auto &c : commands) if (c.first == 0xfd100000) {
        check(palettes < refs.size() && c.second == refs[palettes] + 2048,
              "native EC30 palette address follows the corresponding image");
        ++palettes;
        if (palettes > unsigned(bike)) check(c.second >= 0x80800000,
                                           "rider palette follows its replaced image");
    }
    check(palettes == refs.size(), "all native CI8 images have their palette binding");
}
void unchanged_geometry(const std::vector<Command> &native, const std::vector<Command> &skinned) {
    // The preview intentionally invalidates the cache: a donor header shared
    // with the bike must be emitted again to bind the rider's private texture.
    const auto geometry = [](const auto &commands) {
        std::vector<Command> result;
        for (const auto &c : commands) {
            const unsigned op = c.first >> 24;
            if (op == 1 || op == 5 || op == 6 || op == 0xda || op == 0xd8 || op == 0xdb)
                result.push_back(c);
        }
        return result;
    };
    check(geometry(native) == geometry(skinned), "native vertex and triangle commands unchanged");
}
} // namespace

namespace recomp {
void *alloc(unsigned char *memory, std::size_t size) {
    const auto p = arena; arena += (unsigned(size) + 15) & ~15u;
    check(arena < 64 * 1024 * 1024, "bounded offline texture/frame arena"); return memory + p;
}
void free(unsigned char *, void *) {}
}
rr64::netplay::PhysicsRules rr64::netplay::get_physics_rules() {
    PhysicsRules rules; rules.active = online; return rules;
}
extern "C" int rr64_custom_cop_enabled() { return 0; }
extern "C" unsigned rr64_custom_cop_bike_entry(unsigned char *, unsigned, unsigned) { return 0; }
extern "C" int rr64_custom_cop_can_start(unsigned char *) { return 1; }
extern "C" void func_800796F8(unsigned char *, recomp_context *) {}
extern "C" void func_8000CD34(unsigned char *, recomp_context *) {
    check(false, "fixture materials must already be loaded");
}
extern "C" void guMtxF2L(unsigned char *, recomp_context *c) {
    // The native transform must produce identity before this controlled
    // libultra conversion. Native matrix address selection/emission stays real.
    ++packed_matrices;
    const unsigned source = unsigned(c->r4), target = unsigned(c->r5);
    check(engine::valid_guest_range(source, 64) && engine::valid_guest_range(target, 64),
          "matrix packing stays inside guest memory");
    for (unsigned i = 0; i < 16; ++i)
        check(word(source + i * 4) == (i % 5 == 0 ? 0x3f800000u : 0u),
              "native neutral transform produces identity matrix");
    for (unsigned i = 0; i < 16; ++i) put(target + i * 4, 0);
    for (unsigned i = 0; i < 16; i += 5) half(target + i * 2, 1);
}
extern "C" void _bcopy(unsigned char *, recomp_context *c) {
    check(unsigned(c->r6) == 64, "native matrix copy size");
    std::memmove(m + (unsigned(c->r5) & 0x7fffffff), m + (unsigned(c->r4) & 0x7fffffff), 64);
}
extern "C" void rr64_highlight_render_projection(unsigned char *) {}
extern "C" void rr64_weapon_source(unsigned char *, void *, unsigned) {}
extern "C" void rr64_weapon_matrix(unsigned char *, unsigned, unsigned) {}
extern "C" void rr64_weapon_packed(unsigned char *, unsigned, unsigned) {}
extern "C" void rr64_highlights_weapon_matrix(unsigned char *, unsigned, unsigned) {}

int main() {
    std::array<skin::Appearance, 3> catalog{skin_fixture("male", "Male", 0),
        skin_fixture("female", "Female", 10), skin_fixture("other", "Other", 0)};
    for (unsigned i = 0; i < catalog.size(); ++i) for (unsigned s = 0; s < 4; ++s) {
        auto &t = catalog[i].textures[s];
        std::fill_n(t.bytes.begin(), t.width * t.height, static_cast<unsigned char>(17 + i * 32 + s));
    }
    check(skin::install_catalog(catalog), "install distinct native donor appearances"); skin::begin_session();
    put(0x800d13c8, pool); put(0x800a1454, rider_node(0));
    for (unsigned donor : {0u, 10u}) for (unsigned s = 0; s < 4; ++s) {
        const unsigned h = header(donor, s), size = s == 3 ? 512 : 2048, native = donor == 10 ? 33 : 0;
        put(0x800a1578 + donor * 8, native);
        // The captured showroom loads only its two detailed materials. The
        // race-only lower-detail headers must not be required for a preview.
        put(0x800d3840 + (native * 4 + s) * 8, s < 2 ? h : 0);
        put(h, 0x16); half(h + 0x22, 512); half(h + 0x26, size);
        put(h + 0x2c, s == 3 ? 32 : 64); put(h + 0x30, s == 3 ? 16 : 32); put(h + 0x34, 8);
    }
    const std::array<unsigned, 4> choices{1, 3, 2, 1};
    for (unsigned handler : {0x800256c0u, 0x80027a90u, 0x8002cd60u, 0x8002dc30u}) {
        mode(handler);
        for (unsigned i = 0; i < 4; ++i) setup_slot(i, choices[i]);
        const unsigned slots = handler == 0x80027a90 ? 4 : 1;
        for (unsigned slot = 0; slot < slots; ++slot) for (unsigned chosen : {1u, 2u, 3u}) {
            setup_slot(slot, chosen); skin::set_menu_selection(slot, 0);
            configure(); showroom(slot); const auto native = output();
            check(images(native).back() == header(skin::native_donor(chosen), 1) + 64,
                  "stock showroom stays on original textures");
            skin::set_menu_selection(slot, chosen); configure(); showroom(slot);
            const auto changed = output(); verify_colors(changed, chosen, true);
            unchanged_geometry(native, changed);
            online = true; configure(); showroom(slot); check(output() == native, "online showroom stays native"); online = false;
        }
    }
    mode(0x80027a90);
    for (unsigned i = 0; i < 4; ++i) setup_slot(i, choices[i]);
    configure(); std::vector<Command> first; unsigned first_end = 0;
    for (unsigned slot = 0; slot < 4; ++slot) {
        const auto begin = word(0x800ac650); showroom(slot);
        std::vector<Command> current; flatten(begin, word(0x800ac650), current);
        verify_colors(current, choices[slot], true);
        if (!slot) { first = current; first_end = word(0x800ac650); }
    }
    std::vector<Command> retained; flatten(start, first_end, retained);
    check(retained == first, "later local previews do not overwrite first player commands");
    // Unlisted held-weapon graph sharing the same donor material stays native.
    const unsigned weapon = graph_base + 0x2000;
    make_graph(weapon, mesh_base + 0x4000, 0, 2, true);
    configure(); draw(weapon, false);
    check(images(output()) == std::vector<unsigned>{header(0,0)+64, header(0,1)+64}, "held weapon graph is not recolored");
    // Native graph early exits must pop the preview scope, including null roots.
    for (unsigned i = 0; i < 12; ++i) { draw(0, false); draw(0, true); }
    half(graph(0) + 4, 0x13); half(graph(0) + 10, 1);
    for (unsigned i = 0; i < 12; ++i) draw(graph(0), true);
    setup_slot(0, 1); configure(); showroom(0); verify_colors(output(), 1, true);
    for (unsigned chosen : {1u, 2u, 3u}) {
        setup_slot(0, chosen); visible_matrices(0, false); visible_matrices(0, true);
        skin::set_menu_selection(0, 0); configure(); showroom(0); const auto native = output();
        check(packed_matrices == 4, "both visible native graphs execute root and child matrices");
        skin::set_menu_selection(0, chosen); configure(); showroom(0);
        const auto changed = output(); verify_colors(changed, chosen, true);
        unchanged_geometry(native, changed);
        check(packed_matrices == 4, "skinned native graphs preserve matrix execution");
        check(std::count_if(changed.begin(), changed.end(), [](const auto &c) {
            return (c.first >> 24) == 0xda;
        }) == 6, "native projection and visible model matrices survive replacement");
    }
    // Reject stale entity, wrong actor type and race mode through real traversal.
    for (unsigned bad = 0; bad < 3; ++bad) {
        setup_slot(0, 1); mode(0x80027a90);
        if (bad == 0) put(rider_node(0) + 4, pool + 4);
        if (bad == 1) put(rider_node(0), 3);
        if (bad == 2) put(engine::globals::main_mode, 0x12);
        configure(); draw(graph(0), true);
        check(images(output()) == std::vector<unsigned>{header(0,0)+64, header(0,1)+64},
              "unowned or non-menu rider graph remains native");
    }
    const auto exact_enabled = [](const char *name) {
        const char *value = std::getenv(name);
        return value && std::strcmp(value, "1") == 0;
    };
    const bool tracing = exact_enabled("RR64_DIAGNOSTICS") &&
                         exact_enabled("RR64_RIDER_SKIN_PREVIEW_TRACE");
    check(skin::preview_trace_enabled() == tracing, "preview trace requires both explicit options");
    skin::PreviewTrace sample{};
    unsigned samples = 0, successes = 0;
    while (skin::take_preview_trace(sample)) {
        check(++samples <= 128, "preview diagnostic sample storage is bounded");
        check(sample.image_count <= sample.images.size(), "preview diagnostic image count is bounded");
        if (!sample.reason && sample.replacements) ++successes;
    }
    check(!skin::take_preview_trace(sample), "preview diagnostic drain is empty after consumption");
    check(tracing ? samples && successes : samples == 0,
          "enabled trace observes actual replacements and disabled trace has no samples");
    std::printf("Rider skin native preview: %u checks passed.\n", checks);
    std::printf("Preview diagnostics: enabled=%u samples=%u successes=%u dropped=%u.\n",
                unsigned(tracing), samples, successes, skin::preview_trace_dropped());
}
