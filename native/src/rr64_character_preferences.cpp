#include "rr64_character_preferences.hpp"

#include <array>
#include <atomic>
#include <charconv>
#include <cctype>
#include <cstdint>
#include <fstream>
#include <mutex>
#include <string_view>

namespace rr64::character_preferences {
namespace {
constexpr unsigned player_count = 4;
constexpr unsigned rider_count = 45;
constexpr unsigned slot_bits = 6;
constexpr unsigned slot_mask = 63;
constexpr unsigned all_unknown = 0xFFFFFF;

// A packed atomic gives the file writer one coherent snapshot of all players.
std::atomic<unsigned> preferences{all_unknown};
std::atomic<std::uint64_t> revision{0};
std::uint64_t written_revision = 0;
std::filesystem::path settings_path;
std::mutex file_mutex;
bool initialized = false;

void skip_space(std::string_view& text) {
    while (!text.empty() && std::isspace(static_cast<unsigned char>(text.front())))
        text.remove_prefix(1);
}

bool read_number(std::string_view& text, unsigned& value) {
    skip_space(text);
    if (text.empty() || text.front() < '0' || text.front() > '9')
        return false;
    const auto result = std::from_chars(text.data(), text.data() + text.size(), value);
    if (result.ec != std::errc{})
        return false;
    text.remove_prefix(static_cast<std::size_t>(result.ptr - text.data()));
    return text.empty() || std::isspace(static_cast<unsigned char>(text.front()));
}

unsigned load_preferences(const std::filesystem::path& path) {
    std::ifstream file(path, std::ios::binary);
    if (!file)
        return all_unknown;
    // The complete version 1 record fits comfortably below this bound. Reject
    // oversized/truncated records as a unit rather than accepting a partial slot.
    std::array<char, 128> bytes{};
    file.read(bytes.data(), bytes.size());
    if (!file.eof() || file.bad())
        return all_unknown;
    std::string_view text(bytes.data(), static_cast<std::size_t>(file.gcount()));
    // flush() terminates every record with a newline. Require it so truncating
    // the final rider ID (for example 44 to 4) cannot create a false preference.
    if (text.empty() || text.back() != '\n')
        return all_unknown;
    unsigned version = 0, packed = 0;
    if (!read_number(text, version) || version != 1)
        return all_unknown;
    for (unsigned slot = 0; slot < player_count; ++slot) {
        unsigned rider = unknown;
        if (!read_number(text, rider) || (rider >= rider_count && rider != unknown))
            return all_unknown;
        packed |= (rider == unknown ? slot_mask : rider) << (slot * slot_bits);
    }
    skip_space(text);
    return text.empty() ? packed : all_unknown;
}
} // namespace

void initialize(const std::filesystem::path& directory) {
    std::lock_guard lock(file_mutex);
    settings_path = directory / "character-preferences.cfg";
    preferences.store(load_preferences(settings_path), std::memory_order_release);
    revision.store(0, std::memory_order_release);
    written_revision = 0;
    initialized = true;
}

void remember(unsigned slot, unsigned rider) {
    if (slot >= player_count || rider >= rider_count)
        return;
    const unsigned shift = slot * slot_bits;
    unsigned old = preferences.load(std::memory_order_acquire);
    for (;;) {
        const unsigned next = (old & ~(slot_mask << shift)) | (rider << shift);
        if (next == old)
            return;
        if (preferences.compare_exchange_weak(old, next, std::memory_order_acq_rel)) {
            revision.fetch_add(1, std::memory_order_release);
            return;
        }
    }
}

unsigned last(unsigned slot) {
    if (slot >= player_count)
        return unknown;
    const unsigned rider = (preferences.load(std::memory_order_acquire) >> (slot * slot_bits)) & slot_mask;
    return rider < rider_count ? rider : unknown;
}

void flush() {
    std::lock_guard lock(file_mutex);
    const auto current_revision = revision.load(std::memory_order_acquire);
    if (!initialized || current_revision == written_revision)
        return;
    const unsigned snapshot = preferences.load(std::memory_order_acquire);
    std::error_code error;
    if (!settings_path.parent_path().empty())
        std::filesystem::create_directories(settings_path.parent_path(), error);
    if (error)
        return;
    auto temporary = settings_path;
    temporary += ".tmp";
    std::ofstream file(temporary, std::ios::binary | std::ios::trunc);
    file << "1\n";
    for (unsigned slot = 0; slot < player_count; ++slot) {
        const unsigned rider = (snapshot >> (slot * slot_bits)) & slot_mask;
        file << (rider < rider_count ? rider : unknown) << (slot + 1 == player_count ? '\n' : ' ');
    }
    file.close();
    if (!file)
        return;
    // Replace only a complete, closed file. Preserve the prior file on failure.
    std::filesystem::rename(temporary, settings_path, error);
    if (!error)
        written_revision = current_revision;
    // A concurrent remember() advances revision, leaving another flush pending.
}
} // namespace rr64::character_preferences
