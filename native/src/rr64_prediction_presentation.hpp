#pragma once

#include "rr64_engine_layout.hpp"
#include <array>
#include <chrono>
#include <cmath>

namespace rr64::prediction {

// Corrections own physics immediately. Only the render matrices retain the
// previous mounted pair's position briefly, so accepting a host tick does not
// visibly snap the local bike, body and held weapon independently.
struct MountedPose {
    unsigned char *mapping=nullptr;
    unsigned round=0,actor=0,bike=0,rider=0,model=0,character=0,recovery=0;
    std::array<float,3> position{};
    bool valid=false;
};

inline MountedPose mounted_pose(unsigned char *m,unsigned round,unsigned guest) {
    MountedPose p;
    if(!m || !round || guest>=engine::kMaximumRacers)return p;
    p.mapping=m;p.round=round;p.actor=0x800d8570u+guest*0x118u;
    unsigned owner=0,route=0;
    std::uint16_t active=0,attached=0,body_attached=0,ejected=0,lockout=0,vault=0,busted=0,finished=0;
    if(!engine::read_u16(m,p.actor+0x24,active) || !active ||
       !engine::read_u32(m,p.actor+0xe0,p.bike) || !engine::valid_guest_range(p.bike,engine::bike::stride) ||
       !engine::read_u32(m,p.actor+0xe4,p.rider) || !engine::valid_guest_range(p.rider,engine::rider::stride) ||
       !engine::read_u32(m,p.bike+engine::bike::rider_pointer,owner) || owner!=p.rider ||
       !engine::read_u32(m,p.rider+engine::rider::bike_pointer,owner) || owner!=p.bike ||
       !engine::read_u32(m,p.actor+0x18,p.model) || !engine::read_u32(m,p.actor+0x1c,p.character) ||
       !engine::read_u32(m,p.actor+0xe8,route) || !engine::valid_guest_range(route,0x64) ||
       !engine::read_u32(m,route+0x40,p.recovery) ||
       !engine::read_u16(m,route+0x4c,busted) || busted ||
       !engine::read_u16(m,route+0x52,finished) || finished ||
       !engine::read_u16(m,p.bike+engine::bike::rider_attached,attached) || !attached ||
       !engine::read_u16(m,p.rider+engine::rider::bike_attached,body_attached) || !body_attached ||
       !engine::read_u16(m,p.rider+engine::rider::ejected,ejected) || ejected ||
       !engine::read_u16(m,p.bike+engine::bike::drive_control_lockout,lockout) || lockout ||
       !engine::read_u16(m,p.bike+0x818,vault) || vault)return p;
    for(unsigned axis=0;axis<3;++axis)
        if(!engine::read_float(m,p.bike+engine::bike::body_position+axis*4,p.position[axis]) ||
           !std::isfinite(p.position[axis]))return p;
    p.valid=true;return p;
}

class CorrectionPresentation {
    MountedPose owner_{};
    std::array<float,3> offset_{};
    std::int64_t time_=0;
    static bool same_pair(const MountedPose &a,const MountedPose &b) {
        return a.valid && b.valid && a.mapping==b.mapping && a.round==b.round &&
            a.actor==b.actor && a.bike==b.bike && a.rider==b.rider &&
            a.model==b.model && a.character==b.character && a.recovery==b.recovery;
    }
    static float length_squared(const std::array<float,3> &v) {
        return v[0]*v[0]+v[1]*v[1]+v[2]*v[2];
    }
    void advance(std::int64_t now) {
        if(now<time_ || now-time_>500000){reset();return;}
        const float weight=std::exp(-static_cast<float>(now-time_)/80000.0f);
        for(auto &v:offset_)v*=weight;
        time_=now;
    }
public:
    void reset(){owner_={};offset_={};time_=0;}
    // Call only after the authority/history transaction succeeds. Rejected
    // tickets must not change either simulation or its visible correction.
    void corrected(const MountedPose &before,const MountedPose &after,std::int64_t now) {
        if(!same_pair(before,after)){reset();return;}
        if(!same_pair(owner_,before)){reset();time_=now;}
        advance(now);
        std::array<float,3> delta{};
        for(unsigned i=0;i<3;++i)delta[i]=before.position[i]-after.position[i];
        // Teleports/respawns and excessive error are shown immediately. Never
        // drag a visible rider across a large correction or through a crash.
        if(!std::isfinite(length_squared(delta)) || length_squared(delta)>256.0f){reset();return;}
        for(unsigned i=0;i<3;++i)offset_[i]+=delta[i];
        if(length_squared(offset_)>256.0f){reset();return;}
        owner_=after;time_=now;
    }
    std::array<float,3> sample(const MountedPose &pose,std::int64_t now) {
        if(!same_pair(owner_,pose)){reset();return {};}
        advance(now);return offset_;
    }
};

inline std::int64_t presentation_time_us() {
    return std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
}
// Both reconciliation and pose preparation run on the game-update thread.
// The renderer receives the existing immutable per-frame offset publication.
inline thread_local CorrectionPresentation local_correction_presentation;

} // namespace rr64::prediction
