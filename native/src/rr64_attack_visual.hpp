#pragma once
#include <array>
#include <cmath>
#include <cstdint>

namespace rr64 {
// Pointer-free inputs to the stock rider/weapon pose builders. This is visual
// authority only; it must not replace the simulation's attack/hit controller.
struct AttackVisual {
    std::uint16_t valid=0, descriptor=0, equipment=1, weapon_visible=0;
    // +558, +55C, +54C, +5C4, +5C8, +5CC (5AEE0/5B63C).
    std::array<float,6> clocks{};
};
inline bool valid_attack_visual(const AttackVisual &v) {
    if(v.valid>1 || v.descriptor>=48 || v.equipment<1 || v.equipment>14 || v.weapon_visible>1) return false;
    for(float x:v.clocks) if(!std::isfinite(x)) return false;
    return true;
}
}
