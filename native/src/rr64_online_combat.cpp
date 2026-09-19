#include "recomp.h"
#include "rr64_netplay.hpp"
#include "rr64_engine_layout.hpp"
#include "rr64_attack_visual_memory.hpp"
#include "rr64_prediction_rules.hpp"
#include <bit>
extern "C" void func_80061224(unsigned char*,recomp_context*);
extern "C" void func_800616BC(unsigned char*,recomp_context*);
extern "C" int rr64_valid_combat_statistics(unsigned int address) {
    // Shared by both native assist-credit paths, in offline and online races.
    return rr64::engine::valid_combat_statistics(address);
}
namespace {
thread_local bool replaying=false;
unsigned actor_for(unsigned slot,const rr64::netplay::Status &s) {
    return 0x800d8570u+rr64::online_flow::mapped_slot(slot,s.local_slot,s.replicated_riders)*0x118u;
}
unsigned slot_for(unsigned actor,const rr64::netplay::Status &s) {
    for(unsigned i=0;i<rr64::netplay::kMaximumPlayers;++i) if(actor_for(i,s)==actor) return i;
    return rr64::netplay::kMaximumPlayers;
}
bool pair(unsigned char *m,unsigned actor,unsigned &rider) {
    using namespace rr64::engine;unsigned bike=0,linked=0;
    return read_u32(m,actor+0xe4,rider) && valid_guest_range(rider,rider::stride) &&
        read_u32(m,actor+0xe0,bike) && valid_guest_range(bike,bike::stride) &&
        read_u32(m,bike+bike::rider_pointer,linked) && linked==rider &&
        read_u32(m,rider+rider::bike_pointer,linked) && linked==bike;
}
}
// Return one to suppress the locally discovered outcome. Only the attacking
// owner's native collision result becomes a proposal; remote simulations cannot
// independently damage this same target again.
extern "C" int rr64_online_hit(unsigned char *m,void *context,unsigned kind) {
    using namespace rr64;
    if(replaying || !context) return 0;
    if(prediction::active()){
        // Match live authority ownership during private replay too. Returning
        // an empty transport Status must not accidentally enable client damage
        // that was suppressed in the original predicted step.
        const auto rules=prediction::status_for_rules();
        return rules.active && rules.connected && rules.authoritative && !rules.is_host ? 1 : 0;
    }
    const auto s=netplay::get_status();
    if(!s.active || !s.connected || s.phase!=netplay::Phase::Race) return 0;
    // In host authority the native host collision is the decision. Clients
    // present replicated outcomes; they must not submit owner hit proposals.
    if(s.authoritative) return s.is_host ? 0 : 1;
    auto &ctx=*static_cast<recomp_context*>(context);
    netplay::HitEvent e{};e.round=s.game_setup.revision;e.kind=kind;
    const unsigned a=static_cast<unsigned>(ctx.r4),v=static_cast<unsigned>(ctx.r5);
    const unsigned attacker=slot_for(a,s),victim=slot_for(v,s);
    if(attacker>=netplay::kMaximumPlayers || victim>=netplay::kMaximumPlayers) return 1;
    unsigned ar=0,vr=0;if(!pair(m,a,ar) || !pair(m,v,vr)) return 1;
    if(attacker!=s.local_slot && !(s.is_host && !s.players[attacker].connected)) return 1;
    e.attacker=attacker;e.victim=victim;e.strength=std::bit_cast<float>(static_cast<unsigned>(ctx.r6));
    e.attack=attack_visual::capture(m,ar);
    if(kind==0) {
        // Contact can land on the final animation tick. Capture the descriptor
        // actually consumed by61224 even if its visual phase has reached zero.
        unsigned descriptor=0,equipment=0;
        engine::read_u32(m,ar+0x568,descriptor);engine::read_u32(m,ar+0x564,equipment);
        if(descriptor<attack_visual::descriptor_base ||
           (descriptor-attack_visual::descriptor_base)%attack_visual::descriptor_stride ||
           (descriptor-attack_visual::descriptor_base)/attack_visual::descriptor_stride>=48 ||
           equipment<1 || equipment>14) return 1;
        e.attack.descriptor=(descriptor-attack_visual::descriptor_base)/attack_visual::descriptor_stride;
        e.attack.equipment=equipment;
    }
    netplay::submit_hit(e);
    return 1;
}
// Called on the game thread before publishing this frame's owning rider state.
// The native handler keeps its damage curves, knockdown rules and cop hooks.
extern "C" void rr64_online_combat_drain(unsigned char *m,void *context) {
    using namespace rr64;
    if(!context || prediction::active()) return;
    const auto s=netplay::get_status();
    if(!s.active || !s.connected || s.phase!=netplay::Phase::Race) return;
    if(s.authoritative) return;
    auto &parent=*static_cast<recomp_context*>(context);
    const unsigned sp=static_cast<unsigned>(parent.r29);
    if(sp<2048 || !engine::valid_guest_range(sp-2048,2048)) return;
    netplay::HitEvent e{};
    for(unsigned n=0;n<64 && netplay::take_hit(e);++n) {
        if(e.round!=s.game_setup.revision || e.attacker>=netplay::kMaximumPlayers ||
           e.victim>=netplay::kMaximumPlayers ||
           (e.victim!=s.local_slot && !(s.is_host && !s.players[e.victim].connected))) continue;
        const unsigned a=actor_for(e.attacker,s),v=actor_for(e.victim,s);
        unsigned ar=0,vr=0;if(!pair(m,a,ar) || !pair(m,v,vr)) continue;
        attack_visual::Saved saved;
        if(e.kind==0 && !saved.apply(m,ar,e.attack)) continue;
        recomp_context call=parent;
        const auto guest=[](unsigned x){return static_cast<gpr>(static_cast<std::int64_t>(static_cast<std::int32_t>(x)));};
        call.r29=guest(sp-64);call.r4=guest(a);call.r5=guest(v);call.r6=std::bit_cast<unsigned>(e.strength);
        replaying=true;
        if(e.kind==0) func_80061224(m,&call);else func_800616BC(m,&call);
        replaying=false;saved.restore(m);
    }
}
