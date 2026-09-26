#include "rr64_engine_layout.hpp"
#include "rr64_local_players.hpp"
#include "rr64_native.hpp"
#include "rr64_profile_names.hpp"

#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

extern "C" void func_8007280C(unsigned char*, recomp_context*);
extern "C" void test_thrash_profile_init(unsigned char*, recomp_context*);
// Unrelated name-entry graphics initialization is not needed for this test.
extern "C" void func_80072880(unsigned char*, recomp_context*) {}
static bool thrash_active = false;
extern "C" int rr64_thrash_options_active() { return thrash_active; }

namespace {
using namespace rr64::engine;
constexpr unsigned actor_name = 0x800D857Cu;

void check(bool condition, const char* message) {
    if (!condition) { std::fprintf(stderr, "%s\n", message); std::exit(1); }
}
std::string field(unsigned char* memory, unsigned address) {
    std::string result;
    for (unsigned i = 0; i < globals::campaign_name_capacity; ++i) {
        std::int8_t value = 0;
        read_s8(memory, address + i, value);
        if (!value) return result;
        result += static_cast<char>(value);
    }
    check(false, "Name field is not terminated");
    return result;
}
void put(unsigned char* memory, unsigned address, const char* name) {
    for (unsigned i = 0; i < 12; ++i) {
        write_s8(memory, address + i, *name);
        if (*name) ++name;
    }
}
} // namespace

int main() {
    using namespace rr64::engine;
    std::vector<unsigned char> memory(kRdramSize, 0xA5);
    auto* m = memory.data();
    recomp_context ctx{};
    ctx.r29 = 0x807FF000u;
    rr64::local_players::set_name(0, "test42");
    func_8007280C(m, &ctx);
    check(field(m, globals::campaign_name) == "TEST42_____", "New campaign did not seed the editable profile name");
    std::uint32_t cursor = 0;
    read_u32(m, globals::name_entry_character, cursor);
    check(cursor == 6, "Name editor cursor did not follow the profile name");
    for (unsigned i = 0; i < 50; ++i) {
        std::uint8_t value = 0;
        read_u8(m, 0x8009EF9Cu + i, value);
        check(value == 0xA5, "Profile name overwrote the keyboard letter grid");
    }
    for (unsigned address : {globals::campaign_name - 1, globals::campaign_name + 12}) {
        std::uint8_t value = 0;
        read_u8(m, address, value);
        check(value == 0xA5, "Campaign name write escaped its field");
    }
    rr64::local_players::set_name(0, "123456789ABCDEF");
    func_8007280C(m, &ctx);
    check(field(m, globals::campaign_name) == "123456789AB", "Long profile was not bounded to the game field");
    read_u32(m, globals::name_entry_character, cursor);
    check(cursor == 10, "Full field cursor exceeds the last editable cell");

    // Profile changes and name-entry navigation must not rewrite a save/manual name.
    put(m, globals::campaign_name, "MY SAVE 7");
    rr64::local_players::set_name(0, "other42");
    write_u16(m, globals::controller_pressed_buttons, 0);
    write_s8(m, globals::controller_stick_x, 0);
    write_s8(m, globals::controller_stick_y, 0);
    rr64_name_entry_navigation(m, 8, 4);
    check(field(m, globals::campaign_name) == "MY SAVE 7", "Navigation changed the existing campaign name");

    put(m, 0x80006ABCu, "STOCK");
    test_thrash_profile_init(m, &ctx);
    check(field(m, actor_name) == "STOCK", "Disabled Thrash guard changed the native name");
    const auto before = memory;
    thrash_active = true;
    test_thrash_profile_init(m, &ctx);
    check(field(m, actor_name) == "OTHER42", "Solo Thrash did not use its Controls profile");
    check(field(m, globals::campaign_name) == "MY SAVE 7", "Thrash changed the campaign name");
    for (unsigned i = 0; i < memory.size(); ++i) {
        const unsigned guest = 0x80000000u + (i ^ 3u);
        if (guest < actor_name || guest >= actor_name + 12)
            check(memory[i] == before[i], "Thrash profile changed data outside the actor name");
    }
    rr64::local_players::set_name(0, "   ");
    test_thrash_profile_init(m, &ctx);
    check(field(m, actor_name) == "PLAYER 1", "Empty profile lost its default name");
    rr64_profile_name_new_campaign(nullptr);
    rr64_profile_name_thrash(nullptr);
    std::puts("RR64 profile names: native campaign init, solo copy, bounds, grid and save preservation passed.");
}
