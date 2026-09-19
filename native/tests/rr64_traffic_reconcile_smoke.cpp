#include "rr64_traffic_reconcile.hpp"
#include <cstdlib>
#include <cstdio>
using namespace rr64::world_sync;
void check(bool b){if(!b)std::abort();}
Snapshot roster(unsigned base) {
    Snapshot s{};s.round=1;s.tick=1;
    for(unsigned i=0;i<capacity;++i) {
        auto &v=s.traffic[i];v.active=1;v.id=base+i;v.model=0xD8;v.kind=1;
    }
    return s;
}
int main() {
    auto local=roster(100),remote=local;TrafficPlan p{};
    // Native compaction changes indices without changing vehicle identity.
    for(unsigned i=0;i<capacity;++i) remote.traffic[i]=local.traffic[capacity-1-i];
    check(plan_traffic(local,remote,p) && p.updates==20 && !p.removes && !p.creates);
    for(unsigned i=0;i<capacity;++i) check(p.update[i].local==i && p.update[i].remote==19-i);
    remote=roster(200);
    check(plan_traffic(local,remote,p) && p.removes==20 && p.creates==20 && !p.updates);
    remote=local;remote.traffic[7].model=0xD9;
    check(plan_traffic(local,remote,p) && p.removes==1 && p.creates==1 && p.updates==19);
    check(p.remove[0]==7 && p.create[0]==7);
    remote.traffic={};
    check(plan_traffic(local,remote,p) && p.removes==20 && !p.creates && !p.updates);
    // Invalid input must leave the previous complete plan untouched.
    remote=roster(100);remote.traffic[1].id=100;
    check(!plan_traffic(local,remote,p) && p.removes==20 && !p.updates);
    remote=roster(100);remote.round=2;check(!plan_traffic(local,remote,p));
    std::puts("traffic reconciliation identity/capacity/replacement checks passed");
}
