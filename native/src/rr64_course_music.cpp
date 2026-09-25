#include "rr64_course_music.hpp"
#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <cstring>
#include <mutex>
#include <stdexcept>
#include <vector>

namespace rr64::course_music {
namespace {
constexpr int kNoSong = -1;
constexpr std::size_t kSongs = 11;
constexpr std::size_t kCourses = 16;
struct Song {
    std::uint32_t id = 0, rate = 0, frames = 0, loop = 0;
    std::vector<std::int16_t> pcm;
};
struct Course { std::array<char, 32> id{}; std::uint32_t song = 0; };
struct Bank { std::array<Song, kSongs> songs; std::array<Course, kCourses> courses; };
Bank bank;
std::mutex bank_mutex;
std::atomic<bool> loaded{false}, enabled{false}, active{false};
std::atomic<int> requested{kNoSong};
std::atomic<std::uint64_t> epoch{1};
std::atomic<std::uint64_t> restart_epoch{1};
std::atomic<float> reported_gain{0};
std::atomic<std::uint64_t> reported_frames{0}, reported_loops{0};
std::atomic<std::uint32_t> reported_song{0};
// Audio callback owns these fields; load/unload only publishes an epoch.
int current = kNoSong;
double cursor = 0;
float gain = 0;
std::uint64_t observed_epoch = 0, frames_played = 0, loops_played = 0;
std::uint64_t observed_restart = 0;
bool restart_pending = false;

std::uint32_t read_u32(std::span<const std::uint8_t> bytes, std::size_t& at) {
    if (at > bytes.size() || bytes.size() - at < 4) throw std::runtime_error("Truncated course music bank");
    const auto value = std::uint32_t(bytes[at]) | (std::uint32_t(bytes[at+1]) << 8) |
                       (std::uint32_t(bytes[at+2]) << 16) | (std::uint32_t(bytes[at+3]) << 24);
    at += 4;
    return value;
}
}

void load_bank(std::span<const std::uint8_t> bytes) {
    if (bytes.size() < 16 || bytes.size() > kMaximumBankBytes ||
        std::memcmp(bytes.data(), "R64MUS1\0", 8) != 0) throw std::runtime_error("Invalid course music bank");
    std::size_t at = 8;
    if (read_u32(bytes, at) != kSongs || read_u32(bytes, at) != kCourses)
        throw std::runtime_error("Invalid course music catalogue counts");
    Bank parsed;
    for (auto& course : parsed.courses) {
        if (at > bytes.size() || bytes.size() - at < course.id.size()) throw std::runtime_error("Truncated course music name");
        std::memcpy(course.id.data(), bytes.data() + at, course.id.size());
        at += course.id.size();
        if (course.id.front() == '\0' || course.id.back() != '\0') throw std::runtime_error("Invalid course music name");
        bool ended = false;
        for (char c : course.id) {
            if (c == '\0') { ended = true; continue; }
            if (ended || !((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_'))
                throw std::runtime_error("Invalid course music name");
        }
        course.song = read_u32(bytes, at);
    }
    for (std::size_t i = 0; i < kSongs; ++i) {
        auto& song = parsed.songs[i];
        song.id = read_u32(bytes, at); song.rate = read_u32(bytes, at);
        song.frames = read_u32(bytes, at); song.loop = read_u32(bytes, at);
        if (song.rate < 8000 || song.rate > 96000 || song.frames < 2 || song.loop >= song.frames ||
            song.frames > std::uint64_t(song.rate) * 300 ||
            std::uint64_t(song.frames) * 4 > bytes.size() - at)
            throw std::runtime_error("Invalid course music sample bounds");
        for (std::size_t j = 0; j < i; ++j) if (parsed.songs[j].id == song.id)
            throw std::runtime_error("Duplicate course music song");
        song.pcm.resize(std::size_t(song.frames) * 2);
        for (auto& sample : song.pcm) {
            sample = static_cast<std::int16_t>(std::uint16_t(bytes[at]) | (std::uint16_t(bytes[at+1]) << 8));
            at += 2;
        }
    }
    if (at != bytes.size()) throw std::runtime_error("Trailing course music data");
    for (std::size_t i = 0; i < kCourses; ++i) {
        auto& course = parsed.courses[i];
        for (std::size_t j = 0; j < i; ++j) if (course.id == parsed.courses[j].id)
            throw std::runtime_error("Duplicate course music mapping");
        const auto it = std::find_if(parsed.songs.begin(), parsed.songs.end(),
            [&](const Song& song) { return song.id == course.song; });
        if (it == parsed.songs.end()) throw std::runtime_error("Missing course music song");
        course.song = static_cast<std::uint32_t>(it - parsed.songs.begin());
    }
    std::lock_guard lock(bank_mutex);
    bank = std::move(parsed);
    requested.store(kNoSong);
    epoch.fetch_add(1);
    loaded.store(true);
}

void unload_bank() noexcept {
    loaded.store(false); enabled.store(false); active.store(false);
    requested.store(kNoSong); reported_gain.store(0);
    std::lock_guard lock(bank_mutex);
    bank = Bank{};
    epoch.fetch_add(1);
}
void select_course(std::string_view id) noexcept {
    int selected = kNoSong;
    std::lock_guard lock(bank_mutex);
    if (loaded.load()) for (const auto& course : bank.courses) {
        if (id == std::string_view(course.id.data())) { selected = static_cast<int>(course.song); break; }
    }
    requested.store(selected);
}
void set_enabled(bool value) noexcept { enabled.store(value); }
void set_active(bool value) noexcept { active.store(value); }
void reset_runtime() noexcept {
    active.store(false); enabled.store(false); requested.store(kNoSong);
    restart_epoch.fetch_add(1);
}
bool available() noexcept { return loaded.load(); }
float replacement_gain() noexcept { return reported_gain.load(); }
Statistics statistics() noexcept {
    return {reported_frames.load(), reported_loops.load(), reported_song.load(), reported_gain.load()};
}

void mix(std::span<std::int16_t> output, std::uint32_t rate, float volume) noexcept {
    if (rate < 8000 || rate > 192000 || output.size() % 2 != 0 || !std::isfinite(volume)) return;
    // Loading holds this lock briefly only to publish a fully validated bank.
    // The callback must never wait on disk parsing or allocations.
    std::unique_lock lock(bank_mutex, std::try_to_lock);
    if (!lock.owns_lock()) { reported_gain.store(0); return; }
    const auto now_epoch = epoch.load();
    if (now_epoch != observed_epoch) {
        current = kNoSong; cursor = 0; gain = 0; observed_epoch = now_epoch;
        frames_played = loops_played = 0;
        observed_restart = restart_epoch.load(); restart_pending = false;
    }
    if (const auto serial = restart_epoch.load(); serial != observed_restart) {
        observed_restart = serial; restart_pending = true;
    }
    const int desired = loaded.load() && enabled.load() && active.load() ? requested.load() : kNoSong;
    volume = std::clamp(volume, 0.0f, 2.0f);
    const float fade_step = 1.0f / (static_cast<float>(rate) * .125f);
    for (std::size_t i = 0; i + 1 < output.size(); i += 2) {
        if ((current != desired || restart_pending) && gain == 0) {
            current = desired; cursor = 0; restart_pending = false;
        }
        const float target = !restart_pending && current == desired && current != kNoSong ? 1.0f : 0.0f;
        gain = target > gain ? std::min(target, gain + fade_step) : std::max(target, gain - fade_step);
        if (current == kNoSong || gain <= 0) continue;
        const auto& song = bank.songs[static_cast<std::size_t>(current)];
        if (cursor >= song.frames) {
            cursor = song.loop + std::fmod(cursor - song.frames, double(song.frames - song.loop));
            ++loops_played;
        }
        const auto frame = static_cast<std::uint32_t>(cursor);
        const auto next = frame + 1 < song.frames ? frame + 1 : song.loop;
        const float fraction = static_cast<float>(cursor - frame);
        for (std::size_t channel = 0; channel < 2; ++channel) {
            const float a = song.pcm[std::size_t(frame)*2+channel];
            const float b = song.pcm[std::size_t(next)*2+channel];
            const float value = output[i+channel] + (a + (b-a)*fraction) * gain * volume;
            output[i+channel] = static_cast<std::int16_t>(std::clamp(value, -32768.0f, 32767.0f));
        }
        cursor += double(song.rate) / rate;
        ++frames_played;
    }
    reported_gain.store(gain);
    reported_song.store(current == kNoSong ? 0 : bank.songs[static_cast<std::size_t>(current)].id);
    reported_frames.store(frames_played); reported_loops.store(loops_played);
}
}
