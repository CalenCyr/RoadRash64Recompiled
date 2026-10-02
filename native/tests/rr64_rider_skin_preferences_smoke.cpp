#include "rr64_rider_skin_preferences.hpp"

#include <array>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>
#include <thread>

namespace prefs = rr64::rider_skin_preferences;
namespace fs = std::filesystem;

int main() {
    const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
    const auto directory = fs::temp_directory_path() /
        ("rr64-rider-skin-preferences-" + std::to_string(stamp));
    if (!fs::create_directory(directory))
        return 2;
    struct Cleanup {
        fs::path directory;
        ~Cleanup() { std::error_code error; fs::remove_all(directory, error); }
    } cleanup{directory};
    const auto path = directory / "rider-skin-preferences.cfg";
    auto temporary = path;
    temporary += ".tmp";
    unsigned failures = 0;
    auto check = [&](bool condition, const char* description) {
        if (!condition) {
            ++failures;
            std::cerr << "FAIL: " << description << '\n';
        }
    };
    auto write = [&](std::string_view text) {
        std::ofstream output(path, std::ios::binary | std::ios::trunc);
        output.write(text.data(), static_cast<std::streamsize>(text.size()));
    };
    auto read = [&] {
        std::ifstream input(path, std::ios::binary);
        return std::string(std::istreambuf_iterator<char>(input), {});
    };
    auto check_empty = [&] {
        for (unsigned i = 0; i < 4; ++i)
            check(prefs::last(i).empty(), "invalid file resets every slot");
    };

    prefs::initialize(directory);
    check_empty();
    prefs::flush();
    check(!fs::exists(path), "clean initialization does not write");
    const std::array<std::string, 4> original{
        "doom-guy", "cortana", "punisher", "runescape-bald"};
    for (unsigned i = 0; i < original.size(); ++i)
        prefs::remember(i, original[i]);
    check(!fs::exists(path), "remember does not perform file I/O");
    prefs::flush();
    prefs::initialize(directory);
    for (unsigned i = 0; i < original.size(); ++i)
        check(prefs::last(i) == original[i], "four stable IDs survive reload");
    prefs::remember(1, "");
    prefs::remember(0, "master-chief");
    prefs::flush();
    prefs::initialize(directory);
    check(prefs::last(0) == "master-chief" && prefs::last(1).empty(),
          "existing file is replaced and native choice persists");
    const auto previous = read();
    prefs::remember(99, "doom-guy");
    for (const auto id : {"Doom", "bad/id", "bad id", "_bad", "bad.id"})
        prefs::remember(0, id);
    prefs::remember(0, std::string(33, 'a'));
    prefs::remember(0, std::string("a\0b", 3));
    prefs::flush();
    check(read() == previous && prefs::last(99).empty(), "invalid updates ignored");

    fs::create_directory(temporary);
    prefs::remember(0, "ghost-rider");
    prefs::flush();
    check(read() == previous, "failed write preserves previous complete file");
    fs::remove(temporary);
    prefs::flush();
    prefs::initialize(directory);
    check(prefs::last(0) == "ghost-rider", "failed write remains dirty for retry");
    const std::string valid = "RR64 rider skins 1\ndoom-guy\ncortana\n\npunisher\nend\n";
    for (std::size_t n = 0; n < valid.size(); ++n) {
        write(std::string_view(valid).substr(0, n));
        prefs::initialize(directory);
        check_empty();
    }
    for (const auto invalid : {
        "RR64 rider skins 2\ndoom-guy\ncortana\n\npunisher\nend\n",
        "RR64 rider skins 1\ndoom-guy\nCortana\n\npunisher\nend\n",
        "RR64 rider skins 1\na\nb\nc\nd\nend\nextra\n"}) {
        write(invalid);
        prefs::initialize(directory);
        check_empty();
    }
    write(std::string(1024, 'a'));
    prefs::initialize(directory);
    check_empty();
    write("RR64 rider skins 1\r\na\r\n\r\nc\r\nd\r\nend\r\n");
    prefs::initialize(directory);
    check(prefs::last(0) == "a" && prefs::last(1).empty(), "CRLF file accepted");
    prefs::remember(0, std::string(32, 'a'));
    prefs::flush();
    prefs::initialize(directory);
    check(prefs::last(0) == std::string(32, 'a'), "maximum ID length roundtrips");

    std::thread updates([] {
        for (unsigned i = 0; i < 1000; ++i)
            prefs::remember(0, i % 2 ? "punisher" : "doom-guy");
        prefs::remember(0, "last-choice");
    });
    for (unsigned i = 0; i < 30; ++i)
        prefs::flush();
    updates.join();
    prefs::flush();
    prefs::initialize(directory);
    check(prefs::last(0) == "last-choice", "concurrent updates are not lost by flush");
    std::cout << "Rider skin preferences: " << failures << " failures\n";
    return failures ? 1 : 0;
}
