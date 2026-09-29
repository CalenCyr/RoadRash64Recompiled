#include "rr64_authoritative_round.hpp"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <source_location>
#include <vector>
using namespace rr64::authority;
static void check(bool value,std::source_location where=std::source_location::current()){
 if(!value){std::fprintf(stderr,"resampling check failed line %u\n",where.line());std::exit(1);}
}
static std::uint64_t boundary(unsigned sequence,unsigned hz){return std::uint64_t(sequence)*1000000/hz;}
struct Motion {double x=0,v=0;};
static void accelerate(Motion &m,double seconds){m.x+=m.v*seconds+3.5*seconds*seconds;m.v+=7*seconds;}
int main(){
 double worst_fixed=0,worst_legacy=0;
 for(unsigned client_hz:{30u,41u,49u,59u,60u,120u,144u})for(unsigned host_hz:{30u,59u,60u})
 for(unsigned delay_ms:{0u,20u,55u}){
  HostRound host;check(host.reset(1,2));unsigned produced=0,host_steps=0,consumed=0,max_pending=0;
  struct Delivery{unsigned due;Command command;};std::vector<Delivery> delivery;
  Motion host_motion;
  // Independent integer event clocks, finite delay, duplicate and reordered
  // delivery. Client samples explicitly cover their own full native duration.
  for(unsigned ms=0;ms<=60100;++ms){
   while(ms<=60000 && produced<std::uint64_t(ms)*client_hz/1000){
    const auto sequence=++produced;
    Command command{1,sequence,0x2000,30,0,0,static_cast<unsigned>(boundary(sequence,client_hz)-boundary(sequence-1,client_hz))};
    delivery.push_back({ms+delay_ms+(sequence*7)%9,command});
    if(sequence%7==0)delivery.push_back({ms+delay_ms+10,command});
   }
   for(auto i=delivery.begin();i!=delivery.end();){
    if(i->due<=ms){InputBatch packet{};packet.round=1;packet.count=1;packet.commands[0]=i->command;
     check(host.receive(1,packet));i=delivery.erase(i);
    }else ++i;
   }
   if(std::uint64_t(ms)*host_hz/1000>std::uint64_t(ms?ms-1:0)*host_hz/1000){
    ++host_steps;const auto duration=static_cast<unsigned>(boundary(host_steps,host_hz)-boundary(host_steps-1,host_hz));
    const auto before=host.stamp();Step step;check(host.begin(step,duration));
    check(host.stamp().acknowledged==before.acknowledged && host.stamp().simulated_us==before.simulated_us);
    check(host.finish(step.tick));const auto stamp=host.stamp();consumed=stamp.acknowledged[1];
    check(consumed<=produced && boundary(consumed,client_hz)<=stamp.simulated_us[1]);
    if(stamp.simulated_us[1]){
     check((step.inputs[1].buttons&0x2000)!=0);
     accelerate(host_motion,double(duration)/1000000);
     const auto total=boundary(produced,client_hz);
     const auto cursor=stamp.simulated_us[1];
     // This fixture checks the time contract with an exact constant-force
     // integrator, not native collision/physics determinism or visual smoothness.
     Motion fixed=host_motion;
     accelerate(fixed,double(total>cursor?total-cursor:0)/1000000);
     const auto horizon=double(std::max(total,cursor))/1000000;
     worst_fixed=std::max(worst_fixed,std::abs(fixed.x-3.5*horizon*horizon));
     Motion legacy=host_motion;
     accelerate(legacy,double(total-boundary(consumed,client_hz))/1000000);
     worst_legacy=std::max(worst_legacy,std::abs(legacy.x-3.5*horizon*horizon));
    }
   }
   max_pending=std::max(max_pending,produced-consumed);
  }
  check(max_pending<=16 && produced==consumed);
 }
 check(worst_fixed<1e-7 && worst_legacy>1);
 HostRound host;check(host.reset(1,2));InputBatch burst{};burst.round=1;burst.count=3;
 burst.commands[0]={1,1,0x20,1,0,action_eject,5000};burst.commands[1]={1,2,0,2,0,0,5000};burst.commands[2]={1,3,0,3,0,0,5000};
 check(host.receive(1,burst));Step step;check(host.begin(step));
 check(step.inputs[1].sequence==3 && step.inputs[1].x==3 && step.inputs[1].buttons==0x20 && step.presses[1]==0x20 && step.inputs[1].actions==action_eject);
 check(host.stamp().acknowledged[1]==0 && host.stamp().simulated_us[1]==0);
 check(host.finish(step.tick));check(host.stamp().acknowledged[1]==3 && host.stamp().simulated_us[1]==16667);
 check(host.begin(step));check(step.inputs[1].buttons==0 && step.inputs[1].actions==0 && step.presses[1]==0);check(host.finish(step.tick));
 // A missing sample remains a gap; a later sample cannot erase its tap. A
 // subsequently received interval already behind the cursor retains its edge.
 burst.count=1;burst.commands[0]={1,5,0,5,0,0,5000};check(host.receive(1,burst));check(host.begin(step));check(step.inputs[1].sequence==0);check(host.finish(step.tick));
 burst.commands[0]={1,4,0x4000,4,0,action_eject,5000};check(host.receive(1,burst));check(host.begin(step));
 check(step.inputs[1].sequence==5 && step.inputs[1].buttons==0x4000 && step.inputs[1].x==5 && step.inputs[1].actions==action_eject);check(host.finish(step.tick));
 // A partially consumed command applies its edge once, before its later ACK.
 check(host.reset(2,2));burst.round=2;burst.commands[0]={2,1,0x20,7,0,action_eject,40000};check(host.receive(1,burst));
 check(host.begin(step));check(step.inputs[1].actions==action_eject && step.presses[1]==0x20);check(host.finish(step.tick));
 check(host.stamp().acknowledged[1]==0 && host.stamp().simulated_us[1]==16667);
 check(host.receive(1,burst));check(host.begin(step));check(!step.inputs[1].actions && !step.presses[1]);check(host.finish(step.tick));
 check(host.stamp().acknowledged[1]==0 && host.stamp().simulated_us[1]==33334);
 check(host.begin(step));check(!step.inputs[1].actions && !step.presses[1]);check(host.finish(step.tick));check(host.stamp().acknowledged[1]==1);
 const auto before=host.stamp();check(!host.begin(step,0) && !host.begin(step,250001));check(host.stamp().simulated_us==before.simulated_us);
 // The elapsed hold limit is independent of a host's frame rate.
 for(unsigned duration:{10000u,20000u,100000u}){
  check(host.reset(3,2));burst.round=3;burst.commands[0]={3,1,0x2000,7,0,0,duration};check(host.receive(1,burst));
  check(host.begin(step,duration));check(host.finish(step.tick));
  for(unsigned stale=duration;stale<=300000;stale+=duration){check(host.begin(step,duration));
   check(step.inputs[1].buttons==(stale<=250000?0x2000:0));check(!step.inputs[1].actions && !step.presses[1]);check(host.finish(step.tick));}
 }
 std::printf("63 cadence/delay pairs pass; constant-force residual fixed=%.10g legacy=%.10g; partial ACKs, burst/gap edges and timed expiry pass\n",worst_fixed,worst_legacy);
}
