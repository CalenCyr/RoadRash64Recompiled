#include "rr64_authoritative_round.hpp"
#include <cstdio>
#include <cstdlib>
#include <source_location>
using namespace rr64::authority;
static void check(bool value,std::source_location where=std::source_location::current()){
 if(!value){std::fprintf(stderr,"resampling check failed line %u\n",where.line());std::exit(1);}
}
int main(){
 for(unsigned client_hz:{30u,59u,60u,120u,144u})for(unsigned host_hz:{30u,59u,60u}){
  HostRound host;check(host.reset(1,2));unsigned produced=0,consumed=0,max_pending=0;
  // Integer time events avoid a floating scheduling model hiding drift.
  for(unsigned ms=0;ms<=60000;++ms){
   while(produced<static_cast<std::uint64_t>(ms)*client_hz/1000){
    InputBatch packet{};packet.round=1;packet.count=1;
    packet.commands[0]={1,++produced,0x8000,30,0};check(host.receive(1,packet));
   }
   if(static_cast<std::uint64_t>(ms)*host_hz/1000>static_cast<std::uint64_t>(ms?ms-1:0)*host_hz/1000){
    Step step;check(host.begin(step));check(host.stamp().acknowledged[1]==consumed);
    check(host.finish(step.tick));consumed=host.stamp().acknowledged[1];
   }
   if(produced-consumed>max_pending)max_pending=produced-consumed;
  }
  check(max_pending<=5 && produced==consumed);
 }
 HostRound host;check(host.reset(1,2));InputBatch burst{};burst.round=1;burst.count=3;
 burst.commands[0]={1,1,0x20,1,0,action_eject};burst.commands[1]={1,2,0,2,0};burst.commands[2]={1,3,0,3,0};
 check(host.receive(1,burst));Step step;check(host.begin(step));
 check(step.inputs[1].sequence==3 && step.inputs[1].x==3 && step.inputs[1].buttons==0x20 && step.presses[1]==0x20 && step.inputs[1].actions==action_eject);
 check(host.stamp().acknowledged[1]==0);check(host.finish(step.tick));check(host.stamp().acknowledged[1]==3);
 check(host.begin(step));check(step.inputs[1].buttons==0 && step.inputs[1].actions==0 && step.presses[1]==0);check(host.finish(step.tick));
 // A missing sample remains a gap; a later sample cannot erase its tap.
 burst.count=1;burst.commands[0]={1,5,0,5,0};check(host.receive(1,burst));check(host.begin(step));check(step.inputs[1].sequence==0);check(host.finish(step.tick));
 burst.commands[0]={1,4,0x4000,4,0};check(host.receive(1,burst));check(host.begin(step));
 check(step.inputs[1].sequence==5 && step.inputs[1].buttons==0x4000 && step.inputs[1].x==5);check(host.finish(step.tick));
 std::puts("15 native cadence pairs stay bounded for60s; burst taps/ejects retained, ACK delayed until update completes, gaps preserved");
}
