// Headless regression for the stock sprite emitter. ROM data stays in memory.
#include "recomp.h"
#include <algorithm>
#include <bit>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iterator>
#include <vector>
extern "C" void func_8001CFB8(unsigned char*, recomp_context*);
extern "C" void original_8001CFB8(unsigned char*, recomp_context*);
extern "C" void do_break(uint32_t) { std::abort(); }
extern "C" void switch_error(const char*, uint32_t, uint32_t) { std::abort(); }
static unsigned checks = 0;
static void require(bool value, const char* reason) {
    ++checks;
    if (!value) { std::fprintf(stderr, "%s\n", reason); std::exit(1); }
}
static unsigned word(unsigned char* rdram, unsigned a) { return MEM_W(0, S32(a)); }
static unsigned half(unsigned char* rdram, unsigned a) { return MEM_HU(0, S32(a)); }
static void put(unsigned char* rdram, unsigned a, unsigned v) { MEM_W(0, S32(a)) = v; }
static void pf(unsigned char* m, unsigned a, float f) { put(m, a, std::bit_cast<unsigned>(f)); }
int main(int argc, char** argv) {
    if (argc < 2 || argc > 3) return 2;
    std::ifstream file(argv[1], std::ios::binary);
    std::vector<unsigned char> rom((std::istreambuf_iterator<char>(file)), {});
    require(rom.size() == 0x2000000 && rom[0] == 0x80 && rom[1] == 0x37, "Requires supported big-endian US ROM");
    std::vector<unsigned char> memory(8 * 1024 * 1024);
    auto* rdram = memory.data();
    auto load = [&](unsigned guest, unsigned offset, unsigned size) {
        require(offset + size <= rom.size() && guest - 0x80000000u + size <= memory.size(), "ROM load bounds");
        for (unsigned i = 0; i < size; ++i) MEM_B(0, S32(guest + i)) = rom[offset + i];
    };
    const unsigned bank = 0x80100000, tex = 0x80200000, dl = 0x80600000, style = 0x80700000;
    load(0x80000400, 0x1000, 0xA7800 - 0x400);
    load(bank, 0x1E08690, 0x1E0A7B0 - 0x1E08690);
    load(tex, 0x1E0A7B0, 0x2000000 - 0x1E0A7B0);
    const unsigned desc = bank + word(rdram, bank) + 0xC2 * 24;
    const unsigned pixels = tex + word(rdram, desc);
    const unsigned palette = tex + word(rdram, bank + word(rdram, bank + 8) + half(rdram, desc + 0x12) * 4);
    require(half(rdram, desc + 8) == 16 && half(rdram, desc + 10) == 16 && half(rdram, desc + 14) == 4, "Expected stock route dot descriptor");

    // Emulate the documented TMEM odd-row word swap for this real CI4 asset.
    // x=16 incorrectly crosses into the next row; x=15 is its clear border.
    unsigned char tmem[136]{};
    for (unsigned y = 0; y < 16; ++y) for (unsigned x = 0; x < 8; ++x)
        tmem[y * 8 + (x ^ ((y & 1) ? 4 : 0))] = MEM_BU(0, S32(pixels + y * 8 + x));
    unsigned leakedRows = 0;
    for (unsigned y = 0; y < 16; ++y) for (unsigned x : {15u, 16u}) {
        const unsigned p = y * 8 + x / 2, row = p / 8 * 8;
        const unsigned address = row + ((((p - row) / 4) ^ (y & 1)) * 4) + (p & 3);
        const unsigned index = (tmem[address] >> ((x & 1) ? 0 : 4)) & 15;
        const bool opaque = (half(rdram, palette + index * 2) & 1) != 0;
        if (x == 15) require(!opaque, "Stock dot border must be transparent");
        else leakedRows += opaque;
    }
    require(leakedRows == 12, "Real dot reproduces extra opaque TMEM column");

    auto emit = [&](bool original, float x, float y, float scale) {
        for (unsigned a = 0x8009DCB0; a < 0x8009DCC0; a += 4) put(rdram, a, 0);
        MEM_H(0, S32(0x8009DCBC)) = -1;
        put(rdram, 0x800AC650, dl);
        pf(rdram, style, x); pf(rdram, style + 4, y); pf(rdram, style + 8, .5f);
        pf(rdram, style + 12, scale); pf(rdram, style + 16, scale);
        for (unsigned a = 20; a <= 56; a += 4) pf(rdram, style + a, 1.f);
        MEM_H(0, S32(style + 0x3C)) = 0x18; MEM_H(0, S32(style + 0x3E)) = 0;
        recomp_context c{}; c.f_odd = &c.f0.u32h; c.r29 = S32(0x807F0000);
        c.r4 = S32(bank); c.r5 = S32(pixels); c.r6 = S32(palette); c.r7 = S32(desc);
        put(rdram, unsigned(c.r29) + 0x10, style);
        (original ? original_8001CFB8 : func_8001CFB8)(rdram, &c);
        const unsigned end = word(rdram, 0x800AC650);
        require(end >= dl && end - dl < 16384, "Bounded display-list emission");
        std::vector<unsigned> commands;
        for (unsigned a = dl; a < end; a += 4) commands.push_back(word(rdram, a));
        return commands;
    };
    unsigned cases = 0;
    for (unsigned bits : {4u, 8u}) for (unsigned flags : {4u, 1u})
    for (unsigned size : {16u, 80u}) for (float scale : {.36f, .5f, 1.f})
    for (unsigned ix = 0; ix < 8; ++ix) for (unsigned iy = 0; iy < 8; ++iy) {
        MEM_H(0, S32(desc + 8)) = size; MEM_H(0, S32(desc + 10)) = size;
        MEM_H(0, S32(desc + 12)) = size; MEM_H(0, S32(desc + 14)) = bits;
        MEM_H(0, S32(desc + 16)) = flags | (size > 64 ? 2 : 0);
        put(rdram, desc + 4, 0x3000);
        for (unsigned i = 0; i < 16; ++i) put(rdram, bank + 0x3000 + i * 4, i * 2048);
        const auto old = emit(true, 160.f + ix / 8.f, 120.f + iy / 8.f, scale);
        const auto now = emit(argc == 3, 160.f + ix / 8.f, 120.f + iy / 8.f, scale);
        require(old.size() == now.size(), "Command count remains unchanged");
        unsigned lastTile = 0, stripe = 0;
        std::vector<bool> changed(now.size(), false);
        for (unsigned i = 0; i < now.size(); i += 2) {
            if ((now[i] >> 24) == 0xF2) lastTile = i;
            if ((now[i] >> 24) != 0xE4) continue;
            const unsigned rows = bits == 4 ? 64 : 32, columns = (size + 63) / 64;
            const unsigned width = std::min(64u, size - (stripe % columns) * 64);
            const unsigned height = std::min(rows, size - (stripe / columns) * rows);
            const unsigned w1 = now[lastTile + 1];
            if (((w1 >> 12) & 4095) != (width - 1) * 4 || (w1 & 4095) != (height - 1) * 4)
                std::fprintf(stderr, "bits=%u flags=%u size=%u stripe=%u actual=%08X expected=%ux%u\n", bits, flags, size, stripe, w1, width, height);
            require(((w1 >> 12) & 4095) == (width - 1) * 4 && (w1 & 4095) == (height - 1) * 4,
                "Sprite tile must clamp to last valid texel of each stripe");
            if (bits == 4) {
                require(old[lastTile + 1] - w1 == 0x4004, "Only 4-bit inclusive tile extents change");
                changed[lastTile + 1] = true;
            }
            ++stripe;
        }
        require(stripe > 0, "Sprite emits rectangles");
        for (unsigned i = 0; i < now.size(); ++i)
            require(changed[i] || now[i] == old[i], "Texture loads, UVs, geometry and other sprite formats remain unchanged");
        ++cases;
    }
    std::printf("sprite bounds: %u cases, %u checks passed; no game launched\n", cases, checks);
}
