#include "rr64_character_preferences.hpp"

#include <array>
#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>
#include <thread>

namespace {
namespace prefs = rr64::character_preferences;
unsigned checks = 0;
void require(bool condition, const char* message) {
    ++checks;
    if (!condition) {
        std::fprintf(stderr, "Character preference check failed: %s\n", message);
        std::exit(1);
    }
}
void expect_unknown() {
    for (unsigned slot = 0; slot < 4; ++slot)
        require(prefs::last(slot) == prefs::unknown, "unknown preference");
}
void write(const std::filesystem::path& path, const std::string& text) {
    std::ofstream file(path, std::ios::binary | std::ios::trunc);
    file << text;
    file.close();
    require(static_cast<bool>(file), "fixture file write");
}
void remove_files(const std::filesystem::path& directory) {
    // Delete only the two named test files; never recursively remove a directory.
    std::filesystem::remove(directory / "character-preferences.cfg");
    std::filesystem::remove(directory / "character-preferences.cfg.tmp");
}
} // namespace

int main() {
    const auto root = std::filesystem::temp_directory_path() /
        ("rr64-character-preferences-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    require(std::filesystem::create_directory(root), "unique fixture directory");
    const auto file = root / "character-preferences.cfg";
    prefs::initialize(root);
    expect_unknown();
    prefs::flush();
    require(!std::filesystem::exists(file), "unchanged unknown state creates no file");

    for (unsigned slot = 0; slot < 4; ++slot) {
        for (unsigned rider = 0; rider < 45; ++rider) {
            prefs::remember(slot, rider);
            require(prefs::last(slot) == rider, "every native rider ID accepted");
        }
        prefs::remember(slot, 45);
        prefs::remember(slot, prefs::unknown);
        require(prefs::last(slot) == 44, "invalid rider does not replace preference");
    }
    prefs::remember(4, 0);
    prefs::remember(prefs::unknown, 0);
    require(prefs::last(4) == prefs::unknown && prefs::last(prefs::unknown) == prefs::unknown, "invalid slot bounded");
    prefs::flush();
    prefs::initialize(root);
    for (unsigned slot = 0; slot < 4; ++slot)
        require(prefs::last(slot) == 44, "preferences survive reload");
    prefs::remember(0, 0);
    prefs::flush();
    prefs::initialize(root);
    require(prefs::last(0) == 0 && prefs::last(1) == 44, "existing file replaced without losing other slots");

    for (const std::string& text : {
        std::string{}, std::string{"1\n"}, std::string{"1\n0 1 2\n"},
        std::string{"1\n0 1 2 4"}, std::string{"1\n0 1 2 44"},
        std::string{"2\n0 1 2 3\n"}, std::string{"1\n0 1 2 45\n"},
        std::string{"1\n0 1 2 -1\n"}, std::string{"1\n0 1 2 4294967296\n"},
        std::string{"1\n0 1 2 3extra\n"}, std::string{"1\n0 1 2 3 4\n"},
        std::string{"1\n0 1 2 +3\n"}, std::string(129, '1')}) {
        write(file, text);
        prefs::initialize(root);
        expect_unknown();
    }
    write(file, "1\n4294967295 7 0 44\n");
    prefs::initialize(root);
    require(prefs::last(0) == prefs::unknown && prefs::last(1) == 7 &&
            prefs::last(2) == 0 && prefs::last(3) == 44, "explicit unknown and native bounds load");

    std::atomic<bool> start{false};
    std::array<std::thread, 4> writers;
    for (unsigned slot = 0; slot < 4; ++slot) {
        writers[slot] = std::thread([&, slot] {
            while (!start.load(std::memory_order_acquire))
                std::this_thread::yield();
            for (unsigned i = 1; i <= 5000; ++i)
                prefs::remember(slot, (i + slot) % 45);
        });
    }
    start.store(true, std::memory_order_release);
    for (unsigned i = 0; i < 64; ++i)
        prefs::flush();
    for (auto& writer : writers)
        writer.join();
    prefs::flush();
    prefs::initialize(root);
    for (unsigned slot = 0; slot < 4; ++slot)
        require(prefs::last(slot) == (5000 + slot) % 45, "concurrent updates preserve latest value on disk");

    const auto blocked = root / "blocked";
    write(blocked, "not a directory");
    prefs::initialize(blocked);
    prefs::remember(2, 18);
    prefs::flush();
    require(prefs::last(2) == 18, "failed save preserves in-memory preference");
    std::filesystem::remove(blocked);
    std::filesystem::create_directory(blocked);
    prefs::flush();
    prefs::initialize(blocked);
    require(prefs::last(2) == 18, "failed save remains dirty for retry");

    remove_files(blocked);
    std::filesystem::remove(blocked);
    remove_files(root);
    require(std::filesystem::remove(root), "fixture directory cleanup");
    std::printf("RR64 character preferences: %u checks passed.\n", checks);
}
