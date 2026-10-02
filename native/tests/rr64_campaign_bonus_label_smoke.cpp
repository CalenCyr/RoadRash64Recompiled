#include "rr64_campaign_bonus_save.hpp"
#include "rr64_engine_layout.hpp"

#include <array>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

extern "C" void func_8001F960(unsigned char *, recomp_context *);
extern "C" void func_8002073C(unsigned char *, recomp_context *);
extern "C" void test_bonus_load_label(unsigned char *, recomp_context *);
extern "C" void test_bonus_load_label_original(unsigned char *, recomp_context *);
extern "C" void test_bonus_save_label(unsigned char *, recomp_context *);
extern "C" void test_bonus_save_label_original(unsigned char *, recomp_context *);
extern "C" void test_bonus_overwrite_label(unsigned char *, recomp_context *);
extern "C" void test_bonus_overwrite_label_original(unsigned char *, recomp_context *);

namespace {
using namespace rr64::engine;
constexpr unsigned profile = 0x800D6A40u, cache = 0x800C07C8u;
constexpr unsigned staging = 0x800C0470u, status = 0x8009DE08u;
constexpr unsigned stack = 0x807FE000u, text_buffer = 0x80300000u;
constexpr unsigned format = 0x80002BC4u, unlocks = 0x800A5334u;
unsigned checks = 0, labels = 0;

void check(bool value, const char *message) {
    ++checks;
    if (!value) {
        std::fprintf(stderr, "Campaign bonus labels: %s\n", message);
        std::exit(1);
    }
}
unsigned word(unsigned char *m, unsigned address) {
    unsigned value = 0;
    read_u32(m, address, value);
    return value;
}
std::vector<unsigned char> bytes(unsigned char *m, unsigned address, unsigned size) {
    std::vector<unsigned char> result(size);
    for (unsigned i = 0; i < size; ++i)
        read_u8(m, address + i, result[i]);
    return result;
}
void copy(unsigned char *m, unsigned destination, unsigned source, unsigned size) {
    const auto data = bytes(m, source, size);
    for (unsigned i = 0; i < size; ++i)
        write_s8(m, destination + i, data[i]);
}
void put_text(unsigned char *m, unsigned address, const std::string &value) {
    for (unsigned i = 0; i <= value.size(); ++i)
        write_s8(m, address + i, i == value.size() ? 0 : value[i]);
}
std::string get_text(unsigned char *m, unsigned address) {
    std::string result;
    for (unsigned i = 0; i < 64; ++i) {
        std::uint8_t value = 0;
        read_u8(m, address + i, value);
        if (value == 0)
            return result;
        result += static_cast<char>(value);
    }
    check(false, "unterminated native label");
    return {};
}
void native_checksum(unsigned char *m, unsigned address) {
    write_u32(m, address + 4, 0);
    recomp_context ctx{};
    ctx.r4 = guest_address(address);
    func_8001F960(m, &ctx);
    write_u32(m, address + 4, unsigned(ctx.r2));
}

void prepare(unsigned char *m, unsigned slot, unsigned chapter, int corrupt_bit = -1) {
    for (unsigned i = 0; i < 0xF8; i += 4)
        write_u32(m, profile + i, 0);
    write_u32(m, profile, 4);
    put_text(m, profile + 8, "Rider " + std::to_string(slot));
    write_u32(m, profile + 0x14, 25);
    write_u32(m, profile + 0x20, 1);
    write_u32(m, profile + 0x38, 545 + slot);
    write_u32(m, profile + 0x3C, chapter);
    write_u32(m, profile + 0x50, 0x11111111u);
    write_u32(m, profile + 0x54, 0xA1234567u);
    native_checksum(m, profile);
    rr64_campaign_begin_bonus(m);
    rr64_campaign_qualification_store(m, profile + 0x54, 0xFEDCBA98u - slot);
    for (unsigned i = 0; i < 256; i += 4)
        write_u32(m, staging + i, 0);
    copy(m, staging, profile, 0xF8);
    rr64_campaign_bonus_save(m, profile, staging);
    if (corrupt_bit >= 0) {
        const unsigned address = staging + 0xF8 + unsigned(corrupt_bit) / 8;
        std::uint8_t value = 0;
        read_u8(m, address, value);
        write_s8(m, address, value ^ (1u << (unsigned(corrupt_bit) % 8)));
    }
    copy(m, cache + slot * 0xF8, staging, 0xF8);
    rr64_campaign_bonus_capture(m, cache + slot * 0xF8);
    write_u32(m, status + slot * 4, 2);
}

using Native = void (*)(unsigned char *, recomp_context *);
struct Menu {
    Native patched;
    Native original;
    unsigned chapter_offset;
};
const std::array<Menu, 3> menus{{
    {test_bonus_load_label, test_bonus_load_label_original, 0x78},
    {test_bonus_save_label, test_bonus_save_label_original, 0xE8},
    {test_bonus_overwrite_label, test_bonus_overwrite_label_original, 0xE8},
}};

void run(unsigned char *m, unsigned slot, const Menu &menu, Native instructions,
         unsigned expected_level) {
    recomp_context ctx{};
    ctx.r29 = guest_address(stack);
    ctx.r4 = slot;
    ctx.r5 = guest_address(text_buffer);
    ctx.r6 = guest_address(stack + menu.chapter_offset);
    ctx.r7 = guest_address(stack + menu.chapter_offset + 4);
    // This exact metadata function writes the base chapter and cash to the
    // caller's stack. Its chapter must remain unchanged by label formatting.
    func_8002073C(m, &ctx);
    check(unsigned(ctx.r2) == 2, "native slot metadata retains availability");
    check(get_text(m, text_buffer) == "Rider " + std::to_string(slot),
          "native metadata retains profile name");
    const unsigned base_chapter = word(m, cache + slot * 0xF8 + 0x3C);
    check(word(m, stack + menu.chapter_offset) == base_chapter,
          "metadata returns serialized base chapter");
    check(word(m, stack + menu.chapter_offset + 4) ==
              word(m, cache + slot * 0xF8 + 0x38),
          "metadata retains cash");
    for (unsigned tier = 0; tier < 15; ++tier)
        write_u16(m, unlocks + tier * 2, 0);
    ctx.r4 = guest_address(text_buffer);
    ctx.r17 = slot; // Native save and overwrite list index.
    ctx.r18 = slot; // Native load list index.
    instructions(m, &ctx);
    check(get_text(m, text_buffer) == "Level " + std::to_string(expected_level),
          "native formatter selects the expected visible level");
    check(word(m, stack + menu.chapter_offset) == base_chapter,
          "display hook leaves native unlock input unchanged");
    ++labels;
}

void check_slot(unsigned char *m, unsigned slot, unsigned expected_level) {
    const auto records = bytes(m, cache, 6 * 0xF8);
    const auto live = bytes(m, profile, 0xF8);
    const auto file = bytes(m, staging, 256);
    const unsigned base_level = word(m, cache + slot * 0xF8 + 0x3C) + 1;
    for (const auto &menu : menus) {
        run(m, slot, menu, menu.original, base_level);
        const auto original_unlocks = bytes(m, unlocks, 15 * 2);
        run(m, slot, menu, menu.patched, expected_level);
        check(bytes(m, unlocks, 15 * 2) == original_unlocks,
              "all native tier unlocks match the original formatter path");
        check(bytes(m, cache, 6 * 0xF8) == records &&
                  bytes(m, profile, 0xF8) == live && bytes(m, staging, 256) == file,
              "label paths preserve every cached, live and serialized byte");
    }
}
} // namespace

