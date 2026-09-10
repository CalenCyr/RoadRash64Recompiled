#pragma once
#include <cmath>
#include <utility>

namespace RR64MenuBorders {
inline std::pair<int, int> bounds(bool menu, float x, float width, float scaleX, float scaleY) {
    float inset = 0.0f;
    if (menu && scaleX > scaleY && scaleY > 0.0f) {
        inset = width * 0.5f * (1.0f - scaleY / scaleX);
    }
    return {int(std::lround(x + inset)), int(std::lround(x + width - inset))};
}
}
