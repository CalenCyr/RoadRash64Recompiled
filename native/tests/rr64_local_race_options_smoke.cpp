#include "rr64_local_race_options.hpp"
#include "rr64_custom_cop_rules.hpp"
#include "rr64_custom_cop.hpp"
#include <cstdio>
#include <fstream>
#include <thread>
#include <chrono>
#include <cstdlib>
#include "recomp.h"
#include <vector>
#include "rr64_engine_layout.hpp"
#include "rr64_local_players.hpp"
#include "rr64_netplay.hpp"
#include "rr64_prediction_replay.hpp"
namespace { bool online = false; }
namespace rr64::netplay { Status get_status() { Status result{}; result.active = online; return result; } }
namespace rr64::netplay { bool authority_get_outcome(unsigned,authority::Outcome&,bool&){return false;} }
namespace recomp { void* alloc(unsigned char* memory, size_t size) { static size_t next = 0x700000; const size_t offset = next; next += (size + 15) & ~size_t(15); return memory + offset; } }
namespace { bool placement_ok=false; unsigned voices=0; }
extern "C" void func_8006B740(unsigned char* rdram,recomp_context* ctx) {
 const auto state=MEM_W(0xE8,ctx->r4);
 MEM_W(0,state)=123; // failed stock placement may already modify routing
 MEM_W(0,rr64::engine::guest_address(0x800D77A4))=ctx->r5;
 ctx->r2=placement_ok;
}
extern "C" void func_80056084(unsigned char*,recomp_context*) { ++voices; }
int main() {
    using namespace rr64::local_race_options;
    bool ok = true;
    // Equipment alone never unlocks a role outside Custom Cop Mode.
    for (unsigned player = 0; player < 4; ++player) {
        for (unsigned cop_rider : {40u, 41u}) {
            ok &= rr64::custom_cop::selected_cop(true, 31, cop_rider);
            ok &= !rr64::custom_cop::selected_cop(true, 0, cop_rider);
            ok &= !rr64::custom_cop::selected_cop(false, 31, cop_rider);
        }
        ok &= !rr64::custom_cop::selected_cop(true, 31, player);
    }
    for (unsigned humans = 1; humans <= 4; ++humans) {
        ok &= roster(0, humans, 9) == humans;
        ok &= roster(99, humans, 4) == 11;
    }
    ok &= roster(99, 0, 6) == 6 && roster(99, 5, 6) == 6;
    // A mode hiding all optional stock rows must still reach the new row,
    // wrap in both directions and skip hidden rows without an unbounded loop.
    constexpr unsigned mask = 3 | (1 << 4) | (1 << 7) | (1 << 8);
    ok &= next_row(7, 1, mask) == 8 && next_row(8, 1, mask) == 0;
    ok &= next_row(0, -1, mask) == 8 && next_row(4, -1, mask) == 1;
    ok &= next_row(4, 1, 0) == 0;
    using namespace rr64::engine;
    namespace layout = rr64::engine::local_race;
    std::vector<unsigned char> memory(kRdramSize);
    auto* rdram = memory.data();
    const auto put = [&](unsigned a, unsigned v) { write_u32(rdram, a, v); };
    const auto get = [&](unsigned a) { unsigned v = 0; read_u32(rdram, a, v); return v; };
    put(layout::humans, 4); put(layout::racers, 4); put(layout::ai_choice, 1);
    rr64_local_options_race(rdram); // Default local=false: do not touch another mode.
    ok &= get(layout::racers) == 4;
    rr64::local_players::active.store(true);
    // Every count for every player count, including wrap and a player-count
    // change between setup and race initialization. Stock AI flag stays binary.
    for (unsigned humans = 1; humans <= 4; ++humans) {
        put(layout::humans, humans); put(layout::menu_humans, humans);
        put(layout::menu_cursor, 4); put(layout::menu_buttons, 0);
        rr64_local_options_input(rdram);
        // Previous loop wraps to zero.
        rr64_local_options_race(rdram);
        ok &= get(layout::racers) == humans;
        for (unsigned ai = 1; ai <= max_ai(humans); ++ai) {
            put(layout::menu_buttons, 0x40);
            ok &= rr64_local_options_input(rdram) == 1;
            ok &= get(layout::ai_choice) == 1;
            rr64_local_options_race(rdram);
            ok &= get(layout::racers) == humans + ai;
        }
        rr64_local_options_input(rdram); // Maximum wraps to no AI.
        rr64_local_options_race(rdram);
        ok &= get(layout::racers) == humans && get(layout::ai_choice) == 0;
    }
    put(layout::menu_buttons, 0x20); rr64_local_options_input(rdram);
    rr64_local_options_race(rdram); ok &= get(layout::racers) == 11;
    put(layout::menu_buttons, 0x40); rr64_local_options_input(rdram);
    put(layout::menu_cursor, 8); put(0x800A6418, 4);
    for (unsigned level = 1; level <= 7; ++level) {
        rr64_local_options_input(rdram);
        rr64_local_options_race(rdram);
        ok &= rr64_local_bike_level(4) == level - 1 && get(0x800A6418) == 4;
    }
    rr64_local_options_input(rdram); rr64_local_options_race(rdram);
    ok &= rr64_local_bike_level(3) == 3; // Match Track after Level 7.
    put(layout::menu_buttons, 0x20); rr64_local_options_input(rdram);
    rr64_local_options_race(rdram); ok &= rr64_local_bike_level(0) == 6;
    ok &= rr64_local_options_visible(8, 0) == 1;
    const unsigned table = rr64_local_options_table(layout::menu_table);
    ok &= table != layout::menu_table && get(table + 11 * 36) == 0;
    put(0x80300018, 0xBADF00D);
    rr64_local_options_text(rdram, 8, 0x80300000);
    ok &= MEM_B(0, guest_address(0x80300000)) == 'B';
    ok &= get(0x80300018) == 0xBADF00D;
    put(layout::menu_buttons, 0x10);
    ok &= rr64_local_options_navigation(rdram) == 9;
    rr64_local_options_finish(rdram);
    // Scooter profile substitution keeps the human rider and original profile
    // intact, and is disabled when Match Track or online is active.
    put(layout::menu_cursor, 8); put(layout::menu_buttons, 0x20);
    rr64_local_options_input(rdram); rr64_local_options_race(rdram); // Insanity -> Scooter
    const unsigned original = 0x800A3460, donor = original + 4 * 16;
    for (unsigned i = 0; i < 16; ++i) MEM_B(i, guest_address(original)) = i;
    MEM_B(8, guest_address(donor)) = 6;
    MEM_B(9, guest_address(donor)) = 1;
    MEM_B(10, guest_address(donor)) = 23;
    MEM_B(13, guest_address(donor)) = 38;
    MEM_B(8, guest_address(donor + 16)) = 12;
    const unsigned profile = rr64_local_bike_profile(rdram, 0x800D8570, original);
    ok &= profile != original && MEM_B(10, guest_address(profile)) == 23;
    ok &= MEM_B(13, guest_address(profile)) == 38;
    ok &= MEM_B(11, guest_address(profile)) == 11 && MEM_B(14, guest_address(profile)) == 14;
    ok &= MEM_B(10, guest_address(original)) == 10 && get(0x800D8588) == 23;
    ok &= rr64_local_bike_profile(rdram, 0x800D8571, original) == original;
    // Exercise the actual guest-memory bridge in every local slot. Snapshot
    // unrelated controller/profile fields and unlocks across role assignment.
    put(layout::menu_cursor, 9); put(layout::menu_buttons, 0x40);
    rr64_local_options_input(rdram);
    ok &= rr64_custom_cop_enabled() && rr64_custom_cop_bike_count(4) == 5;
    ok &= rr64_local_options_visible(10, 0) == 1;
    // New setting defaults off, converts only unreserved AI police slots, and
    // leaves the original pool intact when explicitly enabled.
    put(layout::humans,2); rr64_custom_cop_begin(rdram,1);
    recomp_context ai_context{}; ai_context.r29=guest_address(0x80600000);
    const unsigned ai_pool=0x80600070;
    put(ai_pool+5*4,3);put(ai_pool+7*4,2);ai_context.r21=2;
    rr64_custom_cop_ai_pool(rdram,&ai_context);
    ok &= get(ai_pool+5*4)==5 && get(ai_pool+7*4)==0 && ai_context.r21==0;
    put(layout::menu_cursor,10);put(layout::menu_buttons,0x40);
    ok &= rr64_local_options_input(rdram)==1;
    put(ai_pool+5*4,3);put(ai_pool+7*4,2);ai_context.r21=2;
    rr64_custom_cop_ai_pool(rdram,&ai_context);
    ok &= get(ai_pool+5*4)==3 && get(ai_pool+7*4)==2 && ai_context.r21==2;
    rr64_local_options_input(rdram); // back to default Off
    ok &= valid_online_options(512u|1024u|10u) && !valid_online_options(2048u);
    reset_online();ok &= !(online_options()&1024u);
    apply_online_options(512u|1024u);ok &= online_options()==1536u;
    put(0x800A6690, 0); put(0x800A66B8, 4);
    for (unsigned i = 0; i < 4; ++i) {
        put(0x8009EF3C + i * 4, 4);
        ok &= rr64_custom_cop_bike_entry(rdram, i, 123) == 0x800A684C;
    }
    put(0x800A77D8, 0);
    for (unsigned humans = 1; humans <= 4; ++humans) {
        put(layout::humans, humans); put(0x800A656C, humans + 1);
        for (unsigned mask = 0; mask < (1u << humans); ++mask) {
            rr64_local_options_race(rdram);
            for (unsigned i = 0; i < humans; ++i) {
                const unsigned a = 0x800D8570 + i * 0x118;
                put(a + 0x18, 31); put(a + 0x1C, mask & (1u << i) ? 40 : 0);
                put(a + 4, i); put(a + 8, i); put(a + 0x108, 0x12340000 + i);
            }
            const unsigned ai = 0x800D8570 + humans * 0x118;
            put(ai + 0x20, 1);
            rr64_custom_cop_roles(rdram);
            for (unsigned i = 0; i < humans; ++i) {
                const unsigned a = 0x800D8570 + i * 0x118;
                ok &= (get(a + 0x20) == 7) == bool(mask & (1u << i));
                ok &= get(a + 4) == i && get(a + 8) == i && get(a + 0x108) == 0x12340000 + i;
                ok &= bool(rr64_custom_cop_arrest(rdram, a, ai)) == bool(mask & (1u << i));
                ok &= !rr64_custom_cop_arrest(rdram, a, a);
            }
            put(0x800D764C, 1); ok &= !rr64_custom_cop_finished(rdram);
            put(0x800D764C, 0); ok &= bool(rr64_custom_cop_finished(rdram)) == (mask != 0);
            ok &= get(0x800A77D8) == 0;
        }
    }
    // Only active human slots count toward the required cop. A cop rider
    // and bike on different players must not satisfy the paired selection.
    for (unsigned humans = 1; humans <= 4; ++humans) {
        put(layout::menu_humans, humans);
        for (unsigned i = 0; i < 4; ++i) {
            put(0x8009F660 + i * 4, i < humans ? 0 : 31);
            put(0x8009F400 + i * 4, 40);
            put(0x8009EF2C + i * 4, 2);
        }
        ok &= !rr64_custom_cop_can_start(rdram) && !rr64_custom_cop_confirm(rdram);
        for (unsigned i = 0; i < humans; ++i) ok &= get(0x8009EF2C + i * 4) == 1;
        for (unsigned cop = 0; cop < humans; ++cop) {
            put(0x8009F660 + cop * 4, 31);
            ok &= rr64_custom_cop_can_start(rdram) && rr64_custom_cop_confirm(rdram);
            put(0x8009F660 + cop * 4, 0);
        }
    }
    // All cops with no AI is valid free play, even with no racers remaining.
    put(0x800A656C, 4); rr64_custom_cop_roles(rdram);
    ok &= !rr64_custom_cop_finished(rdram);
    // Every police profile remains selectable on each local controller.
    put(layout::menu_humans, 4); put(0x800A6690, 0); put(0x800A66B8, 4);
    for (unsigned i = 0; i < 4; ++i) {
        put(0x8009EF3C + i * 4, 4);
        put(0x8009E238 + i * 4, 8);
        ok &= rr64_custom_cop_rider(rdram, i, 0) == 40;
        for (unsigned variant = 40; variant <= 44; ++variant)
            ok &= rr64_custom_cop_rider(rdram, i, variant) == variant;
        put(0x8009E238 + i * 4, 0x10);
        ok &= rr64_custom_cop_rider(rdram, i, 41) == 41;
        ok &= rr64_custom_cop_rider(rdram, i, 39) == 44;
    }
    const unsigned cop = 0x800D8570, target = cop + 0x118;
    put(cop + 0x20, 7); put(target + 0x20, 5);
    put(cop + 0xE0, 0x80200000); put(cop + 0xE4, 0x80201000);
    MEM_H(0x83C,guest_address(0x80200000))=1;
    rr64_custom_cop_equipment(rdram, cop);
    ok &= MEM_HU(0x83C,guest_address(0x80200000))==0;
    ok &= MEM_HU(0x842, guest_address(0x80200000)) == 1;
    ok &= get(0x802015B0) == 5;
    put(0x80300000, 0); put(0x80300004, 0);
    rr64_custom_cop_spawn(rdram, 0, 0x80300000);
    ok &= get(0x80300000) == 0x41600000 && get(0x80300004) == 0x40F00000;
    rr64_custom_cop_spawn(rdram, 1, 0x80300000);
    ok &= get(0x80300000) == 0x41600000; // Racer's offsets untouched.
    put(target + 0xE8, 0x80202000);
    MEM_H(0x4C, guest_address(0x80202000)) = 1;
    MEM_H(0x88, guest_address(cop)) = 1;
    MEM_H(0x88, guest_address(target)) = 1;
    MEM_B(0xC, guest_address(target)) = 'T'; MEM_B(0xD, guest_address(target)) = 0;
    rr64_custom_cop_bust_message(rdram, cop, target);
    ok &= MEM_B(0x92, guest_address(cop)) == 'B';
    MEM_B(0x92, guest_address(cop)) = 'K';
    rr64_custom_cop_notification(rdram, cop + 0x2C, 3, target + 0xC);
    ok &= MEM_B(0x92, guest_address(cop)) == 'B';
    // Failed roadside attempts must restore route state and ranking; successful
    // posting triggers only after a non-cop passes and throttles voice playback.
    put(cop+0xE8,0x80203000);put(target+0xE8,0x80203100);
    put(0x80203000,77);put(0x80203040,0);put(0x800D77A4,0);
    put(0x800D762C,0x447A0000); // length 1000
    recomp_context context{};context.r29=guest_address(0x80700000);
    rr64_custom_cop_post(rdram,&context,0);
    ok &= get(0x80203000)==77 && get(0x800D77A4)==0;
    placement_ok=true;rr64_custom_cop_post(rdram,&context,0);
    ok &= rr64_custom_cop_pursuit(rdram,0x80200000)==0;
    context.r4=guest_address(cop);context.r6=0x2000;context.r5=0x2000;
    context.r7=0x3F800000;MEM_W(16,context.r29)=0x3F800000;
    rr64_custom_cop_control(rdram,&context);
    ok &= context.r6==0x10 && context.r7==0 && MEM_W(16,context.r29)==0;
    ok &= MEM_HU(0x81C,guest_address(0x80200000))==1;
    MEM_H(0x48,guest_address(0x80203100))=1;
    put(0x80203120,0x43960000); // racer at 300, cop posted at 200
    ok &= rr64_custom_cop_pursuit(rdram,0x80200000)==1;
    ok &= rr64_custom_cop_siren(rdram,0x80200000)==0;
    put(0x800A1820,0);context.r6=0x20;context.r5=0x20;
    rr64_custom_cop_control(rdram,&context);
    ok &= !(context.r6&0x20) && rr64_custom_cop_siren(rdram,0x80200000)==0;
    put(0x800A1820,0x3E800000);context.r6=0;context.r5=0; // quarter-second tap
    rr64_custom_cop_control(rdram,&context);
    ok &= (context.r6&context.r5&0x20)!=0;
    context.r6=0x20;context.r5=0x20;rr64_custom_cop_control(rdram,&context);
    put(0x800A1820,0x3FA00000);context.r6=0x20;context.r5=0;
    rr64_custom_cop_control(rdram,&context);
    ok &= rr64_custom_cop_siren(rdram,0x80200000)==1 && !(context.r6&0x20);
    put(0x800A1820,0x40000000);context.r6=0x20; // continuing to hold must not toggle twice
    rr64_custom_cop_control(rdram,&context);
    ok &= rr64_custom_cop_siren(rdram,0x80200000)==1;
    context.r6=0;rr64_custom_cop_control(rdram,&context);
    ok &= rr64_custom_cop_siren(rdram,0x80200000)==1 && !(context.r6&0x20);
    context.r6=0x20;context.r5=0x20;rr64_custom_cop_control(rdram,&context);
    put(0x800A1820,0x40400000);context.r6=0x20;context.r5=0;
    rr64_custom_cop_control(rdram,&context);
    ok &= rr64_custom_cop_siren(rdram,0x80200000)==0;
    context.r6=0;rr64_custom_cop_control(rdram,&context);
    ok &= !(context.r6&0x20);
    context.r6=4;context.r5=4;rr64_custom_cop_control(rdram,&context);
    ok &= !rr64_custom_cop_trick(rdram,cop) && (context.r6&4) && (context.r5&4) && !(context.r6&0x20);
    context.r6=rr64_cop_weapon_trick_button;context.r5=rr64_cop_weapon_trick_button;rr64_custom_cop_control(rdram,&context);
    ok &= !(context.r6&rr64_cop_weapon_trick_button) && !(context.r5&rr64_cop_weapon_trick_button);
    ok &= rr64_custom_cop_trick(rdram,cop) && (context.r6&0x20) && !(context.r6&4);
    context.r6=0;context.r5=0;rr64_custom_cop_control(rdram,&context);
    ok &= !rr64_custom_cop_trick(rdram,cop);
    // Spoke jam must survive the shared cop input adapter, including edges.
    context.r6=5;context.r5=5;rr64_custom_cop_control(rdram,&context);
    ok &= (context.r6&5)==5 && (context.r5&5)==5 && !rr64_custom_cop_trick(rdram,cop);
    put(0x800D7670,0x3F800000);
    // The original eligible-racer flag stays zero for cops, without blocking recovery.
    put(0x8020304C,0);MEM_H(0x48,guest_address(0x80203000))=0;
    put(0x802004F8,0x42C80000);
    put(0x800048EC,0x40A00000); // Fixture stock delay: five simulation seconds.
    for (unsigned age : {0u,0x3F800000u,0x409FFFFFu,0x40A00000u}) {
        put(0x802004CC,age);
        ok &= !rr64_custom_cop_can_recover(rdram,cop);
    }
    put(0x802004CC,0x40A00001); // Stock comparison is strictly greater-than.
    ok &= rr64_custom_cop_can_recover(rdram,cop) && get(0x80203048)==0;
    put(0x800048EC,0x40C00000); // Follow game data, not a hard-coded five.
    ok &= !rr64_custom_cop_can_recover(rdram,cop);
    put(0x800048EC,0x40A00000);
    put(0x802004F8,0xBF800000);ok &= !rr64_custom_cop_can_recover(rdram,cop);
    put(0x802004F8,0x42C80000);put(0x8020304C,1);
    ok &= !rr64_custom_cop_can_recover(rdram,cop);
    put(0x8020304C,0);
    MEM_H(0x57C,guest_address(0x80201000))=1;MEM_H(0x7F6,guest_address(0x80200000))=0;
    ok &= rr64_custom_cop_roaming(rdram,cop) && !rr64_custom_cop_roaming(rdram,target);
    MEM_H(0x7F6,guest_address(0x80200000))=1;
    ok &= !rr64_custom_cop_roaming(rdram,cop);
    MEM_H(0x7F6,guest_address(0x80200000))=0;MEM_H(0x57C,guest_address(0x80201000))=0;
    ok &= !rr64_custom_cop_roaming(rdram,cop);
    // Victory waits for all opponents, then allows the stock results after 3s.
    put(0x800A656C,2);put(0x800A6578,1);rr64_custom_cop_roles(rdram);
    put(0x800D764C,0);MEM_H(0x4C,guest_address(0x80203100))=0;
    ok &= rr64_custom_cop_win_age(rdram)<0 && rr64_custom_cop_finished(rdram);
    MEM_H(0x4C,guest_address(0x80203100))=1;
    ok &= rr64_custom_cop_win_age(rdram)==0 && !rr64_custom_cop_finished(rdram);
    put(0x800D7670,0x40800000); // 4 seconds, announcement began at 1
    ok &= rr64_custom_cop_finished(rdram);
    // AI police are allies, not an impossible extra arrest requirement.
    const unsigned ai_cop=0x800D8570+2*0x118;
    put(ai_cop+0x20,7);put(0x800A656C,3);
    rr64_custom_cop_begin(rdram,1);rr64_custom_cop_roles(rdram);
    MEM_H(0x4C,guest_address(0x80203100))=0;
    put(0x800D764C,1);
    ok &= rr64_custom_cop_win_age(rdram)<0 && !rr64_custom_cop_finished(rdram);
    MEM_H(0x4C,guest_address(0x80203100))=1;
    ok &= rr64_custom_cop_win_age(rdram)==0 && !rr64_custom_cop_finished(rdram);
    put(0x800D7670,0x40E00000); // three seconds after the announcement at four
    ok &= rr64_custom_cop_finished(rdram);
    rr64_custom_cop_reset(); ok &= !rr64_custom_cop_active();
    ok &= rr64_custom_cop_pursuit(rdram,0x80200000)==-1;
    // Each populated local slot can roam, including non-cops. Exercise the
    // negative recovery paths independently of the selection/menu settings.
    for (unsigned humans = 1; humans <= 4; ++humans) {
        put(layout::humans, humans);
        for (unsigned slot = 0; slot < 4; ++slot) {
            const unsigned actor = 0x800D8570 + slot * 0x118;
            const unsigned bike = 0x80300000 + slot * 0x1000;
            const unsigned rider = 0x80310000 + slot * 0x1000;
            const unsigned state = 0x80320000 + slot * 0x100;
            put(actor + 0xE0, bike); put(actor + 0xE4, rider); put(actor + 0xE8, state);
            put(bike + 0x4F8, 0x42C80000); put(bike + 0x858, 0x41200000);
            MEM_H(0x57C, guest_address(rider)) = 1;
            MEM_H(0x7F6, guest_address(bike)) = 0;
            MEM_H(0x4C, guest_address(state)) = 0;
            ok &= bool(rr64_local_player_roaming(rdram, actor)) == (slot < humans);
            ok &= get(bike + 0x858) == (slot < humans ? 0u : 0x41200000u);
            MEM_H(0x7F6, guest_address(bike)) = 1;
            ok &= !rr64_local_player_roaming(rdram, actor);
            MEM_H(0x7F6, guest_address(bike)) = 0;
            MEM_H(0x57C, guest_address(rider)) = 0;
            ok &= !rr64_local_player_roaming(rdram, actor);
            MEM_H(0x57C, guest_address(rider)) = 1;
            MEM_H(0x4C, guest_address(state)) = 1;
            ok &= !rr64_local_player_roaming(rdram, actor);
            MEM_H(0x4C, guest_address(state)) = 0;
            for (unsigned health : {0xBF800000u, 0x7FC00000u, 0x7F800000u}) {
                put(bike + 0x4F8, health);
                ok &= !rr64_local_player_roaming(rdram, actor);
            }
            put(bike + 0x4F8, 0x42C80000);
            ok &= !rr64_local_player_roaming(rdram, actor + 1);
        }
        ok &= !rr64_local_player_roaming(rdram, 0x800D8570 + humans * 0x118);
    }
    rr64::local_players::active.store(false);
    ok &= !rr64_local_player_roaming(rdram, 0x800D8570);
    rr64::local_players::active.store(true);
    for (unsigned humans : {0u, 5u, 0xFFFFFFFFu}) {
        put(layout::humans, humans);
        ok &= !rr64_local_player_roaming(rdram, 0x800D8570);
    }
    put(layout::humans, 4);
    online = true;
    ok &= !rr64_local_player_roaming(rdram, 0x800D8570);
    ok &= rr64_local_bike_profile(rdram, 0x800D8570, original) == original;
    put(layout::racers, 6); rr64_local_options_race(rdram);
    ok &= get(layout::racers) == 6 && rr64_local_bike_level(4) == 4;
    rr64_local_options_input(rdram);
    ok &= rr64_local_options_navigation(rdram) == -1;
    ok &= rr64_local_options_table(layout::menu_table) == layout::menu_table;
    // Transport is disconnected above, but historical authority rules must
    // still govern replay. One camera does not limit the human actor pool.
    put(layout::humans, 1);
    for (bool host : {false, true}) for (unsigned slot = 0; slot < 14; ++slot) {
        rr64::prediction::SessionRules rules{true,true,true,host,true,0,1u << slot};
        rr64::prediction::ReplayScope replay(rules);
        const unsigned actor = 0x800D8570 + slot * 0x118;
        const unsigned bike = 0x80400000, rider = 0x80401000, state = 0x80402000;
        put(actor + 0xE0, bike); put(actor + 0xE4, rider); put(actor + 0xE8, state);
        put(bike + 0x4F8, 0x42C80000); put(bike + 0x858, 0x41200000);
        MEM_H(0x57C, guest_address(rider)) = 1;
        MEM_H(0x7F6, guest_address(bike)) = 0;
        MEM_H(0x4C, guest_address(state)) = 0;
        ok &= rr64_local_player_roaming(rdram, actor) == 1 && get(bike + 0x858) == 0;
        ok &= !rr64_local_player_roaming(rdram, 0x800D8570 + ((slot + 1) % 14) * 0x118);
        MEM_H(0x7F6, guest_address(bike)) = 1;
        ok &= !rr64_local_player_roaming(rdram, actor);
        MEM_H(0x7F6, guest_address(bike)) = 0;
        MEM_H(0x57C, guest_address(rider)) = 0;
        ok &= !rr64_local_player_roaming(rdram, actor);
        MEM_H(0x57C, guest_address(rider)) = 1;
        MEM_H(0x4C, guest_address(state)) = 1;
        ok &= !rr64_local_player_roaming(rdram, actor);
    }
    std::puts(ok ? "PASS local roster/menu and online replay roaming" : "FAIL local race options");
    return ok ? 0 : 1;
}
