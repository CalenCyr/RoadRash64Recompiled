#pragma once
#include "rr64_engine_layout.hpp"
#include <array>
#include <cmath>

namespace rr64::prediction {
// Disposable image only. Original 6EB1C..6EBEC derives neighbors from the
// sorted progress table. Keep its signed gaps, tie direction and untouched
// boundary fields; clearing those fields would change the native behavior.
inline bool refresh_order_neighbors(unsigned char *m,unsigned count){
    if(!m || !count || count>14)return false;
    constexpr unsigned table=0x800d77a0,actors=0x800d8570;
    std::array<unsigned,14> actor{},stats{},finished{};
    std::array<float,14> progress{};
    std::array<std::uint16_t,14> eligible{},ai{};
    unsigned human_count=0;
    if(!engine::read_u32(m,0x800d7648,human_count))return false;
    float absent=0;unsigned seen=0;
    if(!engine::read_float(m,0x80006bbc,absent) || !std::isfinite(absent))return false;
    // Validate the whole roster before writing any derived fields.
    for(unsigned i=0;i<count;++i){
        if(!engine::read_u32(m,table+i*8,actor[i]) || actor[i]<actors ||
           (actor[i]-actors)%0x118 || (actor[i]-actors)/0x118>=count)return false;
        const unsigned bit=1u<<((actor[i]-actors)/0x118);
        if(seen&bit)return false;
        seen|=bit;
        if(!engine::read_u32(m,actor[i]+0xe8,stats[i]) || (stats[i]&3) ||
           !engine::valid_guest_range(stats[i],0x64) ||
           !engine::read_u32(m,stats[i]+0x50,finished[i]) ||
           !engine::read_u16(m,stats[i]+0x48,eligible[i]) ||
           !engine::read_u16(m,actor[i]+0x26,ai[i]) ||
           !engine::read_float(m,table+i*8+4,progress[i]) || !std::isfinite(progress[i]))return false;
        for(unsigned j=0;j<i;++j)
            if(stats[i]<stats[j]+0x64 && stats[j]<stats[i]+0x64)return false;
    }
    // 6EA3C..6EB18 and 6EFF0..6F034: shared leading eligible racer,
    // leading human and average human rank. AI consumes these alongside links.
    unsigned leader=~0u,human=~0u;float rank_sum=0;
    for(unsigned i=0;i<count;++i){
        if(!finished[i] && eligible[i]){
            if(leader==~0u)leader=i;
            if(!ai[i]){if(human==~0u)human=i;rank_sum+=static_cast<float>(i);}
        }
    }
    if(leader==~0u)leader=0;
    if(human==~0u)human=leader;
    engine::write_u32(m,0x800d7664,leader);
    engine::write_u32(m,0x800d7668,human);
    engine::write_float(m,0x800d766c,static_cast<std::int32_t>(human_count)>0?rank_sum/static_cast<float>(human_count):0);
    for(unsigned i=0;i<count;++i){
        if(finished[i])continue; // 6EA8C skips completed/retired entries.
        const unsigned s=stats[i];
        if(i==0){
            engine::write_float(m,s+0x3c,absent);
            if(count==1){
                engine::write_u32(m,s+0x30,0);
                engine::write_float(m,s+0x34,absent);
                engine::write_u32(m,s+0x28,0);
            }else{
                const float gap=progress[1]-progress[0];
                engine::write_float(m,s+0x3c,gap);
                engine::write_float(m,s+0x34,gap);
                engine::write_u32(m,s+0x30,actor[1]);
                engine::write_u32(m,s+0x28,actor[1]);
            }
        }else{
            const float before=progress[i-1]-progress[i];
            engine::write_float(m,s+0x38,before);
            engine::write_u32(m,s+0x2c,actor[i-1]);
            if(i+1==count){
                engine::write_float(m,s+0x34,before);
                engine::write_u32(m,s+0x28,actor[i-1]);
            }else{
                const float after=progress[i+1]-progress[i];
                engine::write_float(m,s+0x3c,after);
                engine::write_u32(m,s+0x30,actor[i+1]);
                const bool use_before=before < -after;
                engine::write_float(m,s+0x34,use_before?before:after);
                engine::write_u32(m,s+0x28,use_before?actor[i-1]:actor[i+1]);
            }
        }
        engine::write_u32(m,s+0x40,i);
    }
    return true;
}
}
