#include "rr64_netplay.hpp"
#include "rr64_mk64_items.hpp"
#include "rr64_engine_layout.hpp"
#include <atomic>
#include <bit>
#include <cstdio>
#include <cstdlib>
#include <source_location>
#include <vector>

namespace {
unsigned checks=0,queued=0,eject_calls=0,capture_calls=0,failures=0;
bool disabled=false,focused=true,shortcuts=true,imported=true,gate=true,eject_valid=true;
std::array<bool,4> input_held{};
std::atomic_uint32_t rider_eject_requests{0},item_requests{0};
std::vector<std::pair<unsigned,int>> uses;
rr64::netplay::Status status{};
rr64::authority::HostRound host_round;
rr64::authority::Command accepted_command{};
void check(bool value,std::source_location at=std::source_location::current()) {
    ++checks;if(!value){std::fprintf(stderr,"MK64 input failed at line %u\n",at.line());std::exit(1);}
}
}
namespace recompinput {
enum class GameInput { RR64_MK64_USE_ITEM };
bool game_input_disabled(){return disabled;}
bool game_window_focused(){return focused;}
namespace profiles {bool get_action_input(int profile,GameInput){return input_held[profile];}}
}
bool rr64_are_gameplay_shortcuts_active(){return shortcuts;}
namespace rr64::mk64_items {
bool input_active() noexcept{return imported;}
void request_use(unsigned controller) noexcept{if(controller<4)item_requests.fetch_or(1u<<controller);}
unsigned take_action(unsigned controller) noexcept {
    if(controller>=4)return 0;
    const auto mask=1u<<controller;
    return (item_requests.fetch_and(~mask)&mask)?authority::action_mk64_item:0;
}
void stage_use(unsigned slot,int direction) noexcept{uses.emplace_back(slot,direction);}
}
namespace rr64::netplay {
Status get_status(){return status;}
void authority_pin_frame(){}
void authority_fail(const char*){++failures;}
bool authority_race_gate(bool){return gate;}
bool authority_queue_input_recorded(std::uint16_t buttons,std::int8_t x,std::int8_t y,
                                    authority::Command &accepted,std::uint8_t actions,unsigned duration) {
    ++queued;accepted={status.game_setup.revision,queued,buttons,x,y,actions,duration};
    accepted_command=accepted;
    if(!status.is_host)return true;
    authority::InputBatch batch{};batch.round=accepted.round;batch.count=1;batch.commands[0]=accepted;
    return host_round.receive(status.local_slot,batch);
}
bool authority_begin_step(authority::Step &step,unsigned duration){return host_round.begin(step,duration);}
bool authority_finish_step(std::uint64_t tick,authority::Stamp &stamp) {
    if(!host_round.finish(tick))return false;stamp=host_round.stamp();return true;
}
}
namespace rr64::authority {
bool native_controls(unsigned char*,recomp_context&,const Command&,std::uint16_t,std::uint32_t,std::uint16_t){return true;}
}
extern "C" int rr64_online_terrain_prepare(unsigned char*,void*){return 1;}
extern "C" int rr64_online_authority_capture(unsigned char*,const void*){return 1;}
extern "C" void rr64_prediction_capture_reset(){}
extern "C" int rr64_prediction_capture_before(unsigned char*,const void*,const void*){++capture_calls;return 1;}
extern "C" int rr64_prediction_capture_after(unsigned char*){return 1;}
extern "C" void rr64_prediction_reconcile_step(unsigned char*){}
extern "C" void rr64_prediction_verify_before(unsigned char*,void*){}
extern "C" void rr64_prediction_verify_after(unsigned char*){}
extern "C" void rr64_authority_reset_eject(){}
extern "C" int rr64_authority_eject_step(unsigned char*,void*,unsigned,unsigned){++eject_calls;return eject_valid;}
#include "rr64_mk64_item_input_fixture.inc"
#include "../src/rr64_authoritative_step.cpp"

