#pragma once
#include "rr64_prediction_context.hpp"
#include "rr64_engine_layout.hpp"
#include <array>
#include <bit>
#include <cmath>

extern "C" int rr64_terrain_streaming_bounded(unsigned char *rdram);

namespace rr64::prediction {
// Inputs selected by rendering before7B8D4. Resource pointers, queue contents,
// allocator state and the three retained cell lists stay in private memory.
struct StreamingInput {
    CpuContext entry;
    std::array<unsigned,6> words{};
    // Cache readiness is a captured presentation input. A historical native
    // streamer must never consult the live renderer's current cache/setting.
    bool bounded_terrain = false;
    static constexpr std::array<unsigned,6> addresses{
        0x800a771c,0x800a7720,0x800a7724,0x8009dbac,0x8009dbcc,0x800a1830};
    bool capture(unsigned char *m,const recomp_context &context){
        StreamingInput value;
        if(!m || context.r4>3 || !value.entry.capture(context))return false;
        for(unsigned i=0;i<addresses.size();++i)
            if(!engine::read_u32(m,addresses[i],value.words[i]))return false;
        value.bounded_terrain = rr64_terrain_streaming_bounded(m) != 0;
        if(!value.valid())return false;
        *this=value;return true;
    }
    bool valid()const{
        for(unsigned i:{0u,3u,4u})
            if(!std::isfinite(std::bit_cast<float>(words[i])))return false;
        return true;
    }
    bool restore(unsigned char *m)const{
        if(!m || !valid())return false;
        for(unsigned i=0;i<addresses.size();++i)engine::write_u32(m,addresses[i],words[i]);
        return true;
    }
};
}
