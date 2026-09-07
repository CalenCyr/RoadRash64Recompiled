#include "rr64_video_mode.hpp"
#include "rr64_actor_render_fixture.hpp"
#include "recomp.h"
#include <cstdio>
#include <cmath>
#include "../lib/rt64/src/common/rt64_rr64_video_aspect.h"
extern "C" void func_8000A310(unsigned char*, recomp_context*);
extern "C" void func_8000A35C(unsigned char*, recomp_context*);
extern "C" void func_8000A3A8(unsigned char*, recomp_context*);
int main() {
    using namespace rr64::engine;
    rr64::lod::test::Fixture f;
    auto* m=f.live.data(); bool passed=true;
    const auto check=[&](bool ok,const char* label){if(!ok){std::printf("FAIL %s\n",label);passed=false;}};
    const auto read=[&](unsigned address){unsigned v=0;read_u32(m,address,v);return v;};
    write_u32(m,0x800bc9c4u,640u);write_u32(m,0x800bc9ccu,240u);
    write_u32(m,0x8009cc68u,0x8000a3a8u);
    for(unsigned selected=0;selected<4;++selected) {
        write_u32(m,0x8009cc54u,selected);
        for(unsigned mode:{9u,10u,11u,18u,19u,20u,23u,24u,25u,28u,29u,30u}) {
            write_u32(m,globals::main_mode,mode);write_u32(m,globals::pending_mode,mode);
            const auto before=f.live;
            check(rr64::video::combined_callback(m,0x8000a310u,true)==0x8000a3a8u,"combined mode admitted in retained one-player race");
            check(rr64::video::combined_callback(m,0x8000a310u,false)==0x8000a310u,"legacy option unchanged");
            check(before==f.live,"callback selection does not alter saved mode or guest memory");
        }
    }
    for(unsigned address:{0x800a6578u,0x8009db88u}) {
        write_u32(m,address,2u);
        check(rr64::video::combined_callback(m,0x8000a310u,true)==0x8000a310u,"local multiplayer retains original video mode");
        write_u32(m,address,1u);
    }
    for(unsigned address:{0x800a4f24u,0x800bc9c4u,0x800bc9ccu,0x8009cc68u}) {
        const unsigned original=read(address);write_u32(m,address,1u);
        check(rr64::video::combined_callback(m,0x8000a310u,true)==0x8000a310u,"capacity, lock and callback checks");
        write_u32(m,address,original);
    }
    write_u32(m,globals::pending_mode,0u);
    check(rr64::video::combined_callback(m,0x8000a310u,true)==0x8000a310u,"scene exit unchanged");
    recomp_context c{};
    func_8000A310(m,&c);
    check(read(0x800b07dcu)==512 && read(0x800b07e0u)==150 && read(0x8009cc40u)==45,"original Wide is 512x150 letterbox");
    func_8000A3A8(m,&c);
    check(read(0x800b07dcu)==512 && read(0x800b07e0u)==240 && read(0x8009cc40u)==0,"combined original High Res writer is full-height 512x240");
    func_8000A35C(m,&c);
    check(read(0x800b07dcu)==640 && read(0x800b07e0u)==120 && read(0x8009cc40u)==60,"legacy Letterbox remains available");
    const float target=RT64::RR64Video::combinedTarget;
    for(unsigned width:{320u,512u}) {
        const float scale=target/RT64::RR64Video::sourceAspect(true,width,240u);
        check(std::abs(scale-4.0f/3.0f)<0.00001f,"one identical widescreen expansion in Normal and High Res");
        check(std::abs((320.0f*scale/240.0f)-target)<0.00001f,"VI television viewport presents at 16:9");
    }
    check(RT64::RR64Video::sourceAspect(false,512u,240u)==512.0f/240.0f,"legacy renderer aspect is unchanged");
    std::puts(passed?"[RR64-VIDEO] PASS":"[RR64-VIDEO] FAIL");return passed?0:1;
}
