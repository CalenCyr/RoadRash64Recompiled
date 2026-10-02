#include "rr64_mk64_item_material.hpp"
#include "rr64_mk64_item_state.hpp"
#include "rr64_engine_layout.hpp"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

extern "C" void fixture_material_call_bike(unsigned char *, recomp_context *);
extern "C" void fixture_material_call_rider(unsigned char *, recomp_context *);
extern "C" unsigned fixture_material_resolve(unsigned, const unsigned *, bool);

// This fixture continues testing the unchanged material effect fallback for
// stock actors. Rider-skin rendering has its own native-caller fixture target.
extern "C" unsigned rr64_rider_skin_actor_call(unsigned char* m, unsigned node,
                                               unsigned list, unsigned call, unsigned) {
    return rr64_mk64_items_actor_call(m,node,list,call);
}

namespace {
using rr64::mk64_items::MaterialCommand;
unsigned checks = 0, allocations = 0, allocation_cursor = 0x06aa8330;
rr64::mk64_items::RiderState effect;
bool highlight = false;
unsigned highlight_weapon_calls = 0, expected_highlight_cursor = 0;
constexpr unsigned memory_size = 128 * 1024 * 1024;
constexpr unsigned original = 0x80626cd8, model_table = 0x800d0000;
constexpr unsigned node = 0x80210000, entity = 0x80220000;

void check(bool value, const char *message) {
    ++checks;
    if (!value) {
        std::fprintf(stderr, "FAIL %s (check %u)\n", message, checks);
        std::exit(1);
    }
}
unsigned word(unsigned char *m, unsigned address) {
    unsigned result;
    const auto offset = address & 0x7fffffff;
    check(offset <= memory_size - 4, "fixture read bounded");
    std::memcpy(&result, m + offset, sizeof(result));
    return result;
}
void put(unsigned char *m, unsigned address, unsigned value) {
    const auto offset = address & 0x7fffffff;
    check(offset <= memory_size - 4, "fixture write bounded");
    std::memcpy(m + offset, &value, sizeof(value));
}
void command(unsigned char *m, unsigned address, MaterialCommand value) {
    put(m, address, value.first);
    put(m, address + 4, value.second);
}

// This bounded oracle follows only command control flow/state. Address
// resolution is compiled from RT64's actual implementation by the extractor.
// It does not claim to exercise rasterization, vertex transforms or GPU work.
struct Trace {
    bool terminated = false, extended = false;
    unsigned opcode = 0, commands = 0, material_calls = 0, native_calls = 0;
    unsigned matrices = 0, vertices = 0;
    std::array<int, 4> stacks{};
};
Trace interpret(unsigned char *m, unsigned start, unsigned clone) {
    Trace result;
    std::array<unsigned, 16> segments{};
    segments[5] = 0x80626cd8;
    segments[6] = 0x800b73f2; // Exact hang-state segment, not a zero-table fixture.
    segments[7] = 0x802585c0;
    segments[8] = 0x80259000;
    std::vector<unsigned> returns;
    unsigned pc = start & 0x7fffffff;
    for (; result.commands < 4096; ++result.commands) {
        if (pc > memory_size - 8 || (pc & 7)) return result;
        const auto a = word(m, pc), b = word(m, pc + 4);
        const auto op = a >> 24;
        if (a == 0xe0525464) {
            if ((b >> 28) == 1) result.opcode = b & 0xff;
            else if ((b >> 28) == 2) result.opcode = 0;
            else return result;
        } else if (result.opcode && op == result.opcode) {
            const unsigned code = a & 0xffffff;
            if (code == 0x2c) result.extended = (b & 1) != 0;
            constexpr std::array<unsigned, 4> pushes{0x19, 0x1b, 0x1f, 0x27};
            for (unsigned i = 0; i < pushes.size(); ++i) {
                if (code == pushes[i]) ++result.stacks[i];
                if (code == pushes[i] + 1) --result.stacks[i];
            }
        } else if (op == 0xde) {
            const auto target = fixture_material_resolve(b, segments.data(), result.extended);
            if (b == clone) {
                ++result.material_calls;
                if (target != (clone & 0x7fffffff)) return result;
            } else if (b == 0x08000000) {
                ++result.native_calls;
                if (result.extended || result.opcode || target != 0x259000) return result;
            }
            if (!(a & 0x10000)) returns.push_back(pc + 8);
            pc = target;
            continue;
        } else if (op == 0xdf) {
            if (returns.empty()) {
                result.terminated = true;
                return result;
            }
            pc = returns.back();
            returns.pop_back();
            continue;
        } else if (op == 0xda) {
            ++result.matrices;
            if (fixture_material_resolve(b, segments.data(), result.extended) != 0x626cd8)
                return result;
        } else if (op == 0x01) {
            ++result.vertices;
            if (fixture_material_resolve(b, segments.data(), result.extended) != 0x2585c0)
                return result;
        }
        pc += 8;
    }
    return result;
}
}

