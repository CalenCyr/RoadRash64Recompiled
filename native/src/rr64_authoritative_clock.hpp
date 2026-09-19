#pragma once
#include <algorithm>
#include <cstdint>
#include <limits>

namespace rr64::authority {
// Fixed-step scheduling primitive, not a change to native physics. The caller
// chooses the shared simulation quantum and supplies elapsed simulation time,
// excluding pauses. A native executor must honor every planned step before
// consuming it. Rendering cadence never directly determines input count.
class TickBudget {
    std::uint64_t quantum_=0,limit_=0,pending_=0,completed_=0;
public:
    bool reset(std::uint64_t quantum_ns,std::uint64_t maximum_backlog_ns){
        if(!quantum_ns || maximum_backlog_ns<quantum_ns)return false;
        quantum_=quantum_ns;limit_=maximum_backlog_ns;pending_=completed_=0;return true;
    }
    bool add_elapsed(std::uint64_t ns){
        if(!quantum_ || ns>limit_-pending_)return false;
        pending_+=ns;return true;
    }
    unsigned plan(unsigned maximum_steps)const{
        return quantum_?static_cast<unsigned>(std::min<std::uint64_t>(maximum_steps,pending_/quantum_)):0;
    }
    bool consume(unsigned completed_steps){
        if(!quantum_ || completed_steps>pending_/quantum_ ||
           completed_steps>std::numeric_limits<std::uint64_t>::max()-completed_)return false;
        pending_-=std::uint64_t(completed_steps)*quantum_;completed_+=completed_steps;return true;
    }
    std::uint64_t pending_ns()const{return pending_;}
    std::uint64_t completed()const{return completed_;}
};
}
