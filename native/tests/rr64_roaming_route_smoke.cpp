#include "rr64_roaming_route.hpp"
#include "rr64_engine_layout.hpp"
#include "rr64_prediction_rules.hpp"
#include <cstdio>
#include <vector>
#include <cmath>
#include <limits>
namespace { rr64::netplay::Status session; }
namespace rr64::netplay { Status get_status(){return session;} }
extern "C" void func_8006736C(unsigned char*,recomp_context*);
extern "C" void func_800674C4(unsigned char*,recomp_context*);
extern "C" void func_80066D70(unsigned char*,recomp_context*);
extern "C" void func_80068D0C(unsigned char*,recomp_context*);
extern "C" void func_80017DBC(unsigned char*,recomp_context* c) { c->f0.fl=100.0f; }
int main() {
    using namespace rr64::engine;
    std::vector<unsigned char> memory(kRdramSize);
    auto* m=memory.data();
    auto* rdram=m; // Native memory-access macros use this identifier.
    constexpr unsigned actor=0x800D8570,bike=0x80100000,state=0x80101000,
                       path=0x80102000,sp=0x80700000;
    const auto put=[&](unsigned p,unsigned v){write_u32(m,p,v);};
    const auto num=[&](unsigned p,float v){write_float(m,p,v);};
    const auto get=[&](unsigned p){unsigned v=0;read_u32(m,p,v);return v;};
    const auto val=[&](unsigned p){float v=0;read_float(m,p,v);return v;};
    unsigned tests=0,failed=0;
    const auto check=[&](bool ok,const char* name){++tests;if(!ok){++failed;std::printf("FAIL %s\n",name);}};
    put(actor+8,0);put(actor+0xE8,state);put(actor+0xE0,bike);
    put(0x800A6544,path);put(0x800A6540,7);
    // Three consecutive straight curves. A rider is far behind the stored
    // race segment: recovery must choose the current physical location.
    for(unsigned i=0;i<7;++i) {
        MEM_BU(0,guest_address(path+i*16))=1;
        num(path+i*16+8,float(i*50)); num(path+i*16+12,0);
    }
    put(state,4);num(state+8,0.9f);num(state+0x20,200);
    recomp_context c{};c.r17=guest_address(actor);c.r21=guest_address(bike);c.r29=guest_address(sp);
    num(bike+0x16C,25);num(bike+0x170,40);
    check(rr64_roaming_recovery_point(m,&c)==1,"nearby recovery available");
    check(get(sp+0x58)==0 && std::abs(val(sp+0x5C)-.25f)<.0001f,"backtracked recovery at nearest road");
    check(get(state)==4 && val(state+0x20)==200,"selection does not mutate race progress");
    num(bike+0x16C,275);
    check(rr64_roaming_recovery_point(m,&c)==1 && get(sp+0x58)==4,"current position changes target without stale cache");
    num(bike+0x16C,25);
    check(rr64_roaming_recovery_point(m,&c)==1 && get(sp+0x58)==0,"returning near updates again");
    write_u16(m,path+0x12,2);
    check(rr64_roaming_recovery_point(m,&c)==1 && get(sp+0x58)==2,"blocked segment excluded");
    write_u16(m,path+0x12,0);
    num(bike+0x16C,std::numeric_limits<float>::quiet_NaN());
    check(!rr64_roaming_recovery_point(m,&c),"invalid coordinates rejected");
    num(bike+0x16C,25);
    write_u16(m,actor+0x26,1);
    check(!rr64_roaming_recovery_point(m,&c),"AI keeps original recovery");
    write_u16(m,actor+0x26,0);
    constexpr unsigned route=state+0x100;
    put(actor+0xEC,route);put(route,4);
    c.r18=guest_address(route);c.r16=0;c.f0.fl=-.5f;c.f22.fl=0;
    check(rr64_roaming_previous_segment(m,&c)==1 && get(route)==2,"physical route walks backwards");
    check(rr64_roaming_previous_segment(m,&c)==1 && get(route)==0,"physical route reaches beginning");
    check(!rr64_roaming_previous_segment(m,&c),"physical route cannot underflow");
    put(route,4);c.r16=5;
    check(!rr64_roaming_previous_segment(m,&c),"search budget bounded");
    c.r16=0;c.f0.fl=.5f;
    check(!rr64_roaming_previous_segment(m,&c),"forward travel untouched");
    c.r4=guest_address(path);c.r5=guest_address(state);c.r6=0;
    check(rr64_roaming_reverse_route(m,&c)==1,"human backwards uses bidirectional progress");
    // Run the unchanged native accumulator with three 100-unit road segments.
    // 290 -> 25 -> 275 -> 25 verifies backward subtraction and no sticky gap.
    const auto move=[&](unsigned segment,float t) {
        c.r4=guest_address(path);c.r5=guest_address(state);c.r6=segment;
        unsigned bits;std::memcpy(&bits,&t,4);c.r7=bits;
        func_8006736C(m,&c);
        return val(state+0x20)+val(state+8)*val(state+12);
    };
    num(state+12,100);
    check(std::abs(move(0,.25f)-25)<.001f,"native progress decreases after backtracking");
    check(std::abs(move(4,.75f)-275)<.001f,"native progress advances again");
    check(std::abs(move(0,.25f)-25)<.001f,"native progress returns without accumulating error");
    put(state,4);num(state+8,.9f);num(state+0x20,200);
    c.r4=guest_address(path);c.r5=guest_address(state);
    c.r6=4;check(!rr64_roaming_reverse_route(m,&c),"same segment retains native behavior");
    c.r6=0;put(0x800D763C,4);
    check(!rr64_roaming_reverse_route(m,&c),"circuit lap wrap untouched");
    put(0x800D763C,0);
    session.active=session.connected=session.authoritative=true;session.authority_humans=1;
    check(rr64_roaming_reverse_route(m,&c)==1,"authority human eligible");
    session.authority_humans=0;
    check(!rr64_roaming_reverse_route(m,&c),"authority NPC excluded");
    {
        rr64::prediction::ReplayScope replay({true,true,true,false,false,0,1});
        check(rr64_roaming_reverse_route(m,&c)==1,"replay captured human mask used");
    }
    session={};
    // The closest point on a curved road is not its straight endpoint chord.
    num(path+24,50);num(path+28,100);
    num(bike+0x16C,50);num(bike+0x170,80);
    check(rr64_roaming_recovery_point(m,&c)==1 && get(sp+0x58)==0 &&
          std::abs(val(sp+0x5C)-.5f)<.0001f,"nearest curved road point");
    // Circuit arrays contain approach/tail curves outside the progress cycle.
    // A spatially nearest curve is not necessarily a legal recovery target.
    put(0x800A6540,11);put(0x800D763C,6);
    for(unsigned i=0;i<11;++i) {
        MEM_BU(0,guest_address(path+i*16))=1;
        num(path+i*16+8,float(i*50));num(path+i*16+12,0);
    }
    num(bike+0x16C,25);num(bike+0x170,0);
    check(rr64_roaming_recovery_point(m,&c)==1 && get(sp+0x58)==2,
          "circuit approach curve excluded");
    num(bike+0x16C,475);
    check(rr64_roaming_recovery_point(m,&c)==1 && get(sp+0x58)==6,
          "circuit tail curve excluded");
    // Use the game's segment traversal as an independent reachability oracle,
    // then execute its full progress accumulator for every valid start/target.
    for(unsigned old_segment=2;old_segment<=6;old_segment+=2) {
        for(unsigned sample=0;sample<=20;++sample) {
            c.r17=guest_address(actor);c.r21=guest_address(bike);c.r29=guest_address(sp);
            num(bike+0x16C,float(sample*25));
            check(rr64_roaming_recovery_point(m,&c)==1,"circuit recovery selected");
            const unsigned target=get(sp+0x58);const float t=val(sp+0x5C);
            unsigned cursor=old_segment;
            for(unsigned step=0;step<3 && cursor!=target;++step) {
                c.r4=cursor;func_80066D70(m,&c);cursor=unsigned(c.r2);
            }
            const bool reachable=cursor==target;
            check(reachable,"selected circuit curve reachable by native traversal");
            if(!reachable) continue; // Do not hang a failing smoke test.
            put(state,old_segment);num(state+8,.25f);num(state+12,100);
            num(state+0x10,0);num(state+0x20,200);
            c.r4=guest_address(path);c.r5=guest_address(state);c.r6=target;
            std::memcpy(&c.r7,&t,4);
            func_800674C4(m,&c);
            check(get(state)==target && val(state+8)==t && std::isfinite(c.f0.fl),
                  "native circuit progress completes with selected curve");
            check(c.r29==guest_address(sp),"circuit accumulator restores stack");
        }
    }
    c.r17=guest_address(actor);c.r21=guest_address(bike);c.r29=guest_address(sp);
    for(unsigned invalid_wrap: {1u,3u,10u,0xFFFFFFFFu}) {
        put(0x800D763C,invalid_wrap);put(sp+0x58,0x12345678);
        check(!rr64_roaming_recovery_point(m,&c) && get(sp+0x58)==0x12345678,
              "invalid circuit metadata leaves native recovery inputs unchanged");
    }
    // The finish line is inside the last curve, not necessarily at the array
    // wrap. Native lap evaluation must preserve both distinctions after recovery.
    put(0x800D763C,6);num(0x800D7630,300);num(0x800D762C,350);
    put(0x800D8524,3);write_u16(m,0x800D7680,1);
    const auto set_progress=[&](unsigned segment,float t,float base,unsigned laps=0) {
        put(state,segment);put(state+4,laps);num(state+8,t);num(state+12,100);
        num(state+0x10,0);num(state+0x20,base);put(state+0x4C,0);
    };
    const auto recover_progress=[&](unsigned segment,float t) {
        c={};c.r16=guest_address(state);c.r17=guest_address(actor);c.r29=guest_address(sp);
        c.f_odd=&c.f0.u32h;c.f24.fl=23.5f;c.r5=0x12345678;
        put(sp+0x58,segment);num(sp+0x5C,t);
        return rr64_roaming_recovery_progress(m,&c);
    };
    const auto evaluate_lap=[&]() {
        c.r4=guest_address(state);func_80068D0C(m,&c);return c.f0.fl;
    };
    const auto drive=[&](unsigned segment,float t) {
        c.r4=guest_address(path);c.r5=guest_address(state);c.r6=segment;
        std::memcpy(&c.r7,&t,4);func_800674C4(m,&c);return evaluate_lap();
    };
    for(unsigned completed: {0u,1u,7u}) {
        set_progress(4,.05f,200,completed);
        check(recover_progress(2,.85f)==1 && std::abs(c.f0.fl+20)<.001f,
              "backward circuit recovery subtracts distance instead of awarding a lap");
        check(c.r5==0x12345678 && c.f24.fl==23.5f && c.r29==guest_address(sp),
              "recovery preserves caller arguments and saved floating registers");
        check(std::abs(evaluate_lap()-(185+completed*300))<.001f && get(state+4)==completed,
              "backward recovery retains earned laps");
        set_progress(6,.4f,300,completed);
        check(recover_progress(6,.6f)==1 &&
              std::abs(evaluate_lap()-(360+completed*300))<.001f && get(state+4)==completed+1,
              "genuine forward crash crossing still earns exactly one lap");
        for(unsigned repeat=0;repeat<20;++repeat) {
            check(recover_progress(6,.4f)==1 &&
                  std::abs(evaluate_lap()-(340+completed*300))<.001f,
                  "backward finish recross lowers absolute progress");
            check(std::abs(drive(6,.6f)-(360+completed*300))<.001f && get(state+4)==completed+1,
                  "ordinary driving after recovery cannot farm laps");
        }
        drive(6,.95f);
        check(recover_progress(2,.05f)==1 &&
              std::abs(evaluate_lap()-(405+completed*300))<.001f && get(state+4)==completed+1,
              "forward array seam does not add a second lap");
        for(unsigned repeat=0;repeat<20;++repeat) {
            check(recover_progress(6,.95f)==1 &&
                  std::abs(evaluate_lap()-(395+completed*300))<.001f,
                  "backward array seam stays near the old distance");
            check(std::abs(drive(2,.05f)-(405+completed*300))<.001f && get(state+4)==completed+1,
                  "array seam round trip cannot farm laps");
        }
    }
    set_progress(2,.05f,100);
    check(recover_progress(4,.9f)==1 && std::abs(evaluate_lap()+10)<.001f && get(state+4)==0,
          "backtracking before first lap uses negative distance without unsigned lap count");
    check(recover_progress(2,.05f)==1 && std::abs(evaluate_lap()-105)<.001f && get(state+4)==0,
          "returning from prestart recovery restores original distance");
    const auto rejected_progress=[&](const char* message) {
        const auto before=memory;
        check(recover_progress(2,.5f)==0 &&
              std::memcmp(m+(state-0x80000000),before.data()+(state-0x80000000),0x64)==0,message);
    };
    write_u16(m,actor+0x26,1);rejected_progress("AI circuit recovery retains original progress");
    write_u16(m,actor+0x26,0);
    put(state+0x4C,1);rejected_progress("busted and wrecked recovery retains original progress");put(state+0x4C,0);
    put(0x800D763C,0);rejected_progress("open road recovery retains original progress");put(0x800D763C,6);
    num(0x800D7630,0);rejected_progress("invalid lap length leaves progress unchanged");num(0x800D7630,300);
    put(state,8);rejected_progress("unreachable old circuit segment retains native fallback");put(state,2);
    session.active=session.connected=session.authoritative=true;session.authority_humans=0;
    rejected_progress("online NPC recovery retains original progress");
    session.authority_humans=1;set_progress(4,.05f,200);
    check(recover_progress(2,.85f)==1,"online authoritative human recovery corrected");
    constexpr unsigned remote_actor=actor+13*0x118;
    std::memcpy(m+(remote_actor-0x80000000),m+(actor-0x80000000),0x118);
    put(remote_actor+8,0xFFFFFFFF);session.authority_humans=1u<<13;
    set_progress(4,.05f,200);c.r16=guest_address(state);c.r17=guest_address(remote_actor);
    put(sp+0x58,2);num(sp+0x5C,.85f);
    check(rr64_roaming_recovery_progress(m,&c)==1 && std::abs(evaluate_lap()-185)<.001f,
          "fourteenth authoritative human uses slot ownership instead of local controller port");
    session.authority_humans=0;
    {
        rr64::prediction::ReplayScope replay({true,true,true,false,false,0,1});
        check(recover_progress(2,.5f)==1,"recovery progress uses captured replay ownership");
    }
    session={};
    put(0x800D763C,0);
    put(0x800A6540,0xFFFFFFFF);
    check(!rr64_roaming_recovery_point(m,&c),"malformed route count rejected");
    std::printf("Roaming route: %u checks, %u failures\n",tests,failed);
    return failed?1:0;
}
