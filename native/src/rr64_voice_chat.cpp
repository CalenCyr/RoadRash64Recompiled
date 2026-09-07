#include "rr64_voice_chat.hpp"

#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <cstdio>
#include <deque>
#include <mutex>
#include <vector>

#define SDL_MAIN_HANDLED
#include "SDL.h"
#include "opus.h"

#include "rr64_netplay.hpp"

namespace rr64::voice_chat {
namespace {

constexpr int kVoiceRate = 48000;
constexpr int kFrameSamples = 960; // 20 ms at 48 kHz.
constexpr int kVoiceBitrate = 20000;
constexpr std::size_t kPlaybackPrebufferSamples = kFrameSamples * 3u;
constexpr std::size_t kMaximumPlaybackSamples = kFrameSamples * 12u;
constexpr float kVoiceMixGain = 0.72f;

struct PlaybackState {
    std::deque<std::int16_t> samples{};
    float gain = 0.0f;
    float target_gain = 0.0f;
    double phase = 0.0;
    std::int16_t current = 0;
    std::int16_t next = 0;
    bool primed = false;
};

std::atomic_bool g_enabled = true;
SDL_AudioDeviceID g_capture_device = 0;
OpusEncoder* g_encoder = nullptr;
std::array<OpusDecoder*, netplay::kMaximumPlayers> g_decoders{};
std::array<PlaybackState, netplay::kMaximumPlayers> g_playback{};
std::mutex g_playback_mutex;
bool g_race_active = false;
bool g_capture_failure_reported = false;

void clear_playback() {
    std::lock_guard lock(g_playback_mutex);
    g_playback = {};
}

void close_capture() {
    if (g_capture_device != 0) {
        SDL_PauseAudioDevice(g_capture_device, 1);
        SDL_ClearQueuedAudio(g_capture_device);
        SDL_CloseAudioDevice(g_capture_device);
        g_capture_device = 0;
    }
}

void reset_codecs() {
    if (g_encoder != nullptr) {
        opus_encoder_ctl(g_encoder, OPUS_RESET_STATE);
    }
    for (OpusDecoder* decoder : g_decoders) {
        if (decoder != nullptr) {
            opus_decoder_ctl(decoder, OPUS_RESET_STATE);
        }
    }
}

bool ensure_capture() {
    if (g_encoder == nullptr) {
        int error = OPUS_OK;
        g_encoder = opus_encoder_create(kVoiceRate, 1, OPUS_APPLICATION_VOIP, &error);
        if (g_encoder == nullptr || error != OPUS_OK) {
            if (!g_capture_failure_reported) {
                std::fprintf(stderr, "[RR64-VOICE] Could not create Opus encoder (%d).\n", error);
                g_capture_failure_reported = true;
            }
            return false;
        }
        opus_encoder_ctl(g_encoder, OPUS_SET_BITRATE(kVoiceBitrate));
        opus_encoder_ctl(g_encoder, OPUS_SET_COMPLEXITY(5));
        opus_encoder_ctl(g_encoder, OPUS_SET_SIGNAL(OPUS_SIGNAL_VOICE));
        opus_encoder_ctl(g_encoder, OPUS_SET_INBAND_FEC(1));
    }
    if (g_capture_device != 0) {
        return true;
    }

    SDL_AudioSpec desired{};
    desired.freq = kVoiceRate;
    desired.format = AUDIO_S16SYS;
    desired.channels = 1;
    desired.samples = kFrameSamples;
    desired.callback = nullptr;
    SDL_AudioSpec obtained{};
    g_capture_device = SDL_OpenAudioDevice(nullptr, 1, &desired, &obtained, 0);
    if (g_capture_device == 0) {
        if (!g_capture_failure_reported) {
            std::fprintf(stderr, "[RR64-VOICE] Microphone unavailable: %s\n", SDL_GetError());
            g_capture_failure_reported = true;
        }
        return false;
    }
    g_capture_failure_reported = false;
    SDL_PauseAudioDevice(g_capture_device, 0);
    std::fprintf(stderr, "[RR64-VOICE] Race microphone active at 48 kHz mono.\n");
    return true;
}

OpusDecoder* decoder_for(std::uint8_t slot) {
    if (slot >= g_decoders.size()) {
        return nullptr;
    }
    OpusDecoder*& decoder = g_decoders[slot];
    if (decoder == nullptr) {
        int error = OPUS_OK;
        decoder = opus_decoder_create(kVoiceRate, 1, &error);
        if (error != OPUS_OK) {
            decoder = nullptr;
        }
    }
    return decoder;
}

void capture_frames() {
    if (!ensure_capture()) {
        return;
    }

    constexpr std::uint32_t frame_bytes = kFrameSamples * sizeof(std::int16_t);
    if (SDL_GetQueuedAudioSize(g_capture_device) > frame_bytes * 10u) {
        // A suspended or overloaded process must not resume by transmitting
        // stale speech several hundred milliseconds late.
        SDL_ClearQueuedAudio(g_capture_device);
        return;
    }

    std::array<std::int16_t, kFrameSamples> pcm{};
    std::array<std::uint8_t, netplay::kVoicePayloadCapacity> encoded{};
    while (SDL_GetQueuedAudioSize(g_capture_device) >= frame_bytes) {
        if (SDL_DequeueAudio(g_capture_device, pcm.data(), frame_bytes) != frame_bytes) {
            break;
        }
        const int encoded_size = opus_encode(
            g_encoder,
            pcm.data(),
            kFrameSamples,
            encoded.data(),
            static_cast<opus_int32>(encoded.size()));
        if (encoded_size > 0) {
            netplay::submit_local_voice(std::span<const std::uint8_t>(
                encoded.data(), static_cast<std::size_t>(encoded_size)));
        }
    }
}

void decode_received_frames() {
    std::vector<netplay::ReceivedVoiceFrame> frames = netplay::take_received_voice_frames();
    for (const netplay::ReceivedVoiceFrame& frame : frames) {
        if (frame.speaker_slot >= netplay::kMaximumPlayers || frame.payload_size == 0 ||
            frame.payload_size > frame.payload.size()) {
            continue;
        }
        OpusDecoder* decoder = decoder_for(frame.speaker_slot);
        if (decoder == nullptr) {
            continue;
        }
        std::array<std::int16_t, kFrameSamples> decoded{};
        const int decoded_samples = opus_decode(
            decoder,
            frame.payload.data(),
            frame.payload_size,
            decoded.data(),
            kFrameSamples,
            0);
        if (decoded_samples <= 0) {
            continue;
        }

        std::lock_guard lock(g_playback_mutex);
        PlaybackState& playback = g_playback[frame.speaker_slot];
        if (playback.target_gain <= 0.001f) {
            continue;
        }
        playback.samples.insert(
            playback.samples.end(), decoded.begin(), decoded.begin() + decoded_samples);
        while (playback.samples.size() > kMaximumPlaybackSamples) {
            playback.samples.pop_front();
        }
    }
}

void update_spatial_gains(const netplay::Status& status) {
    netplay::RiderState local{};
    const bool has_local = status.local_slot < netplay::kMaximumPlayers &&
        netplay::get_rider_state(status.local_slot, local);

    std::array<float, netplay::kMaximumPlayers> gains{};
    if (has_local) {
        for (std::uint8_t slot = 0; slot < netplay::kMaximumPlayers; ++slot) {
            if (slot == status.local_slot || !status.players[slot].connected) {
                continue;
            }
            netplay::RiderState remote{};
            if (!netplay::get_rider_state(slot, remote)) {
                continue;
            }
            const float dx = remote.position_x - local.position_x;
            const float dy = remote.position_y - local.position_y;
            const float dz = remote.position_z - local.position_z;
            gains[slot] = proximity_gain(std::sqrt(dx * dx + dy * dy + dz * dz));
        }
    }

    std::lock_guard lock(g_playback_mutex);
    for (std::size_t slot = 0; slot < g_playback.size(); ++slot) {
        g_playback[slot].target_gain = gains[slot];
        if (gains[slot] <= 0.001f) {
            g_playback[slot].samples.clear();
            g_playback[slot].primed = false;
        }
        // Smooth rapid distance changes without keeping an out-of-range rider
        // audible for long after the separation occurs.
        g_playback[slot].gain += (gains[slot] - g_playback[slot].gain) * 0.18f;
    }
}

bool prime_playback(PlaybackState& playback) {
    if (playback.primed) {
        return true;
    }
    if (playback.samples.size() < kPlaybackPrebufferSamples) {
        return false;
    }
    playback.current = playback.samples.front();
    playback.samples.pop_front();
    playback.next = playback.samples.front();
    playback.samples.pop_front();
    playback.phase = 0.0;
    playback.primed = true;
    return true;
}

} // namespace

void set_enabled(bool enabled_value) {
    g_enabled.store(enabled_value, std::memory_order_release);
}

bool enabled() {
    return g_enabled.load(std::memory_order_acquire);
}

void update() {
    const netplay::Status status = netplay::get_status();
    const bool active = enabled() && status.active && status.connected &&
        status.phase == netplay::Phase::Race;
    if (!active) {
        netplay::take_received_voice_frames();
        if (g_race_active) {
            close_capture();
            reset_codecs();
            clear_playback();
            g_race_active = false;
            std::fprintf(stderr, "[RR64-VOICE] Race microphone stopped.\n");
        }
        return;
    }

    g_race_active = true;
    capture_frames();
    update_spatial_gains(status);
    decode_received_frames();
}

void mix(std::span<std::int16_t> stereo_samples, std::uint32_t output_rate) {
    if (!enabled() || output_rate == 0 || stereo_samples.size() < 2) {
        return;
    }

    const double source_step = static_cast<double>(kVoiceRate) /
        static_cast<double>(output_rate);
    std::lock_guard lock(g_playback_mutex);
    for (PlaybackState& playback : g_playback) {
        if (playback.gain <= 0.001f || !prime_playback(playback)) {
            continue;
        }
        for (std::size_t output = 0; output + 1 < stereo_samples.size(); output += 2) {
            const float interpolated = static_cast<float>(playback.current) +
                (static_cast<float>(playback.next) - static_cast<float>(playback.current)) *
                    static_cast<float>(playback.phase);
            const int voice = static_cast<int>(std::lround(
                interpolated * playback.gain * kVoiceMixGain));
            stereo_samples[output] = static_cast<std::int16_t>(std::clamp(
                static_cast<int>(stereo_samples[output]) + voice, -32768, 32767));
            stereo_samples[output + 1] = static_cast<std::int16_t>(std::clamp(
                static_cast<int>(stereo_samples[output + 1]) + voice, -32768, 32767));

            playback.phase += source_step;
            while (playback.phase >= 1.0) {
                playback.phase -= 1.0;
                playback.current = playback.next;
                if (playback.samples.empty()) {
                    playback.primed = false;
                    break;
                }
                playback.next = playback.samples.front();
                playback.samples.pop_front();
            }
            if (!playback.primed) {
                break;
            }
        }
    }
}

void shutdown() {
    close_capture();
    if (g_encoder != nullptr) {
        opus_encoder_destroy(g_encoder);
        g_encoder = nullptr;
    }
    for (OpusDecoder*& decoder : g_decoders) {
        if (decoder != nullptr) {
            opus_decoder_destroy(decoder);
            decoder = nullptr;
        }
    }
    clear_playback();
    g_race_active = false;
}

} // namespace rr64::voice_chat
