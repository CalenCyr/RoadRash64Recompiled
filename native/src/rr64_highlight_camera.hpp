#pragma once

#ifdef __cplusplus
#include <array>
namespace rr64::highlight_camera {
using Vec3 = std::array<float, 3>;
struct View {
    Vec3 eye{}, target{}, up{0, 0, 1};
};
// Recorded physical origin and heading, in Road Rash world units (Z is up).
// Produces three restrained alternate angles; no actor state is consulted.
bool make_view(const Vec3& subject, const Vec3& forward, unsigned shot, View& output) noexcept;
// The results-playback owner brackets one complete 6A638 draw. These functions
// preserve camera inputs/derived globals, not actor poses or result state.
bool begin(unsigned char* memory, const View& view) noexcept;
void end(unsigned char* memory) noexcept;
bool active() noexcept;
}
extern "C" {
#endif
// 5D9A4 before 5E60C: after ordinary camera calculation, before actor distance,
// root preparation, sector rebase, matrices and terrain streaming consume it.
void rr64_highlight_camera_apply(unsigned char* memory);
#ifdef __cplusplus
}
#endif
