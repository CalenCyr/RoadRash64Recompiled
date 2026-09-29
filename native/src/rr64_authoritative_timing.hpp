#pragma once
#include <array>
#include <bit>
#include <cmath>
#include <cstdint>

namespace rr64::authority {
// Exact native values at the completed outer update, after the stock routine
// restores its temporarily halved delta. Derived values are retained verbatim:
// recomputing them from delta would not reproduce the native two-pass path.
// This metadata does not align client clocks or execute prediction by itself.
struct NativeTiming {
    // delta, delta squared, scaled delta, inverse delta, game clock, race clock
    std::array<std::uint32_t,6> bits{};
    std::uint32_t substeps=0;
    bool operator==(const NativeTiming&)const=default;
};
inline bool valid_timing(const NativeTiming &timing){
    if(timing.substeps!=1 && timing.substeps!=2)return false;
    for(unsigned i=0;i<timing.bits.size();++i){
        const float value=std::bit_cast<float>(timing.bits[i]);
        if(!std::isfinite(value) || (i<4 && value<=0))return false;
    }
    return true;
}
// Duration belongs to native simulation, not renderer FPS or packet arrival.
// Round once per input; host and guest then share the same integer timeline.
inline std::uint32_t duration_us(const NativeTiming &timing){
    if(!valid_timing(timing))return 0;
    const double us=double(std::bit_cast<float>(timing.bits[0]))*1000000.0;
    if(us<1.0 || us>250000.0)return 0;
    return static_cast<std::uint32_t>(std::llround(us));
}
}
