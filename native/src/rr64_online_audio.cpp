#include "rr64_engine_layout.hpp"
#include "rr64_netplay.hpp"
#include "rr64_prediction_replay.hpp"
#include <algorithm>
#include <bit>
#include <cmath>

namespace {
constexpr unsigned actor_base = 0x800D8570u;
constexpr unsigned actor_stride = 0x118u;
struct WeaponCue {
    unsigned char *memory=nullptr;
    void *context=nullptr;
    gpr stack=0;
    float gain=0;
};
thread_local WeaponCue weapon_cue;

bool listener(unsigned char *memory, unsigned &actor, unsigned &body) {
    if (!memory || rr64::prediction::active()) return false;
    const auto rules = rr64::netplay::get_physics_rules();
    if (!rules.active || !rules.connected || rules.phase != rr64::netplay::Phase::Race ||
        rules.local_slot >= (rules.replicated_riders ? 14u : 4u)) return false;
    actor = actor_base + (rules.replicated_riders ? 0u : rules.local_slot) * actor_stride;
    unsigned owner = 0;
    return rr64::engine::read_u32(memory, actor + 0xE4u, body) &&
           (body & 3u) == 0 &&
           rr64::engine::valid_guest_range(body, rr64::engine::rider::stride) &&
           rr64::engine::read_u32(memory, body + 4u, owner) && owner == actor;
}
}

extern "C" void rr64_online_audio_owner(unsigned char *memory, void *context) {
    if (!context) return;
    unsigned actor = 0, body = 0;
    if (!listener(memory, actor, body)) return;
    auto &ctx = *static_cast<recomp_context *>(context);
    const unsigned source = static_cast<unsigned>(ctx.r2);
    if (source < actor_base || (source - actor_base) % actor_stride != 0 ||
        (source - actor_base) / actor_stride >= rr64::engine::kMaximumRacers) return;
    // 59324..59648 use actor+8 >= 0 as the split-screen local-audio test.
    // Online has one listener. Change only this branch register; native human
    // control, engine production, actor identity and cache ownership stay intact.
    ctx.r3 = source == actor ? 0 : static_cast<gpr>(-1);
}

extern "C" void rr64_online_audio_listener(unsigned char *memory, void *context) {
    if (!context) return;
    unsigned actor = 0, body = 0;
    if (!listener(memory, actor, body)) return;
    // Native distance callers load actor zero's rider, then add +8C in their
    // delay slot. Keep the native source position, distance math and ROM gain.
    static_cast<recomp_context *>(context)->r4 = rr64::engine::guest_address(body);
}

extern "C" void rr64_online_audio_weapon_source(unsigned char *memory, void *context,
                                                 unsigned source_body) {
    // The two native weapon-cycle callers immediately enter 56000, whose
    // 0x20-byte frame calls 58600 exactly once. Bind only that next cue; do not
    // change the native selected weapon, effect choice, pan, priority or pitch.
    weapon_cue={};
    if(!context)return;
    unsigned actor=0,body=0,source_actor=0,owned_body=0;
    if(!listener(memory,actor,body) || (source_body&3u) ||
       !rr64::engine::valid_guest_range(source_body,rr64::engine::rider::stride) ||
       !rr64::engine::read_u32(memory,source_body+4,source_actor) ||
       source_actor<actor_base || (source_actor-actor_base)%actor_stride ||
       (source_actor-actor_base)/actor_stride>=rr64::engine::kMaximumRacers ||
       !rr64::engine::read_u32(memory,source_actor+0xE4,owned_body) ||
       owned_body!=source_body || source_actor==actor)return;
    float listener_x=0,listener_y=0,source_x=0,source_y=0;
    float distance_scale=0,one=0,maximum=0;
    if(!rr64::engine::read_float(memory,body+0x8C,listener_x) ||
       !rr64::engine::read_float(memory,body+0x90,listener_y) ||
       !rr64::engine::read_float(memory,source_body+0x8C,source_x) ||
       !rr64::engine::read_float(memory,source_body+0x90,source_y) ||
       !rr64::engine::read_float(memory,0x80005C90,distance_scale) ||
       !rr64::engine::read_float(memory,0x80005C94,one) ||
       !rr64::engine::read_float(memory,0x80005C98,maximum))return;
    // Same horizontal distance, constants and operation order as 1295C /
    // 59588. Reading the native constants avoids creating a second sound curve.
    const float dx=listener_x-source_x,dy=listener_y-source_y;
    const float distance=std::sqrt(dx*dx+dy*dy);
    const float remaining=one-distance*distance_scale;
    const float gain=remaining*maximum;
    if(!std::isfinite(gain) || !std::isfinite(distance) ||
       !std::isfinite(distance_scale) || distance_scale<=0 ||
       !std::isfinite(maximum) || maximum<=0)return;
    const auto &ctx=*static_cast<recomp_context *>(context);
    weapon_cue={memory,context,static_cast<gpr>(ADD32(ctx.r29,-0x20)),std::clamp(gain,0.0f,255.0f)};
}

extern "C" void rr64_online_audio_weapon_gain(unsigned char *memory, void *context) {
    const auto cue=weapon_cue;
    weapon_cue={};
    if(!context || memory!=cue.memory || context!=cue.context || rr64::prediction::active())return;
    auto &ctx=*static_cast<recomp_context *>(context);
    const auto effect=static_cast<unsigned>(ctx.r4);
    if(ctx.r29!=cue.stack || ctx.r7!=0 ||
       (effect!=0xE8 && effect!=0xED && effect!=0xE9 && effect!=0xEC && effect!=0xEB))return;
    // 56000 supplies the same fixed float for gain and pan. Attenuate only the
    // gain argument after its delay slot has copied the original pan argument.
    const float original=std::bit_cast<float>(static_cast<unsigned>(ctx.r5));
    if(std::isfinite(original) && original>=0)
        ctx.r5=static_cast<std::int32_t>(std::bit_cast<unsigned>(std::min(original,cue.gain)));
}
