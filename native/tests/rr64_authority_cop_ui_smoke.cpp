#include "rr64_netplay.hpp"
#include "rr64_engine_layout.hpp"
#include "recomp.h"
#include <vector>
#include <string>
#include <cstdio>
#include <cstdlib>
#include <bit>
extern "C" void rr64_custom_cop_hud(unsigned char*,void*);
using namespace rr64;
netplay::Status status;netplay::AuthorityFrame frame;bool available=true,local_mode=false;
struct Draw {std::string label;float x,y;};std::vector<Draw> draws;
void check(bool b){if(!b){std::fputs("authority cop HUD check failed\n",stderr);std::exit(1);}}
namespace rr64::netplay {Status get_status(){return status;}bool authority_get_frame(AuthorityFrame &out){out=frame;return available;}}
extern "C" int rr64_custom_cop_active(){return local_mode;}
extern "C" int rr64_custom_cop_can_start(unsigned char*){return 1;}
extern "C" float rr64_custom_cop_win_age(unsigned char*){check(local_mode);return -1;}
extern "C" float rr64_custom_cop_cue(unsigned char*,unsigned){check(local_mode);return -1;}
extern "C" void func_800796F8(unsigned char *m,recomp_context *c){
 Draw draw;std::uint8_t byte=0;
 for(unsigned i=0;i<48;++i){engine::read_u8(m,unsigned(c->r4)+i,byte);if(!byte)break;draw.label+=char(byte);}
 draw.x=std::bit_cast<float>(std::uint32_t(c->r5));draw.y=std::bit_cast<float>(std::uint32_t(c->r6));
 for(unsigned i=0;i<4;++i){engine::read_u8(m,unsigned(c->r7)+i,byte);check(byte==(i==2?0:255));}
 check(draw.x>=0 && draw.x<320 && draw.y>=0 && draw.y<240);draws.push_back(draw);
}
int main(){
 std::vector<unsigned char> memory(8*1024*1024);auto *m=memory.data();recomp_context c{};
 c.r29=static_cast<std::int64_t>(static_cast<std::int32_t>(0x807f0000));
 engine::write_u32(m,0x800a6578,4); // hostile native split-screen layout must not leak into peer HUD
 for(unsigned slot:{0u,3u,13u}){
  status={};status.active=status.connected=status.authoritative=status.replicated_riders=true;status.local_slot=slot;
  frame={};frame.cop_mode=1;frame.riders[slot].active=true;frame.outcomes[slot].role=7;frame.outcomes[slot].busts=100+slot;
  draws.clear();rr64_custom_cop_hud(m,&c);
  check(draws.size()==1 && draws[0].label=="Busts: "+std::to_string(100+slot) && draws[0].x==250 && draws[0].y==8);
  frame.outcomes[slot].cue_age=0.4f;draws.clear();rr64_custom_cop_hud(m,&c);check(draws.size()==2 && draws[1].label=="Bust' em!");
  frame.cop_win_age=0.5f;draws.clear();rr64_custom_cop_hud(m,&c);check(draws.size()==1 && draws[0].label=="COPS WIN!");
  frame.cop_mode=0;draws.clear();rr64_custom_cop_hud(m,&c);check(draws.empty());
 }
 frame.cop_mode=1;available=false;draws.clear();rr64_custom_cop_hud(m,&c);check(draws.empty());
 status={};local_mode=true;engine::write_u32(m,0x800a6578,2);
 for(unsigned i=0;i<2;++i){unsigned a=0x800d8570+i*0x118,s=0x80500000+i*0x64;
  engine::write_u32(m,a+0x20,7);engine::write_u32(m,a+8,i);engine::write_u32(m,a+0xe8,s);engine::write_u32(m,s+0x58,i);
 }
 draws.clear();rr64_custom_cop_hud(m,&c);check(draws.size()==2 && draws[0].y==8 && draws[1].y==128);
 std::puts("authoritative cop HUD uses own network slot/full canvas; native two-view layout preserved (draw routine mocked)");
}
