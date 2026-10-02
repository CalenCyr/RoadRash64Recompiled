#include "rr64_mk64_item_audio.hpp"
#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstring>
#include <mutex>
#include <vector>

namespace rr64::mk64_items {
namespace {
constexpr unsigned sound_count = unsigned(Sound::Count), queue_capacity = 128, voice_capacity = 16;
constexpr unsigned no_loop = 0xFFFFFFFFu;
struct Sample {
    std::vector<std::int16_t> pcm;
    unsigned rate = 0, frames = 0, channels = 0, loop = no_loop;
};
struct Event {
    Sound sound{};
    unsigned key = 0, epoch = 0, priority = 0;
    float gain = 0, pan = 0;
};
struct Voice {
    Sound sound{};
    unsigned key = 0, priority = 0;
    double cursor = 0, age = 0;
    float gain = 0, target = 0, pan = 0;
    bool active = false;
};
struct MixVoice {
    Voice *voice;
    const Sample *sample;
    double step;
    std::array<float, 2> pan;
    float volume;
};
std::array<Sample, sound_count> samples;
std::array<Voice, voice_capacity> voices;
std::array<Event, queue_capacity> queue;
std::atomic_uint write_cursor{0}, read_cursor{0}, epoch{1};
std::atomic_bool loaded{false}, running{false};
std::atomic<float> duck_target{1}, duck_gain{1};
std::atomic<std::int64_t> heartbeat{0};
std::atomic<std::uint64_t> submitted{0}, dropped{0}, rendered{0};
std::atomic_uint voice_count{0};
std::mutex bank_mutex;
unsigned consumer_epoch = 0, seen_clock = 0;
std::array<unsigned, racer_capacity> seen_identity{}, seen_serial{};
std::array<unsigned, racer_capacity> seen_shrink{};
bool was_highlights = false, observed = false;
std::int64_t now_ms() noexcept {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
               std::chrono::steady_clock::now().time_since_epoch())
        .count();
}
bool enqueue(Sound sound, float gain, float pan, unsigned key, unsigned priority) noexcept {
    if (!loaded.load(std::memory_order_acquire) || unsigned(sound) >= sound_count ||
        !std::isfinite(gain) || gain <= 0 || !std::isfinite(pan))
        return false;
    const unsigned at = write_cursor.load(std::memory_order_relaxed),
                   next = (at + 1) % queue_capacity;
    if (next == read_cursor.load(std::memory_order_acquire)) {
        dropped.fetch_add(1, std::memory_order_relaxed);
        return false;
    }
    queue[at] = {sound,
                 key,
                 epoch.load(std::memory_order_acquire),
                 priority,
                 std::clamp(gain, 0.f, 1.f),
                 std::clamp(pan, -1.f, 1.f)};
    write_cursor.store(next, std::memory_order_release);
    submitted.fetch_add(1, std::memory_order_relaxed);
    return true;
}
struct Spatial {
    float gain = 0, pan = 0;
};
Spatial spatial(const AudioRider &rider, std::span<const AudioListener> listeners) noexcept {
    if (rider.local)
        return {1, 0};
    Spatial result{};
    for (const auto &listener : listeners) {
        float distance2 = 0, lateral = 0, right2 = 0;
        for (unsigned axis = 0; axis < 3; ++axis) {
            const float delta = rider.position[axis] - listener.position[axis];
            distance2 += delta * delta;
            lateral += delta * listener.right[axis];
            right2 += listener.right[axis] * listener.right[axis];
        }
        if (!std::isfinite(distance2) || !std::isfinite(lateral) || !std::isfinite(right2) ||
            right2 < .01f)
            continue;
        const float distance = std::sqrt(distance2), falloff = std::max(0.f, 1.f - distance / 80.f);
        const float gain = falloff * falloff;
        if (gain > result.gain)
            result = {gain, distance > .01f
                                ? std::clamp(lateral / (distance * std::sqrt(right2)), -1.f, 1.f)
                                : 0};
    }
    return result;
}
} // namespace

