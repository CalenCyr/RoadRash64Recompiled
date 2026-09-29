#include "rr64_mk64_item_material.hpp"
#include "rr64_mk64_items.hpp"
#include "rr64_mk64_item_lightning.hpp"
#include "rr64_engine_layout.hpp"
#include "librecomp/addresses.hpp"
#include <algorithm>

namespace rr64::mk64_items {
unsigned compile_material(std::span<const MaterialCommand> source,
                          std::span<MaterialCommand> output, unsigned rgba, bool ghost,
                          bool dark) noexcept {
    if (source.empty() || source.size() > 1024 || output.size() < source.size() + 15 ||
        source.back() != MaterialCommand{0xdf000000, 0})
        return 0;
    bool material = false, render = false;
    for (unsigned i = 0; i + 1 < source.size(); ++i) {
        const auto &c = source[i];
        const unsigned op = c.first >> 24;
        // Native compiled actor models are flat lists. Never follow arbitrary
        // nested/branch pointers or patch shared lists in place.
        if (op == 0xde || op == 0xdf || op == 0xdd || op == 0x04 || op == 0x64 || op == 0xe0)
            return 0;
        if (op == 0xfc) {
            if (c != MaterialCommand{0xfc129bff, 0xfffdf638})
                return 0;
            material = true;
        }
        if (c.first == 0xe200001c)
            render = true;
    }
    if (!material || !render)
        return 0;
    unsigned n = 0;
    const auto emit = [&](unsigned a, unsigned b) { output[n++] = {a, b}; };
    emit(0xe7000000, 0);
    emit(0xe0525464, 0x10000064); // RT64 extended command scope.
    for (unsigned op : {0x19u, 0x1bu, 0x1fu, 0x27u})
        emit(0x64000000 | op, 0);
    emit(0xfb000000, rgba);
    emit(0xfa000000, 0); // No inherited damage-flash addition/opaque alpha.
    for (unsigned i = 0; i + 1 < source.size(); ++i) {
        auto c = source[i];
        if (c.first == 0xfb000000)
            c.second = rgba;
        if ((c.first >> 24) == 0xfa) {
            if (dark)
                c.second = 0; // Native damage additions must not brighten the strike silhouette.
            else if (ghost)
                c.second &= 0xffffff00;
        }
        if (c.first == 0xe200001c && ghost) {
            // Retain native cycle-one fog. Cycle two blends input alpha over
            // the framebuffer, tests depth, and does not write opaque depth.
            c.second = (c.second & 0xcccc0000) | 0x001049d8;
        }
        emit(c.first, c.second);
    }
    emit(0xe7000000, 0);
    for (unsigned op : {0x28u, 0x20u, 0x1cu, 0x1au})
        emit(0x64000000 | op, 0);
    emit(0x6400002c, 0); // Restore native segmented addressing before the next actor.
    emit(0xe0525464, 0x20000000);
    emit(0xdf000000, 0);
    return n;
}

namespace {
using namespace engine;
constexpr unsigned frame_capacity = 1024u * 1024u, guard = 0x4954454d;
struct Frame {
    unsigned base = 0, epoch = 0, used = 0;
};
struct State {
    unsigned char *memory = nullptr;
    std::array<Frame, 2> frames{};
};
State state;
unsigned word(unsigned char *rdram, unsigned a) {
    return unsigned(MEM_W(0, guest_address(a)));
}
void put(unsigned char *rdram, unsigned a, unsigned v) {
    MEM_W(0, guest_address(a)) = v;
}
bool prepare(unsigned char *m, unsigned gfx, unsigned epoch) {
    if (state.memory && state.memory != m)
        return false;
    auto &f = state.frames[gfx];
    if (!f.base) {
        auto *p = static_cast<unsigned char *>(recomp::alloc(m, frame_capacity + 16));
        if (!p)
            return false;
        const auto offset = p - m;
        if (offset < 0x800000 || (offset & 7) ||
            std::uint64_t(offset) + frame_capacity + 16 > recomp::mem_size) {
            recomp::free(m, p);
            return false;
        }
        f.base = kRdramBegin + unsigned(offset);
        for (unsigned i = 0; i < 4; ++i)
            put(m, f.base + frame_capacity + i * 4, guard);
        state.memory = m;
    }
    for (unsigned i = 0; i < 4; ++i)
        if (word(m, f.base + frame_capacity + i * 4) != guard)
            return false;
    if (f.epoch != epoch) {
        f.epoch = epoch;
        f.used = 0;
    }
    return true;
}
}
void reset_material_session() noexcept {
    state = {};
}
}

