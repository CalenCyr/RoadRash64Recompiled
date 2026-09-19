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
    put(0x800A6540,0xFFFFFFFF);
    check(!rr64_roaming_recovery_point(m,&c),"malformed route count rejected");
    std::printf("Roaming route: %u checks, %u failures\n",tests,failed);
    return failed?1:0;
}