void release_inputs(){input_held={};for(int p=0;p<4;++p)sample_item_input(p,true,false);item_requests=0;rider_eject_requests=0;}
int main() {
    // Physical profiles retain their own edge, and one source never drains a
    // different controller's simultaneous item/eject request.
    for(unsigned p=0;p<4;++p) {
        release_inputs();input_held[p]=true;sample_item_input(p,true,false);
        check(rr64_authority_take_actions((p+1)%4)==0);
        check(rr64_authority_take_actions(p)==rr64::authority::action_mk64_item);
        sample_item_input(p,true,false);check(rr64_authority_take_actions(p)==0);
        input_held[p]=false;sample_item_input(p,true,false);input_held[p]=true;
        rider_eject_requests=1u<<p;sample_item_input(p,true,false);
        check(rr64_authority_take_actions(p)==rr64::authority::allowed_actions);
        check(rr64_authority_take_actions(p)==0);
    }
    for(unsigned blocker=0;blocker<5;++blocker) {
        release_inputs();input_held[0]=true;
        disabled=blocker==0;focused=blocker!=1;shortcuts=blocker!=2;imported=blocker!=3;
        sample_item_input(0,true,blocker==4);check(rr64_authority_take_actions(0)==0);
        disabled=false;focused=shortcuts=imported=true;
        sample_item_input(0,true,false);check(rr64_authority_take_actions(0)==0);
        input_held[0]=false;sample_item_input(0,true,false);input_held[0]=true;
        sample_item_input(0,true,false);check(rr64_authority_take_actions(0)==2);
    }
    release_inputs();input_held[0]=true;sample_item_input(-1,true,false);sample_item_input(4,true,false);
    check(!item_requests && rr64_authority_take_actions(4)==0);
    // Online guest slot13 is represented by native physical port0; nonreplicated
    // older local slots still use their established native controller index.
    for(bool replicated:{false,true}) {
        release_inputs();status.active=status.connected=true;status.phase=rr64::netplay::Phase::Race;
        status.local_slot=replicated?13:3;status.replicated_riders=replicated;
        input_held[0]=true;sample_item_input(0,true,false);
        const unsigned expected=replicated?0:3;
        check(rr64_authority_take_actions(expected)==2);
    }
    std::vector<unsigned char> memory(rr64::engine::kRdramSize);auto *m=memory.data();recomp_context ctx{};
    using namespace rr64;
    engine::write_u32(m,0x800a656c,14);
    for(unsigned slot=0;slot<14;++slot)engine::write_u16(m,0x800d8570u+slot*0x118u+0x24,1);
    for(auto address:prediction::timing_addresses)engine::write_u32(m,address,std::bit_cast<unsigned>(1.f/60.f));
    engine::write_u16(m,engine::globals::controller_buttons,0x2000);
    engine::write_s8(m,engine::globals::controller_stick_y,-80);
    status={};status.active=status.connected=status.authoritative=status.is_host=true;
    status.phase=netplay::Phase::Race;status.local_slot=0;status.game_setup.revision=7;
    status.authority_humans=(1u<<14)-1;check(host_round.reset(7,status.authority_humans));queued=0;
    for(unsigned slot=1;slot<14;++slot) {
        authority::InputBatch batch{};batch.round=7;batch.count=1;
        batch.commands[0]={7,1,0x2000,0,std::int8_t(int(slot)-7),authority::action_mk64_item,40000};
        check(host_round.receive(slot,batch));
    }
    item_requests=1;uses.clear();check(rr64_authority_step_begin(m,&ctx)==1);
    check(uses.size()==14 && accepted_command.actions==2 && !failures);
    for(unsigned slot=0;slot<14;++slot)check(uses[slot]==std::pair<unsigned,int>{slot,slot?int(slot)-7:-80});
    for(unsigned slot=0;slot<14;++slot){ctx.r4=engine::guest_address(0x800d8570+slot*0x118);check(rr64_authority_translate(m,&ctx)==1);}
    rr64_authority_step_finish(m);
    uses.clear();check(rr64_authority_step_begin(m,&ctx)==1);check(uses.empty());
    for(unsigned slot=0;slot<14;++slot){ctx.r4=engine::guest_address(0x800d8570+slot*0x118);check(rr64_authority_translate(m,&ctx)==1);}
    rr64_authority_step_finish(m);check(!failures);
    // A paused/loading native update cannot consume the pending input edge.
    item_requests=1;engine::write_u16(m,0x800a2192,1);check(rr64_authority_step_begin(m,&ctx)==1);
    check(item_requests==1 && uses.empty());engine::write_u16(m,0x800a2192,0);
    gate=false;check(rr64_authority_step_begin(m,&ctx)==0);check(item_requests==1 && uses.empty());gate=true;
    // Private correction passes short-circuit before input drain or host use.
    const unsigned before_queue=queued,before_eject=eject_calls;
    {prediction::ReplayScope replay;check(replay.valid());check(rr64_authority_step_begin(m,&ctx)==1);}
    check(item_requests==1 && queued==before_queue && eject_calls==before_eject && uses.empty());
    // Guest live prediction records/transmits the edge but cannot execute it.
    status.is_host=false;status.replicated_riders=true;status.local_slot=13;
    check(rr64_authority_step_begin(m,&ctx)==1);check(accepted_command.actions==2);
    check(item_requests==0 && uses.empty() && capture_calls==1);rr64_authority_step_finish(m);
    // An invalid native host state rejects this entire update before staging use.
    status.is_host=true;status.local_slot=0;status.game_setup.revision=8;
    check(host_round.reset(8,status.authority_humans));queued=0;item_requests=1;eject_valid=false;
    check(rr64_authority_step_begin(m,&ctx)==0 && uses.empty() && failures==1);
    std::printf("MK64 item input: %u checks; exact main/shim callback and production host step; 14 slots, guest/private isolation\n",checks);
}
