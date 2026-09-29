#pragma once
#include "rr64_prediction_cop_state.hpp"
#include "rr64_engine_layout.hpp"
#include "rr64_authoritative_outcome.hpp"
#include <cmath>

namespace rr64::prediction {
// A consumed manual-eject edge must not fire twice, but its native protection
// must survive the remaining tail. Reuse only the historical edge's protection
// when the already-corrected host pair confirms that ejection still exists.
// This helper changes one native shadow value; it never writes guest memory.
inline ManualEjectProtection entered_eject_protection(unsigned char *m,unsigned actor,
    const authority::Outcome &outcome,const ManualEjectProtection &recorded) {
    using namespace engine;
    ManualEjectProtection none{};
    if(!m || !recorded.active || !std::isfinite(recorded.durability) || recorded.durability<0 ||
       actor<0x800d8570u || (actor-0x800d8570u)%0x118u || (actor-0x800d8570u)/0x118u>=14 ||
       !outcome.valid || !authority::valid_outcome(outcome) || outcome.busted || outcome.finished)return none;
    unsigned bike=0,rider=0,back=0;
    std::uint16_t active=0,lockout=0,ejected=0;
    float health=0,capacity=0;
    if(!read_u16(m,actor+0x24,active) || !active ||
       !read_u32(m,actor+0xe0,bike) || bike!=recorded.bike ||
       !read_u32(m,actor+0xe4,rider) || (bike&3u) || (rider&3u) ||
       !valid_guest_range(bike,bike::stride) || !valid_guest_range(rider,rider::stride) ||
       !read_u32(m,bike+bike::rider_pointer,back) || back!=rider ||
       !read_u32(m,rider+rider::bike_pointer,back) || back!=bike ||
       !read_u16(m,bike+bike::drive_control_lockout,lockout) || bike_accepts_drive_control(lockout) ||
       !read_u16(m,rider+rider::ejected,ejected) || !ejected ||
       !read_float(m,bike+bike::durability_current,health) || !std::isfinite(health) ||
       !read_float(m,bike+bike::durability_capacity,capacity) || !std::isfinite(capacity) ||
       capacity<=0 || health<0 || health>capacity || recorded.durability>health)return none;
    return recorded;
}
}
