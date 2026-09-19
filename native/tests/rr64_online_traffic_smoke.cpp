#include "rr64_netplay.hpp"
#include "recomp.h"
#include "rr64_engine_layout.hpp"
#include <vector>
#include <cstdlib>
#include <cstring>
namespace { rr64::netplay::Status status; rr64::world_sync::Snapshot remote; bool available=true;unsigned step=0; }
namespace rr64::netplay {Status get_status(){return status;}bool get_world_state(world_sync::Snapshot &s){s=remote;return available;}}
#include "../src/rr64_online_traffic.cpp"
void check(bool ok){if(!ok)std::abort();}
extern "C" void func_8006B9E8(unsigned char *m,recomp_context *c){check(step++==0);rr64::engine::write_u32(m,c->r5,2);rr64::engine::write_float(m,c->r6,.5f);c->r2=1;}
extern "C" void func_80046488(unsigned char*,recomp_context *c){check(step++==1);c->r2=guest(0x80100000);}
extern "C" void func_80077498(unsigned char *m,recomp_context *c){check(step++==2);rr64::engine::write_u32(m,c->r4+4,c->r5);rr64::engine::write_u32(m,c->r4,c->r6);}
extern "C" void func_80047668(unsigned char *m,recomp_context *c){
 check(step++==3);rr64_online_traffic_model(c);check(c->r16==0xda && c->r2==2);
 rr64::engine::write_u32(m,0x80200000,4);rr64::engine::write_u32(m,0x80200004,c->r4);
 // Native47668 stores the flattened graphics resource, not model DA.
 rr64::engine::write_u32(m,0x80200040,33);rr64::engine::write_u32(m,0x800a145c,0x80200000);
}
extern "C" void func_8006BA88(unsigned char *m,recomp_context *c){
 check(step++==4 && c->r5==2 && c->r6==0x3f000000);
 rr64::engine::write_u32(m,0x800d76e0,c->r4);rr64::engine::write_u32(m,0x800a6528,1);
}
int main(){
 using namespace rr64;
 std::vector<unsigned char> memory(16*1024*1024);auto *m=memory.data();
 engine::write_u32(m,0x800df210+0xda*4,0x80300000);
 engine::write_u32(m,0x80300000,3);engine::write_u32(m,0x80300004,0x01000000);
 engine::write_u32(m,0x800a7734,30);
 status.active=status.connected=true;status.phase=netplay::Phase::Race;status.game_setup.revision=1;
 remote.round=1;remote.tick=1;auto &v=remote.traffic[0];
 v.active=v.motion_valid=1;v.id=42;v.model=0xda;v.kind=2;v.road_distance=100;
 for(unsigned i=0;i<v.motion.size();++i)v.motion[i]=float(i);
 v.directions={1,2,3,4,5,6};
 recomp_context ctx{};ctx.r29=guest(0x807f0000);auto saved=ctx;
 status.is_host=true;auto before=memory;rr64_online_traffic_prepare(m);rr64_online_traffic_finish(m,&ctx);
 check(memory==before && step==0);
 status.is_host=false;rr64_online_traffic_prepare(m);check(rr64_online_traffic_owned());
 rr64_online_traffic_finish(m,&ctx);check(step==5 && std::memcmp(&ctx,&saved,sizeof(ctx))==0);
 for(unsigned i=0;i<v.motion.size();++i){float f=0;engine::read_float(m,0x801000a8+i*4,f);check(f==float(i));}
 check(word(m,0x80100004)==42 && word(m,0x800a6528)==1);
 // Same identity is updated in place; no extra allocation or spawn reset.
 remote.tick=2;v.motion[0]=900;rr64_online_traffic_prepare(m);rr64_online_traffic_finish(m,&ctx);check(step==5);
 float f=0;engine::read_float(m,0x801000a8,f);check(f==900);
 // Retirement only marks inactive. The native loop retains resource cleanup.
 remote.tick=3;v.active=0;rr64_online_traffic_prepare(m);
 std::uint16_t active=1;engine::read_u16(m,0x80100334,active);check(!active);
 check(word(m,0x800a145c)==0x80200000 && word(m,0x800a6528)==1);
 // A round transition must not inherit suppression from the preceding race.
 status.game_setup.revision=2;check(!rr64_online_traffic_owned());
 before=memory;rr64_online_traffic_finish(m,&ctx);check(memory==before);
 rr64_online_traffic_prepare(m);check(!rr64_online_traffic_owned());
 status.connected=false;check(!rr64_online_traffic_owned());
 rr64_online_traffic_prepare(m);before=memory;rr64_online_traffic_finish(m,&ctx);check(memory==before);
}