// Model only libc formatting. Native metadata chooses its source/name;
// native label instructions choose the original ROM format and argument,
// and the real native 5F420 executes every unlock write afterward.
extern "C" void sprintf_recomp(unsigned char *m, recomp_context *ctx) {
    const auto source = get_text(m, unsigned(ctx->r5));
    char result[64]{};
    int size = 0;
    if (unsigned(ctx->r5) == format) {
        check(source == "Level %d", "original ROM level format retained");
        size = std::snprintf(result, sizeof(result), source.c_str(), int(ctx->r6));
    } else {
        check(source.find('%') == std::string::npos, "fixture name uses no formatting conversions");
        size = std::snprintf(result, sizeof(result), "%s", source.c_str());
    }
    check(size >= 0 && unsigned(size) < sizeof(result), "bounded fixture format output");
    put_text(m, unsigned(ctx->r4), result);
    ctx->r2 = size;
}

int main() {
    std::vector<unsigned char> memory(kRdramSize);
    auto *m = memory.data();
    // Verified from USA ROM offset37C4, mapped at80002BC4.
    put_text(m, format, "Level %d");
    rr64_campaign_bonus_new_profile(m, profile);
    for (unsigned slot = 0; slot < 6; ++slot)
        prepare(m, slot, 5);
    for (unsigned slot = 0; slot < 6; ++slot)
        check_slot(m, slot, 6);

    // A live bonus campaign cannot relabel a legacy or earlier saved slot.
    for (unsigned chapter = 0; chapter < 5; ++chapter) {
        prepare(m, 2, chapter);
        write_u32(m, profile + 0x3C, 5);
        check_slot(m, 2, chapter + 1);
    }
    for (int bit = 0; bit < 64; ++bit) {
        prepare(m, 0, 5, bit);
        check_slot(m, 0, 5);
    }
    prepare(m, 0, 5);
    write_u32(m, cache + 0x38, word(m, cache + 0x38) + 1);
    check_slot(m, 0, 5); // Exact-record mismatch invalidates the side cache.
    prepare(m, 0, 5);
    rr64_campaign_bonus_scan(m);
    check_slot(m, 0, 5); // Fresh/failed scans cannot borrow an old extension.

    prepare(m, 0, 5);
    for (unsigned availability : {0u, 1u}) {
        write_u32(m, status, availability);
        write_u32(m, stack + 0x78, 0x12345678u);
        write_u32(m, stack + 0x7C, 0x87654321u);
        put_text(m, text_buffer, "Unchanged");
        recomp_context ctx{};
        ctx.r29 = guest_address(stack);
        ctx.r5 = guest_address(text_buffer);
        ctx.r6 = guest_address(stack + 0x78);
        ctx.r7 = guest_address(stack + 0x7C);
        func_8002073C(m, &ctx);
        check(unsigned(ctx.r2) == availability &&
                  word(m, stack + 0x78) == 0x12345678u &&
                  word(m, stack + 0x7C) == 0x87654321u &&
                  get_text(m, text_buffer) == "Unchanged",
              "empty/unavailable native metadata leaves outputs untouched");
    }
    std::printf("Campaign bonus labels: %u checks passed, %u native original/patched label-unlock executions\n",
                checks, labels);
}
