#include "rr64_netplay.hpp"
#include "recomp.h"
#include <vector>
#include <deque>
#include <cstdlib>
#include <cstring>
namespace {rr64::netplay::Status status;std::deque<rr64::netplay::HitEvent> queue;unsigned calls=0;}
namespace rr64::netplay {
Status get_status(){return status;}
bool submit_hit(HitEvent e){queue.push_back(e);return true;}
bool take_hit(HitEvent &e){if(queue.empty())return false;e=queue.front();queue.pop_front();return true;}
}
#include "../src/rr64_online_combat.cpp"
void check(bool b){if(!b)std::abort();}
extern "C" void func_80061224(unsigned char *m,recomp_context *c){
 check(!rr64_online_hit(m,c,0));++calls;c->r4=0; // generated callee clobbers registers
}
extern "C" void func_800616BC(unsigned char *m,recomp_context *c){check(!rr64_online_hit(m,c,1));++calls;}
int main(){
 // The two native assist-credit branches pass actor+2C. Reject stale/null
 // references and every misaligned/interior address, while retaining all14
 // real racers (including AI and disconnected players' existing records).
 for(unsigned offset=0;offset<14u*0x118u;++offset)
  check(rr64_valid_combat_statistics(0x800d859cu+offset)==(offset%0x118u==0));
 for(unsigned bad:{0u,0x2cu,0xffff002cu,0x800d859bu,0x800d859cu+14u*0x118u,0xffffffffu})
  check(!rr64_valid_combat_statistics(bad));
 using namespace rr64;std::vector<unsigned char> memory(16*1024*1024);auto *m=memory.data();
 for(unsigned g=0;g<14;++g){unsigned a=0x800d8570+g*0x118,b=0x80100000+g*engine::bike::stride,r=0x80300000+g*engine::rider::stride;
 engine::write_u32(m,a+0xe0,b);engine::write_u32(m,a+0xe4,r);
 engine::write_u32(m,b+engine::bike::rider_pointer,r);engine::write_u32(m,r+engine::rider::bike_pointer,b);
 engine::write_u32(m,r+0x568,attack_visual::descriptor_base+7*attack_visual::descriptor_stride);engine::write_u32(m,r+0x564,1);}
 for(unsigned count:{2u,4u,14u})for(unsigned local=0;local<count;++local){
 status={};status.active=status.connected=true;status.phase=netplay::Phase::Race;status.local_slot=local;status.replicated_riders=count>4;status.game_setup.revision=7;
 for(auto &p:status.players)p.connected=true;
 recomp_context c{};c.r29=static_cast<std::int64_t>(static_cast<std::int32_t>(0x807f0000));
 c.r4=actor_for(local,status);c.r5=actor_for((local+1)%count,status);c.r6=std::bit_cast<unsigned>(5.f);
 check(rr64_online_hit(m,&c,0)==1 && queue.size()==1 && queue.front().attacker==local);
 // The transport delivers an approved hit only to the victim owner.
 std::swap(queue.front().attacker,queue.front().victim);
 const auto saved=c;const auto before=memory;rr64_online_combat_drain(m,&c);
 check(queue.empty() && memory==before && std::memcmp(&c,&saved,sizeof(c))==0);
 c.r4=actor_for((local+1)%count,status);check(rr64_online_hit(m,&c,0)==1 && queue.empty());
 status.connected=false;check(rr64_online_hit(m,&c,0)==0);
 }
 check(calls==20);
 // Authority switches the decision to the host, for both weapon and impact
 // contacts, without replaying the older owner-proposal queue a second time.
 status.active=status.connected=true;status.authoritative=true;
 recomp_context c{};
 for(bool host:{false,true}){
  status.is_host=host;
  for(unsigned kind:{0u,1u})check(rr64_online_hit(m,&c,kind)==(host?0:1));
  rr64_online_combat_drain(m,&c);check(queue.empty() && calls==20);
  const auto rules=prediction::capture_session_rules(status);
  prediction::ReplayScope replay(rules);
  status.is_host=!host; // transport changed after the historical input
  for(unsigned kind:{0u,1u})check(rr64_online_hit(m,&c,kind)==(host?0:1));
  rr64_online_combat_drain(m,&c);check(queue.empty() && calls==20);
 }
}
