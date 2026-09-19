#include "rr64_netplay.hpp"
#include "rr64_engine_layout.hpp"
#include "rr64_online_ready.hpp"
#include <vector>
#include <cstdlib>

namespace {
rr64::netplay::Status state;
void require(bool value) { if (!value) std::abort(); }
}
namespace rr64::netplay {
Status get_status() { return state; }
bool set_selection(const online_flow::Selection &s) { state.players[state.local_slot].selection=s; return true; }
bool host_release_selection() { return false; }
}
#include "../src/rr64_online_guest_flow.cpp"

int main() {
    for(unsigned slot=0;slot<14;++slot) {
        require(rr64::online_flow::shortcut_slot(0,true,slot,slot>=4)==(slot>=4 ? 0u:slot));
        require(rr64::online_flow::shortcut_slot(3,false,slot,false)==3);
    }

    // Simulate an edge-triggered prompt becoming ready at every phase of a
    // held host command. A delayed peer must see a fresh rising edge soon.
    for(unsigned ready=0;ready<600;++ready) {
        auto previous=rr64::online_flow::start_prompt_buttons(true,ready);
        bool advanced=false;
        for(unsigned now=ready+1;now<=ready+301;++now) {
            auto current=rr64::online_flow::start_prompt_buttons(true,now);
            if(current && !previous) advanced=true;
            previous=current;
            require(rr64::online_flow::start_prompt_buttons(false,now)==0);
        }
        require(advanced);
    }

    std::vector<unsigned char> memory(16*1024*1024);
    auto *m=memory.data();
    using rr64::engine::write_u32;
    auto word=[&](unsigned a) {unsigned v=0;rr64::engine::read_u32(m,a,v);return v;};
    write_u32(m,0x8009EF5C,4);
    write_u32(m,rr64::engine::globals::multiplayer_stage,2);
    rr64_online_private_selection_begin(m);
    require(word(0x8009EF5C)==4); // offline must be untouched
    state.active=state.connected=true;state.connected_players=2;state.local_slot=1;
    state.phase=rr64::netplay::Phase::CharacterSelect;
    state.game_setup.valid=true;state.game_setup.revision=9;
    write_u32(m,0x8009EF5C,2);
    rr64_online_private_selection_begin(m);
    require(word(0x8009EF5C)==1);
    write_u32(m,0x8009F670,40);write_u32(m,0x8009F660,31);write_u32(m,0x8009EF2C,2);
    require(!rr64_online_selection_commit(m));
    require(state.players[1].selection.rider==40 && state.players[1].selection.bike==31);
    rr64_online_private_selection_end(m);
    require(word(0x8009EF5C)==2);
    state.players[0].selection={9,3,1,1,0};
    state.phase=rr64::netplay::Phase::TrackSelect;
    rr64_online_private_selection_begin(m);
    require(rr64_online_selection_commit(m));
    rr64_online_private_selection_end(m);
    require(word(0x8009F670)==3 && word(0x8009F674)==40);
    require(word(0x8009F660)==1 && word(0x8009F664)==31);
    require(word(0x8009F400)==3 && word(0x8009F404)==40);
    require(word(0x8009F5D8)==1 && word(0x8009F5DC)==31);
    state.phase=rr64::netplay::Phase::Race;
    write_u32(m,0x800A4F24,2);write_u32(m,0x800A6578,2);
    write_u32(m,0x800A657C,4);write_u32(m,0x800A6580,7);
    for(unsigned field : {0xE0u,0xE4u,0xE8u}) write_u32(m,0x800D8570+7*0x118+field,0x80300000+field);
    rr64_online_hud_begin(m);
    for(unsigned field : {0xE0u,0xE4u,0xE8u}) require(rr64_online_hud_actor_pointer(m,0x80100000,field)==0x80300000+field);
    require(word(0x800A4F24)==0 && word(0x800A6578)==1 && word(0x800A657C)==7);
    // Native lw sign-extends a pointer into the 64-bit guest register. Match
    // the generated hook expression and dereference using the real MEM macro.
    for(unsigned field : {0xE0u,0xE4u,0xE8u}) {
        unsigned char *rdram=m;
        write_u32(m,0x80300000+field+0x310,0x12345678);
        const gpr pointer=(int64_t)(int32_t)rr64_online_hud_actor_pointer(m,0x80100000,field);
        require(pointer==0xFFFFFFFF80300000ULL+field);
        require(MEM_W(0x310,pointer)==0x12345678);
    }
    rr64_online_hud_end(m);
    require(rr64_online_hud_actor_pointer(m,0x80100000,0xE4)==0x80100000);
    require(word(0x800A4F24)==2 && word(0x800A6578)==2 && word(0x800A657C)==4);
    using namespace rr64::online_ready;
    require(!visible(state)); // never draw during the race
    state.phase=rr64::netplay::Phase::CharacterSelect;
    require(visible(state));
    auto &player=state.players[1];
    player.connected=true; player.name="Officer";
    player.selection={state.game_setup.revision,40,31,1,0};
    state.game_setup.race_options=512;
    require(label(state,player,1)=="Officer READY COP");
    player.selection.round++;
    require(label(state,player,1)=="Officer WAIT");
    player.selection.round=state.game_setup.revision;
    player.selection.bike=0;
    require(label(state,player,1)=="Officer READY");
    player.connected=false;
    require(!confirmed(state,player));
    state.active=false;
    require(!visible(state));
    state.active=true; state.connected_players=14; state.replicated_riders=true;
    for (unsigned slot=0;slot<14;++slot) {
        state.players[slot].connected=true;
        state.players[slot].selection={state.game_setup.revision,slot+10,slot+1,1,0};
    }
    require(rr64_online_player_count_label(1)==14);
    for (unsigned local=0;local<14;++local) {
        state.local_slot=local;
        unsigned mask=0;
        for (unsigned guest=0;guest<14;++guest) {
            const auto slot=rr64::online_flow::mapped_slot(guest,local,true);
            mask |= 1u<<slot;
            require(rr64_online_race_choice(guest,99,0)==slot+10);
            require(rr64_online_race_choice(guest,99,1)==slot+1);
        }
        require(mask==0x3fff);
    }
    state.active=false;
    require(rr64_online_player_count_label(4)==4);
    require(rr64_online_race_choice(0,99,0)==99);
    return 0;
}
