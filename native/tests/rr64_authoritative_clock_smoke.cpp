#include "rr64_authoritative_clock.hpp"
#include "rr64_authoritative_round.hpp"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
using namespace rr64::authority;
void check(bool b){if(!b){std::fputs("authority clock check failed\n",stderr);std::exit(1);}}
struct Result {unsigned maximum_pending=0;std::uint64_t host_steps=0,client_steps=0;};
Result run(unsigned host_hz,unsigned client_hz){
 constexpr std::uint64_t quantum=16666667,second=1000000000,duration=60*second;
 TickBudget host_clock,client_clock;check(host_clock.reset(quantum,second));check(client_clock.reset(quantum,second));
 HostRound host;check(host.reset(7,3));ClientHistory history;history.reset(7);
 std::uint64_t hc=1,cc=1,old_host=0,old_client=0;Result result;
 while(true){
  const auto ht=hc*second/host_hz,ct=cc*second/client_hz;
  if(std::min(ht,ct)>duration)break;
  if(ct<=ht){
   check(client_clock.add_elapsed(ct-old_client));old_client=ct;++cc;
   const unsigned count=client_clock.plan(8);
   for(unsigned i=0;i<count;++i){
    Command command;check(history.append(0x8000,0,64,command));
    InputBatch batch;batch.round=7;batch.count=1;batch.commands[0]=command;
    check(host.receive(1,batch));check(client_clock.consume(1));
   }
  }else{
   check(host_clock.add_elapsed(ht-old_host));old_host=ht;++hc;
   const unsigned count=host_clock.plan(8);
   for(unsigned i=0;i<count;++i){
    Step step;check(host.begin(step));check(host.finish(step.tick));
    const auto stamp=host.stamp();
    check(history.commit_replay(7,stamp.tick,stamp.acknowledged[1],history.last_sequence()));
    check(host_clock.consume(1));
   }
  }
  result.maximum_pending=std::max(result.maximum_pending,unsigned(history.pending()));
 }
 result.host_steps=host_clock.completed();result.client_steps=client_clock.completed();
 check(result.host_steps==result.client_steps && result.host_steps==duration/quantum);
 return result;
}
int main(){
 TickBudget b;check(!b.reset(0,100) && !b.reset(100,99));check(b.reset(10,100));
 check(b.add_elapsed(35) && b.plan(2)==2 && b.pending_ns()==35);
 check(!b.consume(4) && b.pending_ns()==35);check(b.consume(2) && b.pending_ns()==15);
 check(!b.add_elapsed(86) && b.pending_ns()==15);check(b.add_elapsed(85));
 check(b.plan(3)==3 && b.consume(3) && b.pending_ns()==70); // bounded work never discards debt
 for(auto rates:{std::pair{60u,60u},{59u,60u},{30u,60u},{60u,30u},{24u,144u}}){
  const auto r=run(rates.first,rates.second);check(r.maximum_pending<=4);
  std::printf("shared quantum: host frames %u Hz, client frames %u Hz, %llu/%llu simulation steps, max pending %u\n",
   rates.first,rates.second,static_cast<unsigned long long>(r.host_steps),static_cast<unsigned long long>(r.client_steps),r.maximum_pending);
 }
 std::puts("synthetic scheduler/coordinator checks only; not wired into native simulation or a race-performance claim");
}
