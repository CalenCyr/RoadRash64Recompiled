#include "rr64_mk64_item_material.hpp"
#include "rr64_mk64_item_state.hpp"
#include "rr64_mk64_item_lightning.hpp"
#include "rr64_engine_layout.hpp"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <vector>
using namespace rr64::mk64_items;
namespace {
unsigned checks = 0, allocations = 0, cursor = 0x1000000;
RiderState effect;
unsigned effect_clock = 100;
void check(bool b, const char *s) {
    ++checks;
    if (!b) { std::fprintf(stderr, "FAIL %s\n", s); std::exit(1); }
}
}
namespace recomp {
void *alloc(unsigned char *m, std::size_t size) {
    ++allocations;
    const auto start = cursor;
    cursor += (unsigned(size) + 15) & ~15u;
    check(cursor < 32 * 1024 * 1024, "bounded allocation");
    return m + start;
}
void free(unsigned char *, void *) {}
}
namespace rr64::mk64_items {
bool render_effect(unsigned char *, unsigned, RiderState &out, unsigned &clock) noexcept {
    out = effect; clock = effect_clock; return true;
}
}
int main(int argc, char **argv) {
    // Traced command grammar only: no game mesh/texture/image data.
    std::array<MaterialCommand, 6> source{{{0xda380002, 0x05000000},
        {0xfc129bff, 0xfffdf638}, {0xe200001c, 0xc8112230},
        {0x0100600c, 0x06000000}, {0x06000204, 0x0006080a}, {0xdf000000, 0}}};
    const auto original = source;
    std::array<MaterialCommand, 1040> output{};
    auto n = compile_material(source, output, 0xffffff5a, true);
    check(n == source.size() + 15, "balanced wrapper size");
    check(source == original, "shared source remains unchanged");
    check(output[6] == MaterialCommand{0xfb000000, 0xffffff5a}, "ghost environment alpha");
    check(output[10] == MaterialCommand{0xe200001c, 0xc81049d8}, "ghost blend retains native fog");
    check(output[8] == source[0] && output[11] == source[3] && output[12] == source[4],
          "matrix, vertices and triangles unchanged");
    check(output[n-1] == MaterialCommand{0xdf000000, 0}, "ends scoped list");
    for (unsigned size = 0; size < source.size() + 15; ++size)
        check(!compile_material(source, std::span(output).first(size), 0xffffffff, true), "undersized output refused");
    n = compile_material(source, output, 0xff5050ff, false);
    check(output[10] == source[2], "Star retains opaque render mode");
    std::vector<MaterialCommand> bright(source.begin(), source.end());
    bright.insert(bright.begin() + 3, {{0xfa000000, 0xffffffff}, {0xfb000000, 0xffffffff}});
    n = compile_material(bright, output, 0x080810ff, false, true);
    check(n == bright.size() + 15, "strike preserves the bounded native material grammar");
    unsigned additions = 0, environments = 0;
    for (unsigned i = 0; i < n; ++i) {
        if ((output[i].first >> 24) == 0xfa) {
            ++additions;
            check(output[i].second == 0, "damage flash primitive additions cannot cancel dark strike");
        }
        if (output[i].first == 0xfb000000) {
            ++environments;
            check(output[i].second == 0x080810ff, "every struck native material receives actor-local dark tint");
        }
    }
    check(additions == 2 && environments == 2, "initial and embedded colors covered");
    check(output[10] == source[2], "dark flash preserves original depth/fog/blend mode");
    effect.shrink_until = 400;
    for (unsigned age = 0; age < 330; ++age) {
        const auto visual = lightning_visual(effect, 100 + age);
        check(visual.active == (age < 30), "one-second flash deadline never restarts while shrunk");
        check((visual.bolt_alpha != 0) == (age < 15), "brief strike and flash have independent windows");
        if (age < 30) {
            constexpr unsigned expected[] = {0x080810, 0x505078, 0x909040};
            check(visual.tint == expected[(age % 6) / 2], "donor-style six-update rider shade cadence");
        }
    }
    check(!lightning_visual(effect, 99).active, "clock rollback before onset cannot replay old strike");
    effect.star_until = 200;
    check(!lightning_visual(effect, 100).active, "Star takes precedence over stale shrink deadline");
    effect.star_until = 0;
    effect.boo_until = 200;
    check(!lightning_visual(effect, 100).active, "Boo takes precedence over stale shrink deadline");
    effect = {};
    for (unsigned op : {0xde, 0xdf, 0xdd, 0x04, 0x64, 0xe0}) {
        auto bad = source; bad[4].first = op << 24;
        check(!compile_material(bad, output, 0xffffffff, true), "branch/extended list refused");
    }
    auto bad = source; bad[1].second ^= 1;
    check(!compile_material(bad, output, 0xffffffff, true), "untraced material refused");
    std::vector<unsigned char> memory(32*1024*1024);
    auto *m = memory.data();
    const unsigned dl = 0x80600000;
    for (unsigned i = 0; i < source.size(); ++i) {
        rr64::engine::write_u32(m, dl+i*8, source[i].first);
        rr64::engine::write_u32(m, dl+i*8+4, source[i].second);
    }
    effect.boo_until = 200;
    for (unsigned epoch = 1; epoch <= 6; ++epoch) {
        rr64::engine::write_u32(m, 0x8009cba4, epoch & 1);
        rr64::engine::write_u32(m, 0x800a1830, epoch);
        unsigned prior = 0;
        for (unsigned actor = 0; actor < 112; ++actor) {
            const auto p = rr64_mk64_items_actor_list(m, 1, dl);
            check(p != dl && p > prior, "each actor/view has persistent command copy");
            prior = p;
        }
    }
    check(allocations == 2, "no hot-path allocation after two native buffers");
    effect = {};
    check(rr64_mk64_items_actor_list(m, 1, dl) == dl, "expired effect uses original model");
    effect.shrink_until = 400;
    effect_clock = 100;
    const auto dark_list = rr64_mk64_items_actor_list(m, 1, dl);
    check(dark_list != dl, "actual actor material entry draws initial Lightning darkening");
    unsigned tint = 0;
    // Extended clone memory intentionally exceeds the native 8 MiB range.
    std::memcpy(&tint, m + dark_list - 0x80000000u + 6 * 8 + 4, 4);
    check(tint == 0x080810ff, "actual actor entry publishes the initial dark tint");
    effect_clock = 130;
    check(rr64_mk64_items_actor_list(m, 1, dl) == dl, "same still-shrunk actor restores normal native material");
    effect.shrink_until = 430;
    check(rr64_mk64_items_actor_list(m, 1, dl) != dl, "a genuinely new Lightning deadline restarts once");
    effect = {};
    if (argc == 2) {
        std::ifstream input(argv[1], std::ios::binary);
        input.read(reinterpret_cast<char *>(m), 0x800000);
        check(input.gcount() == 0x800000, "private initialized memory fixture");
        unsigned found = 0;
        for (unsigned p = 0x80200000; p < 0x807fff00; p += 4) {
            unsigned type, entity, actor;
            rr64::engine::read_u32(m, p, type);
            if (type != 1 && type != 2) continue;
            rr64::engine::read_u32(m, p+4, entity);
            if (!rr64::engine::read_u32(m, entity+4, actor) ||
                actor < 0x800d8570 || (actor-0x800d8570)%0x118 || (actor-0x800d8570)/0x118 >= 14) continue;
            for (unsigned lod = 0; lod < 3; ++lod) {
                unsigned list; rr64::engine::read_u32(m, p+0x50+lod*4, list);
                if (!rr64::engine::valid_guest_range(list, 8) || (list&7)) continue;
                std::array<MaterialCommand, 1024> commands{};
                unsigned count = 0;
                for (; count < commands.size(); ++count) {
                    if (!rr64::engine::read_u32(m, list+count*8, commands[count].first) ||
                        !rr64::engine::read_u32(m, list+count*8+4, commands[count].second)) break;
                    if (commands[count].first == 0xdf000000) { ++count; break; }
                }
                if (!count || commands[count-1] != MaterialCommand{0xdf000000, 0}) continue;
                check(compile_material(std::span(commands).first(count), output, 0xffffff5a, true) != 0,
                      "actual initialized bike/rider LOD material accepted");
                const unsigned dark_count = compile_material(std::span(commands).first(count),
                                                              output, 0x080810ff, false, true);
                check(dark_count != 0, "actual initialized bike/rider LOD accepts struck silhouette");
                for (unsigned command = 0; command < dark_count; ++command)
                    if ((output[command].first >> 24) == 0xfa)
                        check(output[command].second == 0, "actual model additive color is suppressed during strike");
                ++found;
            }
        }
        check(found >= 30, "multiple actual actor identities and LODs covered");
    }
    std::printf("MK64 material: %u checks passed\n", checks);
}
