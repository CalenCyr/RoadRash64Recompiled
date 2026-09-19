#pragma once
#include "rr64_attack_visual.hpp"
#include "rr64_engine_layout.hpp"

namespace rr64::attack_visual {
constexpr unsigned descriptor_base=0x800a5584u, descriptor_stride=76;
constexpr std::array<unsigned,6> clock_offsets{0x558,0x55c,0x54c,0x5c4,0x5c8,0x5cc};
inline AttackVisual capture(unsigned char *m,unsigned rider) {
    AttackVisual v{};
    unsigned descriptor=0,equipment=0;
    std::uint16_t marker=0;
    if(!engine::valid_guest_range(rider,engine::rider::stride)) return v;
    for(unsigned i=0;i<v.clocks.size();++i)
        if(!engine::read_float(m,rider+clock_offsets[i],v.clocks[i]) || !std::isfinite(v.clocks[i])) return {};
    // An idle snapshot must clear a stale remote attack even without a live
    // descriptor. Never turn an arbitrary guest pointer into a wire address.
    if(v.clocks[0]!=0) {
        if(!engine::read_u32(m,rider+0x568,descriptor) || descriptor<descriptor_base ||
           (descriptor-descriptor_base)%descriptor_stride ||
           (descriptor-descriptor_base)/descriptor_stride>=48 ||
           !engine::read_u32(m,rider+0x564,equipment) || equipment<1 || equipment>14 ||
           !engine::read_u16(m,rider+0x528,marker)) return {};
        v.descriptor=static_cast<std::uint16_t>((descriptor-descriptor_base)/descriptor_stride);
        v.equipment=static_cast<std::uint16_t>(equipment);
        v.weapon_visible=static_cast<std::int16_t>(marker)>=0;
    }
    v.valid=1;
    return v;
}
struct Saved {
    unsigned rider=0;
    std::array<std::uint32_t,8> words{};
    std::uint16_t marker=0;
    bool apply(unsigned char *m,unsigned r,const AttackVisual &v) {
        if(!v.valid || !valid_attack_visual(v) || !engine::valid_guest_range(r,engine::rider::stride)) return false;
        for(unsigned i=0;i<6;++i) engine::read_u32(m,r+clock_offsets[i],words[i]);
        engine::read_u32(m,r+0x568,words[6]);engine::read_u32(m,r+0x564,words[7]);
        engine::read_u16(m,r+0x528,marker);
        rider=r;
        for(unsigned i=0;i<6;++i) engine::write_float(m,r+clock_offsets[i],v.clocks[i]);
        engine::write_u32(m,r+0x568,descriptor_base+v.descriptor*descriptor_stride);
        engine::write_u32(m,r+0x564,v.equipment);
        // The builder tests only the sign; model lookup uses equipment. Keep
        // resource creation/destruction in the stock local scene lifecycle.
        engine::write_u16(m,r+0x528,v.weapon_visible ? 0 : 0xffff);
        return true;
    }
    void restore(unsigned char *m) {
        if(!rider) return;
        for(unsigned i=0;i<6;++i) engine::write_u32(m,rider+clock_offsets[i],words[i]);
        engine::write_u32(m,rider+0x568,words[6]);engine::write_u32(m,rider+0x564,words[7]);
        engine::write_u16(m,rider+0x528,marker);rider=0;
    }
};
}
