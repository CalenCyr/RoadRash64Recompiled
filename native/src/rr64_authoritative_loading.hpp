#pragma once
#include "rr64_engine_layout.hpp"
#include "rr64_online_flow.hpp"

namespace rr64::authority {
// Called at the native dispatcher boundary, after the stock track initializer.
// A menu confirmation is not evidence that the rider allocations exist yet.
inline bool native_roster_loaded(unsigned char *m,unsigned humans,unsigned local,bool mapped) {
    unsigned count=0;
    if(!m || !humans || humans>>14 || local>=14 || !(humans&(1u<<local)) ||
       !engine::read_u32(m,0x800a656c,count) || !count || count>14)return false;
    std::array<unsigned,14> bikes{},riders{};
    for(unsigned slot=0;slot<14;++slot)if(humans&(1u<<slot)) {
        const unsigned guest=online_flow::mapped_slot(slot,local,mapped);
        if(guest>=count)return false;
        const unsigned actor=0x800d8570+guest*0x118;
        unsigned bike=0,rider=0,owner=0;std::uint16_t active=0;
        if(!engine::read_u16(m,actor+0x24,active) || !active ||
           !engine::read_u32(m,actor+0xe0,bike) || !engine::read_u32(m,actor+0xe4,rider) ||
           (bike&3) || (rider&3) || !engine::valid_guest_range(bike,engine::bike::stride) ||
           !engine::valid_guest_range(rider,engine::rider::stride) ||
           !engine::read_u32(m,bike+engine::bike::rider_pointer,owner) || owner!=rider ||
           !engine::read_u32(m,rider+engine::rider::bike_pointer,owner) || owner!=bike)return false;
        for(unsigned previous=0;previous<slot;++previous)
            if(bikes[previous]==bike || riders[previous]==rider)return false;
        bikes[slot]=bike;riders[slot]=rider;
    }
    return true;
}
}
