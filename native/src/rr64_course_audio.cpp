#include "rr64_course_audio.hpp"
#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstring>
#include <mutex>
#include <stdexcept>
#include <vector>

namespace rr64::course_audio {
namespace {
constexpr unsigned effect_count = unsigned(Effect::Count), queue_size = 128, voice_count = 24;
struct Sample { std::vector<std::int16_t> pcm; unsigned rate = 0; bool loop = false; };
struct Event { Effect effect{}; float gain = 0, pitch = 1; unsigned key = 0, epoch = 0; };
struct Voice {
    unsigned effect = 0, key = 0;
    double cursor = 0, age = 0;
    float gain = 0, target_gain = 0, pitch = 1;
    bool active = false;
};
std::array<Sample,effect_count> samples;
std::array<Voice,voice_count> voices;
std::array<Event,queue_size> events;
std::atomic_uint write_cursor{0}, read_cursor{0}, epoch{1};
std::atomic_bool running{false}, loaded{false};
std::atomic<std::int64_t> heartbeat{0};
std::atomic<std::uint64_t> submitted{0}, dropped{0}, rendered{0};
std::atomic_uint active_voices{0};
std::mutex bank_mutex;
unsigned consumer_epoch = 0;
std::int64_t now_ms() noexcept {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
}
unsigned take_u32(std::span<const std::uint8_t> bytes, std::size_t &at) {
    if (at > bytes.size() || bytes.size()-at < 4) throw std::runtime_error("Truncated course audio");
    const unsigned n = unsigned(bytes[at]) | unsigned(bytes[at+1])<<8 |
                       unsigned(bytes[at+2])<<16 | unsigned(bytes[at+3])<<24;
    at += 4;
    return n;
}
}

void load_bank(std::span<const std::uint8_t> bytes) {
    if (bytes.size()<12 || bytes.size()>16*1024*1024 ||
        std::memcmp(bytes.data(),"R64SFX1\0",8))
        throw std::runtime_error("Invalid course audio bank");
    std::size_t at = 8;
    if (take_u32(bytes,at)!=effect_count) throw std::runtime_error("Course audio effect count");
    std::array<Sample,effect_count> prepared;
    for (unsigned i=0;i<effect_count;++i) {
        const unsigned id=take_u32(bytes,at), rate=take_u32(bytes,at),
                       count=take_u32(bytes,at), loop=take_u32(bytes,at);
        if(id!=i || rate<8000 || rate>96000 || !count || count>rate*8u ||
           loop>1 || std::size_t(count)>((bytes.size()-at)/2))
            throw std::runtime_error("Invalid course audio sample");
        auto &sample=prepared[i]; sample.rate=rate;sample.loop=loop!=0;sample.pcm.resize(count);
        for(unsigned k=0;k<count;++k) {
            sample.pcm[k]=static_cast<std::int16_t>(unsigned(bytes[at])|unsigned(bytes[at+1])<<8);
            at+=2;
        }
    }
    if(at!=bytes.size()) throw std::runtime_error("Course audio trailing data");
    // File parsing/allocation/destruction runs on the loading thread. Mixing
    // only tries this lock, so a load can never stall the audio producer.
    std::lock_guard lock(bank_mutex);
    samples.swap(prepared); voices={};epoch.fetch_add(1,std::memory_order_release);
    loaded.store(true,std::memory_order_release);
}
void unload_bank() noexcept {
    running.store(false,std::memory_order_release);
    std::lock_guard lock(bank_mutex);
    loaded.store(false,std::memory_order_release);epoch.fetch_add(1,std::memory_order_release);
    voices={};samples={};active_voices.store(0,std::memory_order_relaxed);
}
void set_running(bool enabled) noexcept {
    heartbeat.store(now_ms(),std::memory_order_relaxed);
    running.store(enabled,std::memory_order_release);
}
// The world-side reset also clears event-generation observations. This mixer
// half is separate so isolated audio tests need no emulated game state.
void reset_mixer() noexcept {
    running.store(false,std::memory_order_release);
    epoch.fetch_add(1,std::memory_order_release);
}
bool request(Effect effect,float gain,unsigned key,float pitch) noexcept {
    if(!loaded.load(std::memory_order_acquire) || unsigned(effect)>=effect_count ||
       !std::isfinite(gain) || gain<=0 || !std::isfinite(pitch) || pitch<.25f || pitch>4)
        return false;
    const unsigned w=write_cursor.load(std::memory_order_relaxed), next=(w+1)%queue_size;
    if(next==read_cursor.load(std::memory_order_acquire)) {
        dropped.fetch_add(1,std::memory_order_relaxed);return false;
    }
    events[w]={effect,std::clamp(gain,0.f,1.f),pitch,key,epoch.load(std::memory_order_acquire)};
    write_cursor.store(next,std::memory_order_release);submitted.fetch_add(1,std::memory_order_relaxed);
    return true;
}
void mix(std::span<std::int16_t> stereo,unsigned rate) noexcept {
    if(rate<8000 || rate>192000 || stereo.size()%2 || !loaded.load(std::memory_order_acquire)) return;
    std::unique_lock lock(bank_mutex,std::try_to_lock);
    if(!lock.owns_lock()) return;
    const auto current_epoch=epoch.load(std::memory_order_acquire);
    if(current_epoch!=consumer_epoch) {voices={};consumer_epoch=current_epoch;}
    unsigned r=read_cursor.load(std::memory_order_relaxed);
    const unsigned w=write_cursor.load(std::memory_order_acquire);
    for(unsigned n=0;r!=w && n<queue_size;++n,r=(r+1)%queue_size) {
        const auto event=events[r];if(event.epoch!=current_epoch) continue;
        Voice *selected=nullptr;
        if(event.key) for(auto &v:voices) if(v.active && v.key==event.key) {selected=&v;break;}
        if(!selected) for(auto &v:voices) if(!v.active) {selected=&v;break;}
        if(!selected) {dropped.fetch_add(1,std::memory_order_relaxed);continue;}
        if(!selected->active || selected->effect!=unsigned(event.effect))
            *selected={unsigned(event.effect),event.key,0,0,0,event.gain,event.pitch,true};
        selected->age=0;selected->target_gain=event.gain;selected->pitch=event.pitch;
    }
    read_cursor.store(r,std::memory_order_release);
    if(!running.load(std::memory_order_acquire) || now_ms()-heartbeat.load(std::memory_order_relaxed)>300) {
        active_voices.store(0,std::memory_order_relaxed);return;
    }
    const double seconds=1.0/rate;
    for(std::size_t frame=0;frame<stereo.size()/2;++frame) {
        float addition=0;
        for(auto &v:voices) {
            if(!v.active) continue;
            const auto &s=samples[v.effect];
            if(s.pcm.empty()) {v.active=false;continue;}
            v.age+=seconds;
            const float target=s.loop && v.age>.15 ? 0.f:v.target_gain;
            v.gain+=(target-v.gain)*std::min(1.f,120.f/float(rate));
            if(s.loop && target==0 && v.gain<.0001f) {v.active=false;continue;}
            if(v.cursor>=s.pcm.size()) {
                if(!s.loop) {v.active=false;continue;}
                v.cursor=std::fmod(v.cursor,double(s.pcm.size()));
            }
            const auto index=std::size_t(v.cursor);
            const auto next=index+1<s.pcm.size()?index+1:(s.loop?0:index);
            const float fraction=float(v.cursor-index);
            addition+=(s.pcm[index]+(s.pcm[next]-s.pcm[index])*fraction)*v.gain;
            v.cursor+=double(s.rate)*v.pitch/rate;
        }
        for(unsigned channel=0;channel<2;++channel) {
            const int value=int(stereo[frame*2+channel])+int(addition);
            stereo[frame*2+channel]=static_cast<std::int16_t>(std::clamp(value,-32768,32767));
        }
    }
    unsigned count=0;for(const auto &v:voices)count+=v.active;
    active_voices.store(count,std::memory_order_relaxed);
    rendered.fetch_add(stereo.size()/2,std::memory_order_relaxed);
}
Statistics statistics() noexcept {
    return {submitted.load(std::memory_order_relaxed),dropped.load(std::memory_order_relaxed),
            rendered.load(std::memory_order_relaxed),active_voices.load(std::memory_order_relaxed)};
}
} // namespace rr64::course_audio
