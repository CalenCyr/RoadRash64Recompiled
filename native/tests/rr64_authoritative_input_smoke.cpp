#include "rr64_authoritative_input.hpp"
#include <cstdlib>
#include <array>
using namespace rr64::authority;
void check(bool b){if(!b)std::abort();}
struct State {int position=0,attacks=0;};
bool simulate(State &s,const Command &c,bool replay) {
    s.position+=c.x;if(c.buttons&1)++s.attacks;return replay;
}
int main(){
 for(unsigned slot=0;slot<14;++slot){
 HostInput host;ClientHistory client;host.reset(7);client.reset(7);
 Command a,b,c;check(client.append(1,2,0,a));check(client.append(0,3,0,b));check(client.append(0,4,0,c));
 check(host.receive(c));Command staged;check(!host.stage(staged)); // reordered, missing tap
 check(host.receive(a));check(host.receive(a));auto conflict=a;conflict.x=9;check(!host.receive(conflict));
 check(host.stage(staged) && staged==a && host.processed()==0);
 check(!host.commit(2));check(host.commit(1));check(!host.commit(1));
 State predicted{999,999},authoritative{2,1};
 check(client.reconcile(7,1,1,authoritative,predicted,simulate));check(predicted.position==9 && predicted.attacks==1);
 std::array<Command,8> resend{};check(client.resend(resend)==2 && resend[0]==b);
 check(host.receive(b));check(host.stage(staged)&&staged==b);check(host.commit(2));
 check(host.stage(staged)&&staged==c);check(host.commit(3));
 check(!client.reconcile(7,1,1,authoritative,predicted,simulate));
 check(!client.reconcile(8,2,1,authoritative,predicted,simulate));
 check(!client.reconcile(7,2,4,authoritative,predicted,simulate));
 auto before=predicted;check(!client.reconcile(7,2,1,authoritative,predicted,[](auto &s,auto,bool){s.position=-1;return false;}));
 check(predicted.position==before.position && client.acknowledged()==1);
 check(client.reconcile(7,3,3,State{9,1},predicted,simulate));check(client.pending()==0);
 for(unsigned i=0;i<history_capacity;++i)check(client.append(0,1,0,a));
 check(!client.append(0,1,0,a)); // never silently erase unacknowledged history
 host.reset(8);check(!host.receive(c));client.reset(8);check(client.pending()==0);
 }
 // Thousands of steps wrap the bounded ring without losing press/release order.
 HostInput host;ClientHistory client;host.reset(1);client.reset(1);State truth{},predicted{};
 for(unsigned tick=1;tick<10000;++tick){Command c;check(client.append(tick%3==0,1,0,c));check(host.receive(c));Command staged;check(host.stage(staged));simulate(truth,staged,true);check(host.commit(staged.sequence));check(client.reconcile(1,tick,host.processed(),truth,predicted,simulate));check(predicted.position==truth.position && predicted.attacks==truth.attacks);}
}
