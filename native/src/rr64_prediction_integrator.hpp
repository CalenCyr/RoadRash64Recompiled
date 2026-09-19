#pragma once
#include "rr64_engine_layout.hpp"
#include "rr64_integrator_state.hpp"

namespace rr64::prediction {
// Value-only state used by the original 34594 translation and 348D8 rotation
// integrators. This is deliberately NOT a serialization of the native object:
// collision links, models, callbacks and allocator ownership stay local.
// Mass/inertia at +0/+18..20 are setup data and are not overwritten.
inline bool capture_integrator(unsigned char *memory,unsigned body,IntegratorState &out) {
    if(!memory || (body&3) || !engine::valid_guest_range(body,0x17c))return false;
    IntegratorState s;
    auto read=[&](unsigned offset,auto &values){
        for(unsigned i=0;i<values.size();++i)
            if(!engine::read_float(memory,body+offset+i*4,values[i]))return false;
        return true;
    };
    if(!read(0x64,s.translation) || !read(0xf4,s.force) || !read(0x10c,s.rotation) ||
       !engine::read_u16(memory,body+0x60,s.flags) || !valid_integrator(s))return false;
    out=s;return true;
}
// The caller resolves a reciprocal actor/bike/rider pair before using this
// primitive. It accepts only the embedded physics object, never a remote address.
inline bool restore_integrator(unsigned char *memory,unsigned body,const IntegratorState &s) {
    if(!memory || (body&3) || !engine::valid_guest_range(body,0x17c) || !valid_integrator(s))return false;
    auto write=[&](unsigned offset,const auto &values){
        for(unsigned i=0;i<values.size();++i)engine::write_float(memory,body+offset+i*4,values[i]);
    };
    write(0x64,s.translation);write(0xf4,s.force);write(0x10c,s.rotation);
    engine::write_u16(memory,body+0x60,s.flags);return true;
}
}