namespace recomp {
void *alloc(unsigned char *m, std::size_t size) {
    ++allocations;
    const auto start = allocation_cursor;
    allocation_cursor += (unsigned(size) + 15) & ~15u;
    check(allocation_cursor < memory_size, "material arena bounded above 16 MiB");
    return m + start;
}
void free(unsigned char *, void *) {}
}
namespace rr64::mk64_items {
bool render_effect(unsigned char *, unsigned, RiderState &out, unsigned &clock) noexcept {
    out = effect;
    clock = 100;
    return true;
}
}
extern "C" int rr64_highlights_presenting() { return highlight; }
extern "C" void rr64_highlights_weapon_draw(unsigned char *m, recomp_context *context, unsigned actor) {
    ++highlight_weapon_calls;
    check(highlight && actor == node, "highlight continuation keeps native actor ownership");
    check(word(m, 0x800ac650) == expected_highlight_cursor &&
          unsigned(context->r5) == expected_highlight_cursor,
          "highlight weapon continuation sees moved native cursor");
}

int main() {
    std::vector<unsigned char> memory(memory_size);
    auto *m = memory.data();
    // Traced native grammar. No game mesh/texture/ROM payload is embedded.
    constexpr std::array<MaterialCommand, 6> source{{
        {0xda380002, 0x05000000}, {0xfc129bff, 0xfffdf638},
        {0xe200001c, 0xc8112230}, {0x0100600c, 0x07000000},
        {0x06000204, 0x0006080a}, {0xdf000000, 0}}};
    for (unsigned i = 0; i < source.size(); ++i) command(m, original + i * 8, source[i]);
    put(m, model_table, original);
    put(m, node + 4, entity);
    put(m, entity + 0x5bc, 0); // Exercise rider continuation with no held weapon.
    command(m, 0x80259000, {0xdf000000, 0});

    std::array<unsigned, 16> captured_segments{};
    captured_segments[6] = 0x800b73f2;
    check(fixture_material_resolve(0x86aa8330, captured_segments.data(), false) == 0x00b5f720,
          "saved hang target resolves into empty memory under native addressing");
    check(fixture_material_resolve(0x86aa8330, captured_segments.data(), true) == 0x06aa8330,
          "extended addressing reaches saved hang material target");

    unsigned scenarios = 0;
    for (unsigned views = 1; views <= 4; ++views)
    for (unsigned gfx = 0; gfx < 2; ++gfx)
    for (unsigned kind = 0; kind < 4; ++kind) {
        effect = {};
        if (kind == 3) effect.shrink_until = 400;
        else {
            if (kind != 1) effect.star_until = 200;
            if (kind != 0) effect.boo_until = 200;
        }
        const unsigned base = 0x80110000 + gfx * 0x30000;
        put(m, 0x8009cba4, gfx);
        put(m, 0x800ac658 + gfx * 4, base);
        put(m, 0x8009cb90, base);
        put(m, 0x800bc9a0, views == 1 ? 0x4650 : 0x36b0);
        put(m, 0x800a1830, 1000 + views * 100 + gfx * 10 + kind);
        put(m, 0x8009db88, views);
        for (unsigned view = 0; view < views; ++view)
        for (auto caller : {fixture_material_call_bike, fixture_material_call_rider}) {
            put(m, rr64::engine::globals::active_viewport, view);
            const unsigned call = base + 0x148 + view * 0x100;
            put(m, 0x800ac650, call);
            for (unsigned i = 0; i < 8; ++i) command(m, call + i * 8, {0xcdcdcdcd, 0xcdcdcdcd});
            recomp_context context{};
            context.r19 = int32_t(node);
            context.r23 = int32_t(0x800b0000);
            context.r30 = int32_t(model_table);
            caller(m, &context);
            const unsigned next = word(m, 0x800ac650);
            const unsigned clone = word(m, call + (word(m, call) == 0xde000000 ? 4 : 20));
            check(word(m, next) == 0xcdcdcdcd, "following main command space unmodified");
            command(m, next, {0xde000000, 0x08000000});
            command(m, next + 8, {0xdf000000, 0});
            const auto trace = interpret(m, call, clone);
            check(trace.terminated && trace.material_calls == 1 && trace.native_calls == 1,
                  "material returns and following native segmented actor terminates");
            check(next == call + 24, "native cursor includes complete extended-call prefix");
            check(unsigned(context.r6) == call + 16, "native w1 store moved with DE command");
            check(word(m, call) == 0xe0525464 && word(m, call + 8) == 0x6400002c &&
                  word(m, call + 12) == 1 && word(m, call + 16) == 0xde000000,
                  "extended addressing enabled before native branch");
            check(clone >= 0x81000000, "clone exercises addressing above 16 MiB");
            if (kind == 3)
                check(word(m, clone + 6 * 8) == 0xfb000000 &&
                          word(m, clone + 6 * 8 + 4) == 0x080810ff,
                      "actual native actor call selects Lightning silhouette material");
            check(!trace.extended && !trace.opcode && trace.stacks == std::array<int, 4>{},
                  "native addressing and all material stacks restored");
            check(trace.matrices == 1 && trace.vertices == 1,
                  "native matrix and vertex references remain valid");
            ++scenarios;
        }
    }
    check(allocations == 2, "all views reuse exactly two graphics-buffer arenas");
    effect = {};
    const unsigned base = 0x80140000, call = base + 0x148;
    put(m, 0x800ac650, call);
    recomp_context context{};
    context.r19 = int32_t(node);
    context.r23 = int32_t(0x800b0000);
    context.r30 = int32_t(model_table);
    fixture_material_call_bike(m, &context);
    check(word(m, call) == 0xde000000 && word(m, call + 4) == original &&
          word(m, 0x800ac650) == call + 8, "expired material preserves ordinary native call");

    // Refused calls must leave both the caller's reserved command and global
    // cursor untouched. In particular, no partial extended prefix is allowed.
    effect.boo_until = 200;
    constexpr unsigned native_count = 0x36b0;
    for (unsigned refusal = 0; refusal < 4; ++refusal) {
        const unsigned reserved = refusal < 2 ? base + 0x140 + native_count * 8 -
                                                   (refusal == 0 ? 16 : 1024) : call;
        const unsigned cursor = refusal == 2 ? reserved + 16 : reserved;
        put(m, 0x800ac650, cursor);
        for (unsigned i = 0; i < 6; ++i) put(m, reserved + i * 4, 0xa5a50000 + i);
        put(m, reserved, refusal == 3 ? 0xdf000000 : 0xde000000);
        std::array<unsigned, 6> before{};
        for (unsigned i = 0; i < before.size(); ++i) before[i] = word(m, reserved + i * 4);
        const auto prior_allocations = allocations;
        check(rr64_mk64_items_actor_call(m, node, original, reserved) == original,
              "unsafe main-DL call returns original material");
        check(word(m, 0x800ac650) == cursor && allocations == prior_allocations,
              "refused call preserves cursor and allocation count");
        for (unsigned i = 0; i < before.size(); ++i)
            check(word(m, reserved + i * 4) == before[i], "refused call leaves all command words untouched");
    }

    // The rider branch also exits through the replay-weapon hook. Exercise its
    // actual generated cursor increment with and without a material extension.
    highlight = true;
    for (bool active : {false, true}) {
        effect = {};
        if (active) effect.boo_until = 200;
        put(m, 0x800ac650, call);
        context = {};
        context.r19 = int32_t(node);
        context.r23 = int32_t(0x800b0000);
        context.r30 = int32_t(model_table);
        expected_highlight_cursor = call + (active ? 24 : 8);
        fixture_material_call_rider(m, &context);
        check(word(m, 0x800ac650) == expected_highlight_cursor,
              "highlight rider caller finishes after its complete native call");
    }
    check(highlight_weapon_calls == 2, "both highlight cursor paths executed");
    highlight = false;
    for (unsigned i = 0; i < source.size(); ++i)
        check(word(m, original + i * 8) == source[i].first &&
              word(m, original + i * 8 + 4) == source[i].second, "shared native material unchanged");
    std::printf("MK64 native material call: %u checks passed (%u caller/view/effect cases)\n", checks, scenarios);
}
