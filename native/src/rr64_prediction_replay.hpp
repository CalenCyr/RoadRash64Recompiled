#pragma once
#include <cstdint>
#include <limits>

namespace rr64::prediction {
struct SessionRules {
    bool active=false,connected=false,authoritative=false,is_host=false,replicated_riders=false;
    std::uint8_t local_slot=255;
    std::uint32_t humans=0;
};
// This scope only isolates registered native side effects. The caller must
// separately provide isolated guest memory; it is not a physics replay engine.
inline thread_local bool replay_active=false;
inline thread_local std::uint64_t replay_epoch=0;
inline thread_local SessionRules replay_rules{};
inline bool active(){return replay_active;}
class ReplayScope {
    bool owned_=false;
public:
    explicit ReplayScope(const SessionRules& rules={}){
        if(!replay_active && replay_epoch!=std::numeric_limits<std::uint64_t>::max()){
            ++replay_epoch;replay_active=owned_=true;replay_rules=rules;
        }
    }
    ReplayScope(const ReplayScope&)=delete;
    ReplayScope& operator=(const ReplayScope&)=delete;
    ~ReplayScope(){if(owned_){replay_active=false;replay_rules={};}}
    bool valid()const{return owned_;}
};
template<class T>
T &isolated_state(T &live,T &shadow,std::uint64_t &shadow_epoch){
    if(!active())return live;
    if(shadow_epoch!=replay_epoch){shadow=live;shadow_epoch=replay_epoch;}
    return shadow;
}
}