extern "C" unsigned rr64_mk64_items_actor_list(unsigned char *m, unsigned node, unsigned original) {
    using namespace rr64::mk64_items;
    using namespace rr64::engine;
    RiderState effect{};
    unsigned clock = 0, gfx = 0, epoch = 0;
    if (!m || !render_effect(m, node, effect, clock) || (original & 7) ||
        !valid_guest_range(original, 8) || !read_u32(m, 0x8009cba4, gfx) || gfx > 1 ||
        !read_u32(m, 0x800a1830, epoch))
        return original;
    const auto lightning = lightning_visual(effect, clock);
    if (effect.star_until <= clock && effect.boo_until <= clock && !lightning.active)
        return original;
    std::array<MaterialCommand, 1024> source{};
    std::array<MaterialCommand, 1040> transformed{};
    unsigned count = 0;
    for (; count < source.size(); ++count) {
        const unsigned a = original + count * 8;
        if (!valid_guest_range(a, 8))
            return original;
        source[count] = {word(m, a), word(m, a + 4)};
        if (source[count].first == 0xdf000000) {
            ++count;
            break;
        }
    }
    constexpr std::array<unsigned, 6> colors{0xff5050, 0xffdf50, 0x70ff70,
                                             0x50dfff, 0x8080ff, 0xff70df};
    const bool ghost = effect.boo_until > clock;
    const unsigned color = effect.star_until > clock ? colors[(clock / 3) % colors.size()]
                           : lightning.active        ? lightning.tint
                                                     : 0xffffff;
    const unsigned n =
        compile_material(std::span(source).first(count), transformed,
                         (color << 8) | (ghost ? 90u : 255u), ghost, lightning.active);
    if (!n || !prepare(m, gfx, epoch))
        return original;
    auto &f = state.frames[gfx];
    if (n * 8 > frame_capacity - f.used)
        return original;
    const unsigned result = f.base + f.used;
    for (unsigned i = 0; i < n; ++i) {
        put(m, result + i * 8, transformed[i].first);
        put(m, result + i * 8 + 4, transformed[i].second);
    }
    f.used += n * 8;
    return result;
}

extern "C" unsigned rr64_mk64_items_actor_call(unsigned char *m, unsigned node, unsigned original,
                                               unsigned call) {
    using namespace rr64::mk64_items;
    using namespace rr64::engine;
    unsigned gfx = 0, base = 0, active = 0, count = 0, cursor = 0, opcode = 0;
    // The native caller has reserved its DE command, but its two branches
    // advance the main cursor on opposite sides of this hook. Validate both.
    if (!m || !read_u32(m, 0x8009cba4, gfx) || gfx > 1 ||
        !read_u32(m, 0x800ac658 + gfx * 4, base) || !read_u32(m, 0x8009cb90, active) ||
        active != base || !read_u32(m, 0x800bc9a0, count) || (count != 0x4650 && count != 0x36b0) ||
        !read_u32(m, 0x800ac650, cursor) || (cursor != call && cursor != call + 8) ||
        !valid_guest_range(base, 0x140 + count * 8) || call < base + 0x148 ||
        call > base + 0x140 + count * 8 - 1064 || (call & 7) || !read_u32(m, call, opcode) ||
        opcode != 0xde000000)
        return original;
    const unsigned list = rr64_mk64_items_actor_list(m, node, original);
    if (list == original)
        return original;
    // Enable extended addressing BEFORE branching to the clone. Enabling it
    // inside that clone is too late: the native DE would truncate its address.
    put(m, call, 0xe0525464);
    put(m, call + 4, 0x10000064);
    put(m, call + 8, 0x6400002c);
    put(m, call + 12, 1);
    put(m, call + 16, 0xde000000);
    put(m, call + 20, list);
    return list;
}
