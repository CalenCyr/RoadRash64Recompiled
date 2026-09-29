#pragma once
#include "rr64_prediction_diagnostics.hpp"
#include "rr64_engine_layout.hpp"
#include <cmath>

namespace rr64::prediction {
// Shared flag interpretation for live/private/authoritative actor snapshots.
// Counts and native camera modes remain values, not inputs to a camera repair.
inline unsigned camera_state_flags(unsigned bike_attached,unsigned rider_attached,
                                   unsigned ejected,unsigned drive_locked) noexcept {
    return unsigned(bike_attached!=0) | (unsigned(rider_attached!=0)<<1) |
           (unsigned(ejected!=0)<<2) | (unsigned(drive_locked!=0)<<3);
}
inline CameraDiagnostic capture_camera(unsigned char *m,unsigned native_view) noexcept {
    CameraDiagnostic result;
    if(!m || native_view>=4 ||
       !engine::read_u32(m,0x800a4fa0+native_view*4,result.mode) ||
       !engine::read_u32(m,0x800a657c+native_view*4,result.mapped_actor) || result.mapped_actor>=14)return result;
    const unsigned actor=0x800d8570+result.mapped_actor*0x118;
    unsigned bike=0,rider=0,owner=0;
    std::uint16_t active=0,attached=0,body_attached=0,ejected=0,locked=0;
    if(!engine::read_u16(m,actor+0x24,active) || !active ||
       !engine::read_u32(m,actor+0xe0,bike) || (bike&3) || !engine::valid_guest_range(bike,engine::bike::stride) ||
       !engine::read_u32(m,actor+0xe4,rider) || (rider&3) || !engine::valid_guest_range(rider,engine::rider::stride) ||
       !engine::read_u32(m,bike+engine::bike::rider_pointer,owner) || owner!=rider ||
       !engine::read_u32(m,rider+engine::rider::bike_pointer,owner) || owner!=bike ||
       !engine::read_u32(m,bike+4,owner) || owner!=actor ||
       !engine::read_u32(m,rider+4,owner) || owner!=actor ||
       !engine::read_u16(m,bike+engine::bike::rider_attached,attached) ||
       !engine::read_u16(m,rider+engine::rider::bike_attached,body_attached) ||
       !engine::read_u16(m,rider+engine::rider::ejected,ejected) ||
       !engine::read_u16(m,bike+engine::bike::drive_control_lockout,locked))return result;
    result.state_flags=camera_state_flags(attached,body_attached,ejected,locked);
    bool finite=true;
    for(unsigned i=0;i<4;++i) {
        float value=0;
        if(!engine::read_float(m,bike+0x244+i*4,value) || !std::isfinite(value)){finite=false;continue;}
        result.rotation[i]=value;
    }
    for(unsigned i=0;i<3;++i) {
        float value=0;
        if(!engine::read_float(m,bike+0x7dc+i*4,value) || !std::isfinite(value)){finite=false;continue;}
        result.anchor[i]=value;
    }
    result.valid=finite;
    return result;
}
template<class RiderState>
inline CameraAuthorityDiagnostic capture_camera_authority(const RiderState &rider) noexcept {
    CameraAuthorityDiagnostic result;
    if(!rider.active || !rider.root.valid)return result;
    const auto &root=rider.root;
    result.state_flags=camera_state_flags(root.bike_attached,root.rider_attached,root.ejected,root.drive_lockout);
    bool finite=true;
    for(unsigned i=0;i<4;++i) {
        if(!std::isfinite(root.bike_rotation[i])){finite=false;continue;}
        result.rotation[i]=root.bike_rotation[i];
    }
    result.valid=finite;
    return result;
}
}
