#include "rr64_character_preferences.hpp"
#include "rr64_engine_layout.hpp"
#include "rr64_native.hpp"
#include "rr64_netplay.hpp"

#include <array>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <vector>

// Links the production menu bridge and atomic preference store. Only network
// status and the existing cop-bike lookup are test doubles; no ROM or game
// process is needed. Full RDRAM comparisons enforce each hook's write scope.
namespace {
namespace prefs = rr64::character_preferences;
namespace engine = rr64::engine;
constexpr unsigned selected = 0x8009F670;
constexpr unsigned race_selected = 0x8009F400;
constexpr unsigned explicit_choice = 0x8009EAE8;
constexpr unsigned ready = 0x8009EF2C;
constexpr unsigned cops_unlocked = 0x800A77D8;
constexpr unsigned campaign = 0x800D6A40;
unsigned checks = 0;
rr64::netplay::Status online_status{};
bool custom_cop = false;
std::array<bool, 4> cop_bike{};

void require(bool condition, const char* message) {
    ++checks;
    if (!condition) {
        std::fprintf(stderr, "Character menu check failed: %s\n", message);
        std::exit(1);
    }
}

struct Fixture {
    std::vector<unsigned char> memory = std::vector<unsigned char>(engine::kRdramSize, 0xA5);
    const std::filesystem::path missing_directory = std::filesystem::temp_directory_path() /
        ("rr64-character-menu-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));

    Fixture() {
        require(!std::filesystem::exists(missing_directory), "preference fixture path must be unused");
        reset();
    }
    unsigned char* data() { return memory.data(); }
    void word(unsigned address, unsigned value) { engine::write_u32(data(), address, value); }
    void half(unsigned address, unsigned value) { engine::write_u16(data(), address, static_cast<std::uint16_t>(value)); }
    unsigned word(unsigned address) {
        unsigned value = 0;
        engine::read_u32(data(), address, value);
        return value;
    }
    void reset() {
        prefs::initialize(missing_directory); // No files are created or modified.
        online_status = {};
        custom_cop = false;
        cop_bike.fill(false);
        half(cops_unlocked, 0);
        word(engine::local_race::menu_humans, 4);
        for (unsigned slot = 0; slot < 4; ++slot) {
            word(selected + slot * 4, 3 + slot);
            word(race_selected + slot * 4, 9 + slot);
            half(explicit_choice + slot * 2, 0);
            word(ready + slot * 4, 1);
        }
        // Distinct saved identity and nonzero money/bike make accidental
        // campaign writes visible, including the unpurchased New Game choice.
        word(campaign + 0x14, 11);
        word(campaign + 0x18, 27);
        word(campaign + 0x38, 2000);
    }
    void expect_restore(unsigned slot, unsigned rider, bool multiplayer, bool allowed) {
        auto expected = memory;
        if (allowed) {
            engine::write_u32(expected.data(), selected + slot * 4, rider);
            engine::write_u16(expected.data(), explicit_choice + slot * 2, 1);
        }
        rr64_character_restore(data(), slot, multiplayer);
        require(memory == expected, "restore changes only the allowed menu rider and explicit flag");
    }
    void expect_preferences(const std::array<unsigned, 4>& expected) {
        for (unsigned slot = 0; slot < 4; ++slot)
            require(prefs::last(slot) == expected[slot], "only the intended local preference changes");
    }
};
}

namespace rr64::netplay {
Status get_status() { return online_status; }
}
extern "C" int rr64_custom_cop_enabled() { return custom_cop; }
extern "C" unsigned rr64_custom_cop_bike_entry(unsigned char*, unsigned slot, unsigned stock) {
    return custom_cop && slot < cop_bike.size() && cop_bike[slot] ? 0x800A684C : stock;
}

int main() {
    Fixture f;
    for (unsigned slot = 0; slot < 4; ++slot) {
        f.expect_restore(slot, prefs::unknown, false, false);
        f.expect_restore(slot, prefs::unknown, true, false);
    }

    // Boundary IDs, every local slot, both selector kinds, and the actual
    // native unlock flag. Remembering a locked cop must never grant an unlock.
    for (unsigned slot = 0; slot < 4; ++slot)
        for (unsigned rider : {0u, 39u, 40u, 44u})
            for (bool unlocked : {false, true})
                for (bool multiplayer : {false, true}) {
                    f.reset();
                    prefs::remember(slot, rider);
                    f.half(cops_unlocked, unlocked ? 7 : 0);
                    f.expect_restore(slot, rider, multiplayer, rider < 40 || unlocked);
                }

    // The custom cop record restricts its rider to40..44 even when the stock
    // unlock permits every rider. A regular bike retains the native limits.
    for (unsigned slot = 0; slot < 4; ++slot)
        for (unsigned rider : {12u, 40u, 44u})
            for (bool appended_cop_bike : {false, true}) {
                f.reset();
                custom_cop = true;
                cop_bike[slot] = appended_cop_bike;
                prefs::remember(slot, rider);
                f.expect_restore(slot, rider, true, appended_cop_bike ? rider >= 40 : rider < 40);
            }
    f.reset();
    custom_cop = true;
    cop_bike[0] = true;
    f.half(cops_unlocked, 1);
    prefs::remember(0, 12);
    f.expect_restore(0, 12, true, false);

    // Explicit input, and a campaign-loaded choice already marked explicit,
    // outrank a remembered preference. Another player's manual flag is local.
    for (unsigned slot = 0; slot < 4; ++slot)
        for (unsigned manual : {1u, 0xFFFFu}) {
            f.reset();
            prefs::remember(slot, 18);
            f.half(explicit_choice + slot * 2, manual);
            f.word(selected + slot * 4, 27);
            f.word(race_selected + slot * 4, 27);
            f.expect_restore(slot, 18, false, false);
        }
    f.reset();
    for (unsigned slot = 0; slot < 4; ++slot)
        prefs::remember(slot, 15 + slot);
    f.half(explicit_choice + 2, 1);
    for (unsigned slot = 0; slot < 4; ++slot)
        f.expect_restore(slot, 15 + slot, true, slot != 1);
    const auto restored = f.memory;
    for (unsigned slot = 0; slot < 4; ++slot)
        rr64_character_restore(f.data(), slot, true);
    require(f.memory == restored, "subsequent selector refresh preserves the active choice");

    // This hook is installed only on the two proven New Game reset paths.
    // It must seed the new menu even after a prior explicit choice, but leave
    // campaign/race-load identity, purchase cost, bike and unlock state intact.
    for (unsigned rider : {6u, 40u, 44u})
        for (bool unlocked : {false, true}) {
            f.reset();
            prefs::remember(0, rider);
            f.half(explicit_choice, 1);
            f.half(cops_unlocked, unlocked);
            custom_cop = true;
            cop_bike[0] = true; // Cannot authorize a police rider in campaign.
            auto expected = f.memory;
            if (rider < 40 || unlocked) {
                engine::write_u32(expected.data(), selected, rider);
                engine::write_u16(expected.data(), explicit_choice, 1);
            }
            rr64_character_new_campaign(f.data());
            require(f.memory == expected, "New Game seeds menu only and respects native unlocks");
        }
    f.reset();
    const auto no_preference = f.memory;
    rr64_character_new_campaign(f.data());
    require(f.memory == no_preference, "New Game without preference keeps stock state");

    // Production remember() is read-only with respect to every guest byte.
    f.reset();
    for (unsigned slot = 0; slot < 4; ++slot)
        f.word(selected + slot * 4, 20 + slot);
    auto before = f.memory;
    for (unsigned slot = 0; slot < 4; ++slot)
        rr64_character_remember(f.data(), slot);
    f.expect_preferences({20, 21, 22, 23});
    require(f.memory == before, "remember never writes guest memory");
    f.word(selected, 45);
    rr64_character_remember(f.data(), 0);
    require(prefs::last(0) == 20, "invalid native choice cannot replace saved preference");

    // Every ready mask and local player count; unconfirmed and absent players
    // must retain their earlier choices, including canceled selection screens.
    for (unsigned count = 1; count <= 4; ++count)
        for (unsigned mask = 0; mask < 16; ++mask) {
            f.reset();
            f.word(engine::local_race::menu_humans, count);
            std::array<unsigned, 4> expected{};
            for (unsigned slot = 0; slot < 4; ++slot) {
                prefs::remember(slot, slot);
                f.word(selected + slot * 4, 30 + slot);
                f.word(ready + slot * 4, mask & (1u << slot) ? 2 : 1);
                expected[slot] = slot < count && (mask & (1u << slot)) ? 30 + slot : slot;
            }
            before = f.memory;
            rr64_character_remember_multiplayer(f.data());
            f.expect_preferences(expected);
            require(f.memory == before, "multiplayer capture never mutates guest state");
        }
    for (unsigned count : {0u, 5u, 14u, 0xFFFFFFFFu}) {
        f.reset();
        f.word(engine::local_race::menu_humans, count);
        for (unsigned slot = 0; slot < 4; ++slot)
            f.word(ready + slot * 4, 2);
        rr64_character_remember_multiplayer(f.data());
        f.expect_preferences({prefs::unknown, prefs::unknown, prefs::unknown, prefs::unknown});
    }

    // Private online selector is guest row0, even for network slot13. Other
    // phases may contain canonical remote rows and must never save them.
    using Phase = rr64::netplay::Phase;
    for (Phase phase : {Phase::Offline, Phase::Connecting, Phase::Lobby, Phase::GameSetup,
                        Phase::CharacterSelect, Phase::TrackSelect, Phase::Race})
        for (bool connected : {false, true})
            for (unsigned local_slot : {0u, 3u, 13u}) {
                f.reset();
                online_status.active = true;
                online_status.connected = connected;
                online_status.phase = phase;
                online_status.local_slot = static_cast<std::uint8_t>(local_slot);
                online_status.replicated_riders = local_slot >= 4;
                f.word(engine::local_race::menu_humans, 14);
                for (unsigned slot = 0; slot < 4; ++slot) {
                    prefs::remember(slot, slot);
                    f.word(selected + slot * 4, 35 + slot);
                    f.word(ready + slot * 4, 2);
                }
                before = f.memory;
                rr64_character_remember_multiplayer(f.data());
                f.expect_preferences({connected && phase == Phase::CharacterSelect ? 35u : 0u, 1, 2, 3});
                require(f.memory == before, "online capture leaves local and remote guest rows untouched");
            }
    f.reset();
    online_status.active = online_status.connected = true;
    online_status.phase = Phase::CharacterSelect;
    f.word(ready, 1);
    f.word(ready + 4, 2);
    rr64_character_remember_multiplayer(f.data());
    f.expect_preferences({prefs::unknown, prefs::unknown, prefs::unknown, prefs::unknown});

    f.reset();
    before = f.memory;
    rr64_character_restore(nullptr, 0, 1);
    rr64_character_restore(f.data(), 4, 1);
    rr64_character_restore(f.data(), prefs::unknown, 0);
    rr64_character_remember(nullptr, 0);
    rr64_character_remember(f.data(), 4);
    rr64_character_remember(f.data(), prefs::unknown);
    rr64_character_remember_multiplayer(nullptr);
    rr64_character_new_campaign(nullptr);
    require(f.memory == before, "null memory and invalid slots have no guest effects");
    f.expect_preferences({prefs::unknown, prefs::unknown, prefs::unknown, prefs::unknown});
    require(!std::filesystem::exists(f.missing_directory), "menu hooks perform no filesystem writes");
    std::printf("RR64 character menu: %u checks passed.\n", checks);
}
