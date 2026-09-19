#pragma once
#include "rr64_world_sync.hpp"
namespace rr64::world_sync {
// Native roster compaction is not a traffic-state change. Compare complete
// validated captures by race identity, retaining both indices for diagnostics.
struct TrafficComparison {
    struct Difference {unsigned live=capacity,replay=capacity;};
    bool valid=false;
    unsigned count=0;
    std::array<Difference,capacity*2> differences{};
};
inline TrafficComparison compare_traffic(const std::array<Traffic,capacity> &a,
                                        const std::array<Traffic,capacity> &b){
    TrafficComparison result;
    if(!valid(Snapshot{1,1,a}) || !valid(Snapshot{1,1,b}))return result;
    result.valid=true;
    std::array<bool,capacity> matched{};
    for(unsigned i=0;i<capacity;++i){
        if(!a[i].active)continue;
        unsigned j=0;
        while(j<capacity && (!b[j].active || b[j].id!=a[i].id))++j;
        if(j<capacity)matched[j]=true;
        if(j==capacity || a[i]!=b[j])result.differences[result.count++]={i,j};
    }
    for(unsigned j=0;j<capacity;++j)
        if(b[j].active && !matched[j])result.differences[result.count++]={capacity,j};
    return result;
}
}
