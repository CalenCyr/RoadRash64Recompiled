// Offline coordinator audit. Rates here are hypothetical SIMULATION rates,
// not display FPS, and this does not execute native bike physics.
#include "rr64_authoritative_round.hpp"
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <vector>
using namespace rr64::authority;
void check(bool b){if(!b)std::abort();}
struct Result {double host,client,oldest_ms=0,overflow_at=-1;unsigned pending=0;};
Result run(double host_hz,double client_hz){
 HostRound host;check(host.reset(7,3));ClientHistory history;history.reset(7);
 std::vector<double> created{0};double next_client=0,next_host=0;
 Result result{host_hz,client_hz};
 while(std::min(next_client,next_host)<60.){
  if(next_client<=next_host){
   Command command;
   if(!history.append(0x8000,0,64,command)){result.overflow_at=next_client;break;}
   created.push_back(next_client);
   InputBatch batch;batch.round=7;batch.count=1;batch.commands[0]=command;
   check(host.receive(1,batch));next_client+=1./client_hz;
  }else{
   Step step;check(host.begin(step));check(host.finish(step.tick));
   const auto stamp=host.stamp();
   // Immediate ACK, no packet loss, and no replay cost: best-case transport.
   check(history.commit_replay(7,stamp.tick,stamp.acknowledged[1],history.last_sequence()));
   next_host+=1./host_hz;
  }
  result.pending=std::max(result.pending,static_cast<unsigned>(history.pending()));
  if(history.pending())result.oldest_ms=std::max(result.oldest_ms,
   (std::min(next_client,next_host)-created[history.acknowledged()+1])*1000.);
 }
 return result;
}
int main(){
 std::puts("{\"scope\":\"synthetic simulation cadence, immediate transport ACK, no native physics\",\"cases\":[");
 const double rates[][2]={{60,60},{59,60},{30,60},{60,30}};
 for(unsigned i=0;i<4;++i){
  const auto r=run(rates[i][0],rates[i][1]);
  std::printf("%s{\"host_hz\":%.2f,\"client_hz\":%.2f,\"max_pending\":%u,\"oldest_ms\":%.3f,\"overflow_seconds\":%.3f}",
   i?",\n":"",r.host,r.client,r.pending,r.oldest_ms,r.overflow_at);
 }
 std::puts("\n],\"interpretation\":\"One input per independent native step requires cadence alignment before activation; this is an audit, not a passing latency claim.\"}");
}
