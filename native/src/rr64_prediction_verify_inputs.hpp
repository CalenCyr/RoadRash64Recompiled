#pragma once
#include "rr64_engine_layout.hpp"
#include <array>
#include <cstdio>
#include <vector>

namespace rr64::prediction {
// Private diagnostics: retain only offsets of differing entry words, never
// their contents. This exposes state omitted by the named movement schema.
// Differences can be legitimate rendering state; this is not a correction map.
struct EntryDifferences {
    unsigned slot=0,words=0;
    const char *region="";
    bool valid=false;
    std::array<std::uint64_t,9> mask{};
};
inline EntryDifferences compare_entry_region(unsigned char *live,unsigned char *replay,
        unsigned address,unsigned size,unsigned slot,const char *region){
    EntryDifferences out;out.slot=slot;out.region=region;
    if(!live || !replay || (address&3) || (size&3) || !size ||
       size>out.mask.size()*64*4 || !engine::valid_guest_range(address,size))return out;
    out.valid=true;
    for(unsigned offset=0;offset<size;offset+=4){
        unsigned a=0,b=0;
        if(!engine::read_u32(live,address+offset,a) || !engine::read_u32(replay,address+offset,b)){
            out.valid=false;return out;
        }
        if(a!=b){++out.words;out.mask[offset/256]|=std::uint64_t(1)<<((offset/4)%64);}
    }
    return out;
}
inline std::vector<EntryDifferences> compare_actor_entries(unsigned char *live,unsigned char *replay,unsigned count){
    std::vector<EntryDifferences> out;
    if(count>14)return out;
    out.reserve(count*4);
    for(unsigned slot=0;slot<count;++slot){
        const unsigned actor=0x800d8570+slot*0x118;
        out.push_back(compare_entry_region(live,replay,actor,0x118,slot,"actor"));
        for(unsigned part=0;part<3;++part){
            unsigned a=0,b=0;
            const unsigned offset=0xe0+4*part;
            const char *name=part==0?"bike":part==1?"rider":"stats";
            if(!engine::read_u32(live,actor+offset,a) || !engine::read_u32(replay,actor+offset,b) || a!=b){
                EntryDifferences mismatch;mismatch.slot=slot;mismatch.region=name;out.push_back(mismatch);continue;
            }
            out.push_back(compare_entry_region(live,replay,a,
                part==0?engine::bike::stride:part==1?engine::rider::stride:0x64,slot,name));
        }
    }
    return out;
}
inline void report_actor_entries(const std::vector<EntryDifferences> &entries){
    for(const auto &entry:entries){
        if(entry.valid && !entry.words)continue;
        std::fprintf(stderr,"[RR64-REPLAY-ENTRY] slot=%u region=%s valid=%u different-words=%u word-masks=",
            entry.slot,entry.region,entry.valid,entry.words);
        // Block n, bit b denotes byte offset n*256+b*4; no guest values emitted.
        for(const auto mask:entry.mask)std::fprintf(stderr,"%016llx,",static_cast<unsigned long long>(mask));
        std::fputc('\n',stderr);
    }
}
}
