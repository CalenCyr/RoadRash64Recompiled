// Native immutable world packets use stable IDs partitioned by camera. Their
// translations include camera rebasing, so a small direction reversal is not an
// actor teleport. Preserve the original five-unit small-motion bound before
// AUTO's direction penalty amplifies it. Large/invalid moves keep stock policy.
#pragma once
#include "rt64_transform_group.h"
#include "common/rt64_math.h"
#include <cmath>
namespace RT64::RR64StaticWorldMotion {
inline bool smallTranslation(const TransformGroup &group,
    const hlslpp::float4x4 &previous, const hlslpp::float4x4 &current) {
    const uint32_t id=group.matrixId;
    const bool nativeWorld=(id>=0x52510000u && id<0x52518000u) ||
        (id>=0x52520000u && id<0x52528000u);
    if(!nativeWorld || group.ordering!=G_EX_ORDER_LINEAR || group.positionInterpolation!=G_EX_COMPONENT_AUTO) return false;
    double squared=0;
    for(unsigned i=0;i<3;i++) {
        const float a=previous[3][i],b=current[3][i];
        if(!std::isfinite(a)||!std::isfinite(b)) return false;
        const double delta=double(b)-double(a); squared+=delta*delta;
    }
    return squared<25.0;
}
}
