#pragma once
#include "rr64_world_sync.hpp"

namespace rr64::world_sync {
// A bounded reconciliation plan, not a native allocator. Local indices refer
// only to the observed roster; no network slot or remote address is a pointer.
// The bridge must retire removals through native scene cleanup BEFORE creating
// replacements: inactive scene nodes may still own shared model resources.
struct TrafficPlan {
    struct Update { unsigned local=0, remote=0; };
    std::array<unsigned,capacity> remove{},create{};
    std::array<Update,capacity> update{};
    unsigned removes=0,creates=0,updates=0;
};
inline bool plan_traffic(const Snapshot &local,const Snapshot &remote,TrafficPlan &out) {
    if(!valid(local) || !valid(remote) || local.round!=remote.round) return false;
    TrafficPlan p{};
    std::array<bool,capacity> retained{};
    for(unsigned i=0;i<capacity;++i) {
        const auto &a=local.traffic[i];
        if(!a.active) continue;
        bool keep=false;
        for(unsigned j=0;j<capacity;++j) {
            const auto &b=remote.traffic[j];
            if(!b.active || a.id!=b.id) continue;
            // Reusing an identity with a different model requires the full
            // model/collision setup. A position write cannot change the car.
            if(a.model==b.model && a.kind==b.kind) {
                p.update[p.updates++]={i,j};retained[j]=true;keep=true;
            }
            break;
        }
        if(!keep) p.remove[p.removes++]=i;
    }
    for(unsigned j=0;j<capacity;++j)
        if(remote.traffic[j].active && !retained[j]) p.create[p.creates++]=j;
    out=p;return true;
}
}
