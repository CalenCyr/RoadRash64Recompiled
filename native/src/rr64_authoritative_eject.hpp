#pragma once
#include "rr64_engine_layout.hpp"
#include "rr64_prediction_cop_state.hpp"
#include <cmath>

namespace rr64::authority {
// Resolve canonical host actors, not the four physical controller ports.
// Transition invokes the unchanged native eject routine supplied by the caller.
template<class Transition>
bool eject_step(unsigned char *m,unsigned slot,bool requested,
                prediction::ManualEjectState &protections,Transition transition) {
    using namespace engine;
    if(!m || slot>=protections.size())return false;
    const unsigned actor=0x800d8570u+slot*0x118u;
    unsigned bike=0,rider=0,back=0;
    auto &protection=protections[slot];
    if(!read_u32(m,actor+0xe0,bike) || !valid_guest_range(bike,bike::stride) ||
       !read_u32(m,actor+0xe4,rider) || !valid_guest_range(rider,rider::stride) ||
       !read_u32(m,bike+bike::rider_pointer,back) || back!=rider ||
       !read_u32(m,rider+rider::bike_pointer,back) || back!=bike){
        protection={};return false;
    }
    float health=0,capacity=0;
    std::uint16_t lockout=0,attached=0,mounted=0,ejected=0;
    if(!read_float(m,bike+bike::durability_current,health) || !std::isfinite(health) ||
       !read_float(m,bike+bike::durability_capacity,capacity) || !std::isfinite(capacity) ||
       !read_u16(m,bike+bike::drive_control_lockout,lockout))return false;
    if(protection.active){
        if(protection.bike!=bike)protection={};
        else {
            if(health<protection.durability){health=protection.durability;write_float(m,bike+bike::durability_current,health);}
            if(bike_accepts_drive_control(lockout))protection={};
        }
    }
    if(!requested)return true;
    if(!read_u16(m,bike+bike::rider_attached,attached) ||
       !read_u16(m,rider+rider::bike_attached,mounted) ||
       !read_u16(m,rider+rider::ejected,ejected))return false;
    // An edge while already falling is consumed, not deferred until remount.
    if(!rider_can_manual_eject(attached,mounted,ejected))return true;
    if(health<0 || capacity<=0 || health>capacity)return false;
    if(!transition(bike))return false;
    protection={bike,health,true};
    return true;
}
}
