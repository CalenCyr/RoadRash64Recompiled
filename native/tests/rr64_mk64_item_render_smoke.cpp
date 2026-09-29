#include "rr64_engine_layout.hpp"
#include "rr64_mk64_item_assets.hpp"
#include "rr64_mk64_item_render.hpp"
#include "rr64_mk64_item_lightning.hpp"
#include <algorithm>
#include <bit>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iterator>
#include <limits>
#include <vector>

namespace {
unsigned checks = 0, allocation_cursor = 0x1000000, allocations = 0;
std::array<bool, 14> anchor_valid{};
std::array<rr64::mk64_items::Vec, 14> anchors{};
void check(bool value, const char *label) {
    ++checks;
    if (!value) {
        std::fprintf(stderr, "FAIL: %s\n", label);
        std::exit(1);
    }
}
unsigned word(unsigned char *m, unsigned a) {
    unsigned v;
    std::memcpy(&v, m + a - 0x80000000u, 4);
    return v;
}
void matrix(unsigned char *m, unsigned a, const std::array<float, 16> &values) {
    for (unsigned i = 0; i < 16; ++i) {
        const auto fixed = std::uint32_t(std::int32_t(values[i] * 65536));
        rr64::engine::write_u16(m, a + i * 2, std::uint16_t(fixed >> 16));
        rr64::engine::write_u16(m, a + 32 + i * 2, std::uint16_t(fixed));
    }
}
void check_item_geometry(unsigned char *m, unsigned matrix_address, unsigned vertices,
                         unsigned count, const rr64::mk64_items::Object &object, unsigned part,
                         const rr64::mk64_items::AssetLayout &layout) {
    using namespace rr64::mk64_items;
    std::array<float, 16> transform{};
    for (unsigned i = 0; i < 16; ++i)
        transform[i] = std::bit_cast<float>(word(m, matrix_address + i * 4));
    const unsigned model = shell(object.kind) ? 0u : object.kind == Item::Banana ? 2u : 5u;
    const auto &bounds = layout.meshes[model];
    for (unsigned axis = 0; axis < 3; ++axis) {
        float camera = 0, sector = 0;
        rr64::engine::read_float(m, 0x800D69F8 + axis * 4, camera);
        rr64::engine::read_float(m, 0x800A4FDC + axis * 4, sector);
        float center = transform[12 + axis];
        for (unsigned local = 0; local < 3; ++local)
            center += bounds.center[local] * transform[local * 4 + axis];
        check(std::abs(center - (object.position[axis] - camera - sector) * 10) < .0001f,
              "drawn mesh center equals authoritative collision center");
    }
    std::array<float, 3> low{}, high{};
    low.fill(std::numeric_limits<float>::infinity());
    high.fill(-std::numeric_limits<float>::infinity());
    for (unsigned vertex = 0; vertex < count; ++vertex) {
        std::array<float, 3> point{}, world{};
        for (unsigned axis = 0; axis < 3; ++axis) {
            std::uint16_t value = 0;
            std::memcpy(&value, m + ((vertices + vertex * 16 + axis * 2 - 0x80000000u) ^ 2u), 2);
            point[axis] = std::int16_t(value);
        }
        for (unsigned axis = 0; axis < 3; ++axis)
            for (unsigned local = 0; local < 3; ++local)
                world[axis] += point[local] * transform[local * 4 + axis] / 10;
        // Measure the actual uploaded vertices in the emitted model's rotated axes.
        // This verifies world size without assuming a billboard or cube angle.
        for (unsigned axis = 0; axis < 3; ++axis) {
            float length_squared = 0, coordinate = 0;
            for (unsigned world_axis = 0; world_axis < 3; ++world_axis) {
                const float basis = transform[axis * 4 + world_axis];
                length_squared += basis * basis;
                coordinate += world[world_axis] * basis;
            }
            check(length_squared > 0, "finite nonzero item matrix basis");
            coordinate /= std::sqrt(length_squared);
            low[axis] = std::min(low[axis], coordinate);
            high[axis] = std::max(high[axis], coordinate);
        }
    }
    // Independent emitted-vertex measurement: shells are 75% of Items06's
    // 2.3-unit silhouette; the other original mesh dimensions stay unchanged.
    const std::array<float, 3> expected = shell(object.kind) ? std::array{1.725f, 1.725f, 0.f}
                                          : object.kind == Item::Banana
                                              ? std::array{.7f, .6125f, .7f}
                                          : part == 1 ? std::array{.94285714f, 1.57142857f, 0.f}
                                                      : std::array{1.57142857f, 2.2f, 1.57142857f};
    for (unsigned axis = 0; axis < 3; ++axis) {
        if (std::abs(high[axis] - low[axis] - expected[axis]) >= .0001f)
            std::fprintf(stderr, "kind=%u part=%u axis=%u actual=%f expected=%f\n",
                         unsigned(object.kind), part, axis, high[axis] - low[axis], expected[axis]);
        check(std::abs(high[axis] - low[axis] - expected[axis]) < .0001f,
              "emitted original item vertices use Road Rash fitted dimensions");
    }
}
} // namespace
namespace recomp {
void *alloc(unsigned char *m, std::size_t n) {
    ++allocations;
    const auto at = allocation_cursor;
    allocation_cursor += (unsigned(n) + 15u) & ~15u;
    check(allocation_cursor < 32 * 1024 * 1024, "bounded guest render allocation");
    return m + at;
}
void free(unsigned char *, void *) {}
} // namespace recomp
namespace rr64::mk64_items {
bool render_rider_anchor(unsigned char *, unsigned slot, Vec &out) noexcept {
    if (slot >= anchors.size() || !anchor_valid[slot])
        return false;
    out = anchors[slot];
    return true;
}
}
int main(int argc, char **argv) {
    using namespace rr64::mk64_items;
    using namespace rr64::engine;
    check(argc == 2, "private original item bank argument");
    std::ifstream input(argv[1], std::ios::binary);
    std::vector<std::uint8_t> bank{std::istreambuf_iterator<char>(input), {}};
    AssetLayout layout{};
    std::string error;
    check(parse_item_assets(bank, layout, error), "complete original bank");
    check(layout.textures[13].width == 40 && layout.meshes[5].triangles == 8,
          "double mushroom and original fake cube");
    for (std::size_t length = 0; length < bank.size(); length += 127)
        check(!parse_item_assets(std::span(bank).first(length), layout, error),
              "truncated bank rejected");
    for (unsigned offset : {0u, 8u, 12u, 16u, 20u, 24u, 28u}) {
        auto bad = bank;
        bad[offset] ^= 0x80;
        check(!parse_item_assets(bad, layout, error), "malformed bank header rejected");
    }
    auto trailing = bank;
    trailing.push_back(0);
    check(!parse_item_assets(trailing, layout, error), "trailing bank rejected");
    check(install_render_asset(bank, error), "install authenticated private bank");
    std::vector<unsigned char> memory(32 * 1024 * 1024);
    auto *m = memory.data();
    for (unsigned axis = 0; axis < 3; ++axis) {
        write_float(m, 0x800D69F8 + axis * 4, std::array{101.f, -203.f, 17.f}[axis]);
        write_float(m, 0x800A4FDC + axis * 4, std::array{1000.f, -2000.f, 50.f}[axis]);
    }
    write_u32(m, 0x8009DB88, 4);
    write_u32(m, globals::terrain_map_width, 70);
    write_float(m, 0x8009DBB4, 10);
    write_u32(m, 0x800BC9A0, 0x4650);
    const std::array<float, 16> projection{1, 0, 0, 0, 0, 1, 0, 0, 0, 0, -1, -1, 0, 0, -1, 0};
    const std::array<float, 16> identity{1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};
    for (unsigned view = 0; view < 4; ++view)
        for (unsigned slot = 0; slot < 2; ++slot) {
            write_u16(m, 0x800B73F0 + view * 12 + slot * 2, 1);
            matrix(m, 0x800B6668 + view * 0x180 + slot * 64, projection);
            matrix(m, 0x800B6EE8 + view * 0x180 + slot * 64, identity);
        }
    Snapshot state{};
    state.enabled = 1;
    state.clock = 100;
    state.random = 1;
    state.next_generation = 64;
    for (unsigned n = 0; n < 64; ++n) {
        auto &object = state.objects[n];
        object.generation = n + 1;
        object.born = 90;
        object.expires = 200;
        object.kind = std::array{Item::Banana, Item::GreenShell, Item::RedShell, Item::BlueShell,
                                 Item::FakeBox}[n % 5];
        object.mode = ObjectMode::Resting;
        object.owner = n % 14;
        object.target = no_target;
        object.position = {1101, -2203, 65};
    }
    check(valid(state), "complete fixed-capacity presentation state");
    for (unsigned epoch = 1; epoch < 17; ++epoch) {
        state.clock = 100 + epoch;
        const unsigned gfx = epoch & 1, base = 0x80100000 + gfx * 0x30000;
        write_u32(m, 0x8009CBA4, gfx);
        write_u32(m, globals::actor_render_buffer_slot, gfx);
        write_u32(m, 0x800A1830, epoch);
        write_u32(m, 0x800AC658 + gfx * 4, base);
        write_u32(m, 0x8009CB90, base);
        write_u32(m, 0x800AC650, base + 0x148);
        for (unsigned view = 0; view < 4; ++view) {
            write_u32(m, globals::active_viewport, view);
            const unsigned before = word(m, 0x800AC650);
            check(draw_world(m, state), "64 original models rendered per view");
            check(word(m, 0x800AC650) == before + 40, "bounded display-list bridge");
            const unsigned commands = word(m, before + 28);
            unsigned vertices = 0, triangles = 0, generation = 0, part = 0, current_matrix = 0;
            for (unsigned at = commands; at < commands + 32768; at += 8) {
                const unsigned first = word(m, at), second = word(m, at + 4);
                if ((first >> 24) == 0xDF)
                    break;
                vertices += (first >> 24) == 1;
                triangles += (first >> 24) == 5;
                if (first == 0x6400000c) {
                    generation = (second >> 2) & 0xfffff;
                    part = second & 3;
                }
                if (first == 0x64000030)
                    current_matrix = word(m, at + 12);
                if ((first >> 24) == 1) {
                    check(generation >= 1 && generation <= state.objects.size() &&
                              current_matrix != 0,
                          "uploaded mesh has a bounded object identity and matrix");
                    check_item_geometry(m, current_matrix, second, (first >> 12) & 255,
                                        state.objects[generation - 1], part, layout);
                }
                if (first == 0xE200001F)
                    check((second & 0x10) && (second == 0x00504B50u || (second & 0x20)),
                          "world item depth test; original translucent fake cube retains "
                          "read-only depth");
            }
            check(vertices == 76 && triangles == 224, "all model parts and original topology");
            check(!draw_world(m, state), "same view epoch cannot overwrite queued world commands");
            check(draw_hud_rectangle(m, view, Item(epoch - 1), 8.f + (view & 1) * 160.f,
                           8.f + (view >> 1) * 120.f, 20.f, 16.f),
                  "all original inventory states in separate HUD");
            check(!draw_hud_rectangle(m, view, Item::Star, 0, 0, 40.f, 32.f),
                  "same view epoch cannot overwrite queued HUD");
        }
    }
    for (unsigned views = 1; views <= 4; ++views) {
        write_u32(m, 0x800A1830, 100 + views);
        for (unsigned view = 0; view < views; ++view) {
            const float x = 20.25f + (view & 1) * 160.f;
            const float y = 30.5f + (view >> 1) * 120.f;
            // The native adapter supplies independently scaled, quantized
            // bounds. MK cards must fill this exact square/rectangle, not
            // retain their old separate 40x32 HUD area.
            const float width = views >= 3 ? 16.f : 36.f;
            const float height = views >= 3 ? 16.f : 24.25f;
            const unsigned before = word(m, 0x800AC650);
            check(draw_hud_rectangle(m, view, Item::Boo, x, y, width, height),
                  "original weapon rectangle accepts per-view native scaling");
            const unsigned commands = word(m, before + 28);
            unsigned rectangles = 0, deltas = 0;
            for (unsigned at = commands; at < commands + 1024; at += 8) {
                const unsigned first = word(m, at), second = word(m, at + 4);
                if ((first >> 24) == 0xDF)
                    break;
                if ((first >> 24) == 0xE4) {
                    ++rectangles;
                    check(((second >> 12) & 4095) == unsigned(x * 4) &&
                              (second & 4095) == unsigned(y * 4) &&
                              ((first >> 12) & 4095) == unsigned((x + width) * 4) &&
                              (first & 4095) == unsigned((y + height) * 4),
                          "actual RDP card endpoints equal the native weapon rectangle");
                }
                if (first == 0xF1000000) {
                    ++deltas;
                    check((second >> 16) == unsigned(std::lround(1024.f * 40 / width)) &&
                              (second & 65535) == unsigned(std::lround(1024.f * 32 / height)),
                          "full original card texture fits the same native HUD footprint");
                }
            }
            check(rectangles == 1 && deltas == 1,
                  "one replacement icon and no duplicate rectangle");
        }
    }
    check(allocations == 2, "only double-buffer allocations across all views and frames");
    // The real command producer must handle all struck actors alongside the
    // entire projectile pool, in every native viewport and buffer.
    for (unsigned slot = 0; slot < 14; ++slot) {
        anchors[slot] = {1101, -2203, 62};
        anchor_valid[slot] = true;
        state.riders[slot].shrink_until = 416;
    }
    state.clock = 116;
    const auto saved_objects = state.objects;
    std::array<std::vector<unsigned>, 4> saved_lists{};
    for (unsigned gfx = 0; gfx < 2; ++gfx) {
        const unsigned base = 0x80100000 + gfx * 0x30000;
        write_u32(m, 0x8009CBA4, gfx);
        write_u32(m, globals::actor_render_buffer_slot, gfx);
        write_u32(m, 0x800A1830, 200 + gfx);
        write_u32(m, 0x800AC658 + gfx * 4, base);
        write_u32(m, 0x8009CB90, base);
        write_u32(m, 0x800AC650, base + 0x148);
        std::array<unsigned, 4> list_addresses{};
        for (unsigned view = 0; view < 4; ++view) {
            write_u32(m, globals::active_viewport, view);
            const unsigned before = word(m, 0x800AC650);
            check(draw_world(m, state), "Lightning and full item pool share bounded world pass");
            const auto statistics = render_statistics();
            check(statistics.lightning_strikes == 14 && statistics.objects == 64 &&
                      statistics.triangles == 504 && statistics.command_bytes < 32768,
                  "all fourteen two-layer bolts coexist with maximum projectile commands");
            const unsigned commands = word(m, before + 28);
            list_addresses[view] = commands;
            saved_lists[view].clear();
            unsigned bolt_batches = 0, pushes = 0, pops = 0, no_interpolation = 0;
            bool bolt = false;
            for (unsigned at = commands; at < commands + statistics.command_bytes; at += 8) {
                const unsigned first = word(m, at), second = word(m, at + 4);
                saved_lists[view].push_back(first);
                saved_lists[view].push_back(second);
                if (first == 0x6400000c)
                    bolt = (second & 0xff000000) == 0x57000000;
                if (bolt && first == 1 && second == 0)
                    ++no_interpolation;
                if (bolt && first == 0xe200001f)
                    check(second == 0x00504B50 && !(second & 0x20),
                          "strike respects scene depth without hiding later riders");
                if (bolt && (first >> 24) == 1) {
                    ++bolt_batches;
                    check(((first >> 12) & 255) == 20, "bounded lightning vertex batch");
                    const auto packed = word(m, second);
                    check(std::int16_t(packed >> 16) == (bolt_batches & 1 ? -11 : -4) &&
                              (packed & 65535) == 400,
                          "actual extended-memory lightning vertices are initialized");
                    check((word(m, second + 12) & 255) == (bolt_batches & 1 ? 170 : 255),
                          "outer glow and inner core retain bounded authored alpha");
                }
                pushes += first == 0x64000019;
                pops += first == 0x6400001a;
            }
            check(bolt_batches == 28 && no_interpolation == 14 && pushes == 1 && pops == 1,
                  "one balanced state scope, no strike interpolation or per-tick restart");
            check(word(m, commands + statistics.command_bytes - 16) == 0xe0525464 &&
                      word(m, commands + statistics.command_bytes - 12) == 0x20000000,
                  "strike restores native command/address interpretation");
            check(!draw_world(m, state), "repeated viewport cannot overwrite queued strikes");
        }
        for (unsigned view = 0; view < 4; ++view)
            for (unsigned n = 0; n < saved_lists[view].size(); ++n)
                check(word(m, list_addresses[view] + n * 4) == saved_lists[view][n],
                      "later split views preserve queued strike command lists");
    }
    for (auto &object : state.objects)
        object.kind = Item::FakeBox;
    write_u32(m, 0x800A1830, 299);
    const unsigned worst_before = word(m, 0x800AC650);
    check(draw_world(m, state), "worst-case two-part objects and all struck actors fit together");
    const auto worst = render_statistics();
    check(worst.objects == 64 && worst.lightning_strikes == 14 && worst.triangles == 920 &&
              worst.command_bytes < 32768,
          "maximum two-part geometry retains command headroom");
    const unsigned worst_commands = word(m, worst_before + 28);
    unsigned worst_matrices = 0;
    for (unsigned at = worst_commands; at < worst_commands + worst.command_bytes; at += 8)
        worst_matrices += word(m, at) == 0x64000030;
    check(worst_matrices == 142, "all 142 emitted model matrices fit the reserved arena");
    state.objects = {};
    write_u32(m, globals::active_viewport, 0);
    for (unsigned age = 0; age < 40; ++age) {
        state.clock = 116 + age;
        write_u32(m, 0x800A1830, 300 + age);
        const auto before = word(m, 0x800AC650);
        check(draw_world(m, state) == (age < 15), "unchanged deadline ends strike after fifteen ticks");
        if (age >= 15)
            check(word(m, 0x800AC650) == before, "expired strike emits no list");
    }
    state.clock = 116;
    write_u32(m, 0x800A1830, 400);
    for (auto &rider : state.riders)
        rider.star_until = 200;
    check(!draw_world(m, state), "Star protected riders never receive strike visuals");
    for (auto &rider : state.riders) {
        rider.star_until = 0;
        rider.boo_until = 200;
    }
    check(!draw_world(m, state), "Boo protected riders never receive strike visuals");
    for (auto &rider : state.riders)
        rider.boo_until = 0;
    anchor_valid.fill(false);
    check(!draw_world(m, state), "invalid actor mappings cannot display stray strikes");
    state.enabled = 0;
    anchor_valid.fill(true);
    check(!draw_world(m, state), "disabled item snapshots cannot display strikes");
    state.enabled = 1;
    state.objects = saved_objects;
    check(allocations == 2, "Lightning uses existing two frame arenas without hot allocation");
    check(!install_render_asset(bank, error), "queued asset memory cannot be overwritten");
    const auto before = word(m, 0x800AC650);
    state.objects[0].position[0] = NAN;
    check(!draw_world(m, state) && word(m, 0x800AC650) == before,
          "invalid snapshot has no publication");
    reset_render_session();
    clear_render_asset();
    check(!render_asset_available(), "reset then unload disables art");
    std::printf("MK64 item assets/render: %u checks passed; no GPU or live "
                "appearance claim.\n",
                checks);
}
