#include "rr64_rider_skin_preferences.hpp"

#include <array>
#include <cstdint>
#include <fstream>
#include <mutex>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace rr64::rider_skin_preferences {
namespace {
using Choices = std::array<std::string, 4>;
constexpr std::string_view header = "RR64 rider skins 1";
constexpr std::size_t max_id_size = 32;

// File work never holds the state mutex, so game-thread selection stays short.
std::mutex state_mutex;
std::mutex file_mutex;
Choices choices;
std::uint64_t revision = 0;
std::uint64_t written_revision = 0;
std::filesystem::path settings_path;
bool initialized = false;

bool valid_id(std::string_view id) {
    if (id.size() > max_id_size)
        return false;
    for (const char c : id) {
        if (!((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-'))
            return false;
    }
    return true;
}

bool take_line(std::string_view& text, std::string_view& line) {
    const auto end = text.find('\n');
    if (end == std::string_view::npos)
        return false;
    line = text.substr(0, end);
    text.remove_prefix(end + 1);
    if (!line.empty() && line.back() == '\r')
        line.remove_suffix(1);
    return true;
}

Choices load(const std::filesystem::path& path) {
    std::ifstream file(path, std::ios::binary);
    if (!file)
        return {};
    // Four bounded IDs fit in 160 bytes. Read one bounded record and accept it
    // only in full, including the end marker; truncated IDs must not become IDs.
    std::array<char, 256> bytes{};
    file.read(bytes.data(), bytes.size());
    if (!file.eof() || file.bad())
        return {};
    std::string_view text(bytes.data(), static_cast<std::size_t>(file.gcount()));
    std::string_view line;
    if (!take_line(text, line) || line != header)
        return {};
    Choices loaded;
    for (auto& id : loaded) {
        if (!take_line(text, line) || !valid_id(line))
            return {};
        id.assign(line);
    }
    if (!take_line(text, line) || line != "end" || !text.empty())
        return {};
    return loaded;
}

bool replace(const std::filesystem::path& temporary,
             const std::filesystem::path& destination) {
#ifdef _WIN32
    // std::filesystem::rename cannot replace an existing file on Windows.
    // MoveFileEx keeps the previous complete record intact if replacement fails.
    return MoveFileExW(temporary.c_str(), destination.c_str(),
        MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) != 0;
#else
    std::error_code error;
    std::filesystem::rename(temporary, destination, error);
    return !error;
#endif
}
} // namespace

void initialize(const std::filesystem::path& directory) {
    std::lock_guard file_lock(file_mutex);
    settings_path = directory / "rider-skin-preferences.cfg";
    const auto loaded = load(settings_path);
    std::lock_guard state_lock(state_mutex);
    choices = loaded;
    revision = written_revision = 0;
    initialized = true;
}

void remember(unsigned slot, std::string_view stable_id) {
    if (slot >= choices.size() || !valid_id(stable_id))
        return;
    std::lock_guard lock(state_mutex);
    if (choices[slot] != stable_id) {
        choices[slot].assign(stable_id);
        ++revision;
    }
}

std::string last(unsigned slot) {
    if (slot >= choices.size())
        return {};
    std::lock_guard lock(state_mutex);
    return choices[slot];
}

void flush() {
    std::lock_guard file_lock(file_mutex);
    Choices snapshot;
    std::uint64_t snapshot_revision;
    {
        std::lock_guard state_lock(state_mutex);
        if (!initialized || revision == written_revision)
            return;
        snapshot = choices;
        snapshot_revision = revision;
    }
    std::error_code error;
    if (!settings_path.parent_path().empty())
        std::filesystem::create_directories(settings_path.parent_path(), error);
    if (error)
        return;
    auto temporary = settings_path;
    temporary += ".tmp";
    std::ofstream file(temporary, std::ios::binary | std::ios::trunc);
    file << header << '\n';
    for (const auto& id : snapshot)
        file << id << '\n';
    file << "end\n";
    file.close();
    if (!file || !replace(temporary, settings_path))
        return;
    // A selection made during disk I/O has a newer revision and stays dirty.
    std::lock_guard state_lock(state_mutex);
    written_revision = snapshot_revision;
}
} // namespace rr64::rider_skin_preferences
