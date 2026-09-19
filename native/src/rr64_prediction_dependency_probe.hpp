#pragma once
#include "rr64_engine_layout.hpp"
#include <algorithm>
#include <array>
#include <cstdio>
#include <vector>

namespace rr64::prediction {
// Diagnostic candidates, NOT a replay patch list. Observe reads of words that
// differed at entry and still hold their private entry value. This narrows
// missing dependencies to actual native consumers without dumping guest data.
// Native hook reads are outside this generated-code probe, and a write followed
// by restoration can also qualify: a hit alone is not proof of causality.
struct DependencyProbe {
    struct Word {unsigned address=0,value=0;};
    struct Read {unsigned pc=0,address=0,size=0;};
    std::vector<Word> words;
    std::array<Read,128> reads{};
    unsigned stored=0,hits=0;
    bool valid=false;
    void capture(unsigned char *live,unsigned char *replay,unsigned stack){
        *this=DependencyProbe{};
        if(!live || !replay)return;
        for(unsigned off=0;off<engine::kRdramSize;off+=4){
            const unsigned address=0x80000000u+off;
            // Scratch stack values are not durable world dependencies.
            if(stack>=8192 && address>=stack-8192 && address<stack+256)continue;
            unsigned a=0,b=0;engine::read_u32(live,address,a);engine::read_u32(replay,address,b);
            if(a!=b)words.push_back({address,b});
        }
        valid=true;
    }
    void read(unsigned char *m,unsigned address,unsigned size,unsigned pc){
        if(!valid || !m || !size || size>8 || !engine::valid_guest_range(address,size))return;
        address=0x80000000u+(address&0x7fffffu);
        const unsigned first=address&~3u,last=(address+size-1)&~3u;
        auto it=std::lower_bound(words.begin(),words.end(),first,[](const Word &w,unsigned a){return w.address<a;});
        bool hit=false;
        for(;it!=words.end() && it->address<=last;++it){
            unsigned current=0;engine::read_u32(m,it->address,current);
            if(current==it->value){hit=true;break;}
        }
        if(!hit)return;
        ++hits;
        for(unsigned i=0;i<stored;++i)if(reads[i].pc==pc && reads[i].address==address && reads[i].size==size)return;
        if(stored<reads.size())reads[stored++]={pc,address,size};
    }
    void report()const{
        std::fprintf(stderr,"[RR64-REPLAY-DEPENDENCIES] valid=%u entry-words=%zu hits=%u stored=%u\n",valid,words.size(),hits,stored);
        for(unsigned i=0;i<stored;++i)std::fprintf(stderr,"[RR64-REPLAY-DEPENDENCY] pc=%08x address=%08x size=%u\n",reads[i].pc,reads[i].address,reads[i].size);
    }
};
inline thread_local DependencyProbe *dependency_probe=nullptr;
struct DependencyProbeScope {
    DependencyProbe *previous=dependency_probe;
    explicit DependencyProbeScope(DependencyProbe &probe){dependency_probe=&probe;}
    ~DependencyProbeScope(){dependency_probe=previous;}
    DependencyProbeScope(const DependencyProbeScope&)=delete;
};
inline void probe_dependency_read(unsigned char *m,unsigned address,unsigned size,unsigned pc){
    if(dependency_probe)dependency_probe->read(m,address,size,pc);
}
}