bool install_audio_bank(std::span<const std::uint8_t> bytes, std::string &error) {
    const auto fail = [&] {
        error = "Invalid or incomplete original MK64 item audio bank";
        return false;
    };
    if (bytes.size() < 12 || bytes.size() > 16 * 1024 * 1024 ||
        std::memcmp(bytes.data(), "R64ISFX1", 8))
        return fail();
    std::size_t at = 8;
    const auto word = [&]() {
        if (at + 4 > bytes.size()) {
            at = bytes.size() + 1;
            return 0xFFFFFFFFu;
        }
        const unsigned n = unsigned(bytes[at]) | unsigned(bytes[at + 1]) << 8 |
                           unsigned(bytes[at + 2]) << 16 | unsigned(bytes[at + 3]) << 24;
        at += 4;
        return n;
    };
    if (word() != sound_count)
        return fail();
    std::array<Sample, sound_count> prepared;
    for (unsigned i = 0; i < sound_count; ++i) {
        const unsigned id = word(), rate = word(), frames = word(), channels = word(),
                       loop = word();
        const bool song = i == unsigned(Sound::StarMusic);
        if (id != i || rate != 26800 || !frames || frames > rate * (song ? 120u : 8u) ||
            channels != (song ? 2u : 1u) || (loop != no_loop && loop >= frames) ||
            at > bytes.size() || std::uint64_t(frames) * channels * 2u > bytes.size() - at)
            return fail();
        constexpr std::array<bool, sound_count> loops{false, true, false, false, false, false, true,
                                                      true,  true, true,  false, false, true};
        if ((loop != no_loop) != loops[i])
            return fail();
        auto &sample = prepared[i];
        sample.rate = rate;
        sample.frames = frames;
        sample.channels = channels;
        sample.loop = loop;
        sample.pcm.resize(std::size_t(frames) * channels);
        for (auto &value : sample.pcm) {
            value = std::int16_t(unsigned(bytes[at]) | unsigned(bytes[at + 1]) << 8);
            at += 2;
        }
    }
    if (at != bytes.size())
        return fail();
    std::lock_guard lock(bank_mutex);
    samples.swap(prepared);
    voices = {};
    epoch.fetch_add(1, std::memory_order_release);
    loaded.store(true, std::memory_order_release);
    error.clear();
    return true;
}
void clear_audio_bank() noexcept {
    running.store(false, std::memory_order_release);
    std::lock_guard lock(bank_mutex);
    loaded.store(false, std::memory_order_release);
    epoch.fetch_add(1, std::memory_order_release);
    samples = {};
    voices = {};
    voice_count.store(0, std::memory_order_relaxed);
    duck_target.store(1, std::memory_order_relaxed);
    duck_gain.store(1, std::memory_order_relaxed);
}
bool audio_available() noexcept {
    return loaded.load(std::memory_order_acquire);
}
void reset_audio() noexcept {
    running.store(false, std::memory_order_release);
    epoch.fetch_add(1, std::memory_order_release);
    seen_identity = {};
    seen_serial = {};
    seen_shrink = {};
    seen_clock = 0;
    was_highlights = false;
    observed = false;
    duck_target.store(1, std::memory_order_relaxed);
}
void present_audio(const Snapshot &state, std::span<const AudioRider> riders,
                   std::span<const AudioListener> listeners, bool enabled,
                   bool highlights) noexcept {
    heartbeat.store(now_ms(), std::memory_order_relaxed);
    if (!state.enabled || !valid(state) || riders.size() != racer_capacity || listeners.empty() ||
        listeners.size() > 4 || highlights) {
        if (observed || highlights != was_highlights || running.load(std::memory_order_relaxed))
            reset_audio();
        was_highlights = highlights;
        running.store(false, std::memory_order_release);
        duck_target.store(1, std::memory_order_relaxed);
        return;
    }
    if (state.clock < seen_clock || was_highlights)
        reset_audio();
    was_highlights = false;
    seen_clock = state.clock;
    observed = true;
    running.store(enabled, std::memory_order_release);
    if (!enabled)
        return; // Freeze cursors and retain event observations through pause.
    bool local_star = false, local_boo = false;
    for (unsigned slot = 0; slot < racer_capacity; ++slot) {
        const auto &rider = riders[slot];
        const auto &item = state.riders[slot];
        if (!rider.identity) {
            seen_identity[slot] = seen_serial[slot] = seen_shrink[slot] = 0;
            continue;
        }
        const auto space = spatial(rider, listeners);
        if (seen_identity[slot] != rider.identity) {
            seen_identity[slot] = rider.identity;
            seen_serial[slot] = item.event_serial;
            seen_shrink[slot] = item.shrink_until;
        } else if (item.event_serial != seen_serial[slot]) {
            seen_serial[slot] = item.event_serial;
            Sound sound = Sound::Hit;
            bool one_shot = true;
            switch (item.cue) {
            case Cue::Shell:
                sound = Sound::Shell;
                break;
            case Cue::Banana:
            case Cue::FakeBox:
                sound = Sound::Banana;
                break;
            case Cue::Mushroom:
                sound = Sound::Mushroom;
                break;
            case Cue::Lightning:
                sound = Sound::Lightning;
                break;
            case Cue::Boo:
                sound = Sound::BooStart;
                break;
            case Cue::Star:
            case Cue::None:
                one_shot = false;
                break;
            default:
                break;
            }
            if (one_shot)
                enqueue(sound, space.gain, space.pan, 0, rider.local ? 3 : 2);
        }
        const bool star = item.star_until > state.clock, boo = item.boo_until > state.clock;
        if (item.shrink_until > seen_shrink[slot] && rider.local)
            enqueue(Sound::LightningImpact, 1, 0, 0, 3);
        seen_shrink[slot] = item.shrink_until;
        local_star |= rider.local && star;
        local_boo |= rider.local && boo;
        if (star && !rider.local)
            enqueue(Sound::StarDistant, space.gain * .4f, space.pan, 0x100u + slot, 1);
        if (boo && rider.local)
            enqueue(Sound::BooLoop, .45f, 0, 0x200u + slot, 1);
    }
    // One Star score per shared audio mix, even with several local views.
    if (local_star)
        enqueue(Sound::StarMusic, 1, 0, 0x300, 4);
    duck_target.store(local_star ? 0.f : local_boo ? .35f : 1.f, std::memory_order_relaxed);
    unsigned audible_shells = 0;
    for (const auto &object : state.objects) {
        if (!object.generation || !shell(object.kind) || object.mode != ObjectMode::Flying)
            continue;
        const auto space = spatial({object.position, object.generation, false}, listeners);
        if (space.gain < .03f || audible_shells >= 4)
            continue;
        ++audible_shells;
        enqueue(Sound::ShellMotion, space.gain * .18f, space.pan, 0x40000000u | object.generation,
                0);
        if (object.target < racer_capacity && riders[object.target].local)
            enqueue(Sound::Warning, .35f, 0, 0x400u + object.target, 2);
    }
}
float item_music_gain() noexcept {
    return duck_gain.load(std::memory_order_relaxed);
}
void mix_audio(std::span<std::int16_t> stereo, unsigned rate, float sfx_volume,
               float music_volume) noexcept {
    if (rate < 8000 || rate > 192000 || (stereo.size() & 1u) || !std::isfinite(sfx_volume) ||
        !std::isfinite(music_volume))
        return;
    sfx_volume = std::clamp(sfx_volume, 0.f, 1.f);
    music_volume = std::clamp(music_volume, 0.f, 1.f);
    const bool active = running.load(std::memory_order_acquire) &&
                        now_ms() - heartbeat.load(std::memory_order_relaxed) <= 300 &&
                        loaded.load(std::memory_order_acquire);
    const float target = active ? duck_target.load(std::memory_order_relaxed) : 1.f;
    float duck = duck_gain.load(std::memory_order_relaxed);
    duck += (target - duck) * (1.f - std::exp(-float(stereo.size() / 2) / float(rate) * 12.f));
    duck_gain.store(duck, std::memory_order_relaxed);
    std::unique_lock lock(bank_mutex, std::try_to_lock);
    if (!lock.owns_lock())
        return;
    const auto current_epoch = epoch.load(std::memory_order_acquire);
    if (consumer_epoch != current_epoch) {
        voices = {};
        consumer_epoch = current_epoch;
    }
    unsigned read = read_cursor.load(std::memory_order_relaxed);
    const unsigned write = write_cursor.load(std::memory_order_acquire);
    for (unsigned n = 0; read != write && n < queue_capacity;
         ++n, read = (read + 1) % queue_capacity) {
        const auto event = queue[read];
        if (event.epoch != current_epoch)
            continue;
        Voice *chosen = nullptr;
        if (event.key)
            for (auto &voice : voices)
                if (voice.active && voice.key == event.key) {
                    chosen = &voice;
                    break;
                }
        if (!chosen)
            for (auto &voice : voices)
                if (!voice.active) {
                    chosen = &voice;
                    break;
                }
        if (!chosen)
            for (auto &voice : voices)
                if (voice.priority < event.priority &&
                    (!chosen || voice.priority < chosen->priority ||
                     (voice.priority == chosen->priority && voice.gain < chosen->gain)))
                    chosen = &voice;
        if (!chosen) {
            dropped.fetch_add(1, std::memory_order_relaxed);
            continue;
        }
        if (!chosen->active || chosen->key != event.key || !event.key ||
            chosen->sound != event.sound)
            *chosen = {event.sound, event.key,  event.priority, 0,   0,
                       0,           event.gain, event.pan,      true};
        chosen->age = 0;
        chosen->target = event.gain;
        chosen->pan = event.pan;
    }
    read_cursor.store(read, std::memory_order_release);
    if (!active) {
        voice_count.store(0, std::memory_order_relaxed);
        return;
    }
    // Events are consumed before mixing, and bank_mutex holds the samples and
    // voice parameters stable for this entire buffer. Prepare only occupied
    // slots in their original order so sample accumulation stays identical.
    std::array<MixVoice, voice_capacity> mixing;
    unsigned mixing_count = 0;
    for (auto &voice : voices) {
        if (!voice.active)
            continue;
        const auto &sample = samples[unsigned(voice.sound)];
        const std::array<float, 2> pan = sample.channels == 2
                                             ? std::array{1.f, 1.f}
                                             : std::array{std::sqrt((1.f - voice.pan) * .5f),
                                                          std::sqrt((1.f + voice.pan) * .5f)};
        mixing[mixing_count++] = {&voice, &sample, double(sample.rate) / rate, pan,
                                  voice.sound == Sound::StarMusic ? music_volume * .6f
                                                                  : sfx_volume * .35f};
    }
    const double seconds = 1.0 / rate;
    const float smoothing = std::min(1.f, 90.f / rate);
    for (std::size_t frame = 0; frame < stereo.size() / 2; ++frame) {
        std::array<float, 2> addition{};
        for (unsigned slot = 0; slot < mixing_count; ++slot) {
            const auto &mix = mixing[slot];
            auto &voice = *mix.voice;
            if (!voice.active)
                continue;
            const auto &sample = *mix.sample;
            if (!sample.frames) {
                voice.active = false;
                continue;
            }
            const bool loops = sample.loop != no_loop;
            voice.age += seconds;
            const float desired = loops && voice.age > .15 ? 0 : voice.target;
            voice.gain += (desired - voice.gain) * smoothing;
            if (loops && desired == 0 && voice.gain < .0001f) {
                voice.active = false;
                continue;
            }
            if (voice.cursor >= sample.frames) {
                if (!loops) {
                    voice.active = false;
                    continue;
                }
                voice.cursor = sample.loop +
                               std::fmod(voice.cursor - sample.loop, sample.frames - sample.loop);
            }
            const auto index = unsigned(voice.cursor);
            const unsigned next = index + 1 < sample.frames ? index + 1
                                  : loops                   ? sample.loop
                                                            : index;
            const float fraction = float(voice.cursor - index);
            for (unsigned channel = 0; channel < 2; ++channel) {
                const unsigned source_channel = sample.channels == 2 ? channel : 0;
                const float first = sample.pcm[index * sample.channels + source_channel];
                const float second = sample.pcm[next * sample.channels + source_channel];
                addition[channel] += (first + (second - first) * fraction) * voice.gain *
                                     mix.pan[channel] * mix.volume;
            }
            voice.cursor += mix.step;
        }
        // Dedicated headroom for simultaneous items; native effects retain
        // their own allocation and samples. Saturation is the last boundary.
        for (unsigned channel = 0; channel < 2; ++channel) {
            const float mixed = std::clamp(addition[channel], -10000.f, 10000.f);
            stereo[frame * 2 + channel] = std::int16_t(
                std::clamp(int(stereo[frame * 2 + channel]) + int(mixed), -32768, 32767));
        }
    }
    unsigned count = 0;
    for (const auto &voice : voices)
        count += voice.active;
    voice_count.store(count, std::memory_order_relaxed);
    rendered.fetch_add(stereo.size() / 2, std::memory_order_relaxed);
}
AudioStatistics audio_statistics() noexcept {
    return {submitted.load(std::memory_order_relaxed), dropped.load(std::memory_order_relaxed),
            rendered.load(std::memory_order_relaxed), voice_count.load(std::memory_order_relaxed)};
}
} // namespace rr64::mk64_items
