#include "rr64_roaming_route.hpp"
#include "rr64_experimental_course_route.hpp"
#include "rr64_engine_layout.hpp"
#include "rr64_prediction_rules.hpp"
#include <cstdio>
#include <vector>
#include <cmath>
#include <limits>
namespace {
rr64::netplay::Status session;
rr64::experimental_course::RouteData imported_route;
bool imported_active=false,custom_cop=false;
constexpr unsigned route_heap=0x80200000;
}
namespace rr64::netplay {
Status get_status(){return session;}
PhysicsRules get_physics_rules(){return {};}
}
namespace rr64::experimental_course {
bool active() noexcept { return imported_active; }
const RouteData* route_data() noexcept { return &imported_route; }
}
extern "C" int rr64_custom_cop_enabled() { return custom_cop; }
extern "C" int rr64_custom_cop_is_player(unsigned char*,unsigned) { return 0; }
// Allocation is independent of race rules; supply the validated free block.
extern "C" void func_8001BDF8(unsigned char*,recomp_context* c) {
    c->r2=rr64::engine::guest_address(route_heap+8);
}
extern "C" void func_8001C084(unsigned char*,recomp_context*) {}
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
    // Install a real closed imported route before executing the game's lap
    // calculation. A forced lap flag used to finish Tag riders after one lap.
    std::array<unsigned char,11*16> records{};
    const float x[11]={0,50,100,150,200,100,0,50,100,150,200};
    for(unsigned i=0;i<11;++i) {
        auto* record=records.data()+i*16;
        record[0]=i==10?3:(i&1)?4:1;record[4]=record[5]=1;
        unsigned bits;std::memcpy(&bits,&x[i],4);
        for(unsigned b=0;b<4;++b) record[8+b]=static_cast<unsigned char>(bits>>(24-b*8));
    }
    imported_route.records_be=records.data();imported_route.byte_count=records.size();
    imported_route.record_count=11;imported_route.wrap_segment=imported_route.finish_segment=6;
    imported_route.prior_laps_required=2;imported_route.lap_period=400;
    imported_route.lap_threshold=450;imported_route.finish_threshold=1250;
    imported_route.finish_parameter=.5f;imported_route.finish[0]=50;
    imported_route.native_laps=true;imported_active=true;
    constexpr unsigned descriptor=0x80110000;
    put(0x800D7620,descriptor);put(0x800BBD00,route_heap);
    put(route_heap,4096);write_u16(m,route_heap+4,8);
    write_s8(m,route_heap+6,0);write_s8(m,route_heap+7,0);
    put(route_heap+4104,0);write_s8(m,route_heap+4110,1);
    for(bool cop: {false,true}) for(unsigned type=1;type<=8;++type) {
        custom_cop=cop;
        const bool point_mode=type==6 || type==7;
        const unsigned laps=type==4?6:type==3 || type==8?2:0;
        put(0x8009EAE4,type);put(0x800D8524,cop?8:type);
        put(0x800A6544,0);write_u16(m,0x800D7680,point_mode?0:1);
        check(rr64_experimental_course_build_route(m,&c)==1,"imported route installs in each native mode");
        std::uint16_t lap_enabled=0;read_u16(m,0x800D7680,lap_enabled);
        check(lap_enabled==(!point_mode || cop),"imported route retains native point-mode lap flag");
        check(get(0x800D7644)==laps && val(0x800D7634)==450+laps*400,
              "imported route retains each mode's required lap count");
        set_progress(6,.6f,400);put(state+0x58,0);
        const float distance=evaluate_lap();
        const bool retains_laps=!point_mode || cop;
        check(get(state+4)==unsigned(retains_laps) && std::abs(distance-(retains_laps?460:60))<.001f,
              "native finish crossing retains laps only for finish-based modes");
        check(get(state+0x58)==unsigned(type==6 && !cop),
              "native Deathmatch lap awards a point and Tag lap awards none");
        if(type==7 && !cop)
            check(!(distance>=val(0x800D7634) && get(state+4)>get(0x800D7644)),
                  "imported Tag lap cannot trigger native finish eligibility");
        rr64::experimental_course::restore_descriptor(m);
        read_u16(m,0x800D7680,lap_enabled);
        check(lap_enabled==!point_mode,"stock return restores original lap flag");
    }
    imported_active=custom_cop=false;
    std::printf("Roaming route: %u checks, %u failures\n",tests,failed);
    return failed?1:0;
}
