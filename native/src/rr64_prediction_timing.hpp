#pragma once
#include "rr64_authoritative_timing.hpp"
#include "rr64_engine_layout.hpp"
#include <bit>
#include <cmath>

namespace rr64::prediction {
// Integer scheduling phase, separate from elapsed-time floats and network
// timing. Native 4EA74/4EA84 advance these outside the replay prefix; AI reads
// them (including 4EC24's modulo-64 gate). Retain each historical entry exactly.
// The third word is the unscaled order/notification clock read at6E84C.
// Preserve its float bits separately from the physics delta (which6AFFC may
// halve). Full race-order replay must not inherit the previous update's clock.
using UpdateCounters=std::array<std::uint32_t,3>;
inline bool capture_update_counters(unsigned char *m,UpdateCounters &out){
    UpdateCounters value{};
    if(!m || !engine::read_u32(m,0x800a182c,value[0]) || !engine::read_u32(m,0x800a1830,value[1]) ||
       !engine::read_u32(m,0x800a1818,value[2]) || !std::isfinite(std::bit_cast<float>(value[2])))return false;
    out=value;return true;
}
inline bool restore_update_counters(unsigned char *m,const UpdateCounters &value){
    if(!m || !std::isfinite(std::bit_cast<float>(value[2])))return false;
    engine::write_u32(m,0x800a182c,value[0]);engine::write_u32(m,0x800a1830,value[1]);
    engine::write_u32(m,0x800a1818,value[2]);return true;
}
// Capture before6AFFC halves delta for its optional second pass. Store the
// derived values verbatim: recomputing them introduces a different trajectory.
inline constexpr std::array<unsigned,6> timing_addresses{
    0x8009cba8,0x8009cbac,0x8009cbb0,0x8009cbb4,0x800a1820,0x800d7670};
inline bool capture_timing(unsigned char *m,authority::NativeTiming &out){
    authority::NativeTiming value{};std::uint16_t split=0;
    for(unsigned i=0;i<timing_addresses.size();++i)
        if(!engine::read_u32(m,timing_addresses[i],value.bits[i]))return false;
    if(!engine::read_u16(m,0x800a659a,split))return false;
    value.substeps=split?2:1;
    if(!authority::valid_timing(value))return false;
    out=value;return true;
}
// This is for a disposable historical image only, never the live game clock.
inline bool restore_timing(unsigned char *m,const authority::NativeTiming &value){
    if(!m || !authority::valid_timing(value))return false;
    for(unsigned i=0;i<timing_addresses.size();++i)
        engine::write_u32(m,timing_addresses[i],value.bits[i]);
    engine::write_u16(m,0x800a659a,value.substeps==2?1:0);
    return true;
}
}
