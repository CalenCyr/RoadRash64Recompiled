#pragma once
#include "rr64_highlight_recording.hpp"

namespace rr64::highlights {
// Reads the actual attack graph after its native animation producer. Empty,
// hidden, replaced or malformed models yield an absent weapon, never a guess
// based on the inventory selection.
bool capture_weapon(unsigned char *, unsigned rider, WeaponPose &) noexcept;
// Only the owned scratch graph is written. Live rider/equipment/pose bytes are
// never overridden. The native renderer copies transforms into its usual frame
// buffers before this call returns.
bool draw_weapon(unsigned char *, void *context, const WeaponPose &,
                 const Vec3 &anchor, const Quaternion &, const Vec3 &camera,
                 unsigned source_bank, float model_scale = 1.f,
                 const Vec3 &pivot = {}) noexcept;
void reset_weapon_scratch() noexcept;
}
