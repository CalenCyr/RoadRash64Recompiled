#pragma once
#ifdef __cplusplus
#include "rr64_course_hazard_state.hpp"
#include "rr64_mk64_item_state.hpp"
namespace rr64::highlights {
void reset() noexcept;
// Read-only render override, never installed in the live hazard simulation.
const netplay::CourseHazardState *render_hazards() noexcept;
const mk64_items::Snapshot *render_items() noexcept;
// Recorded world anchors only during playback drawing; never live physics.
bool render_rider_anchors(unsigned canonical_slot, std::array<float, 3> &bike,
                          std::array<float, 3> &rider, bool &attached,
                          std::array<float, 3> &bike_origin) noexcept;
}
extern "C" {
#endif
int rr64_highlights_wait(unsigned char *, unsigned mode);
int rr64_highlights_frame_gate(unsigned char *, unsigned mode, void *context);
int rr64_highlights_block_dispatch(void);
int rr64_highlights_presenting(void);
void rr64_highlights_capture(unsigned char *);
// Read-only producer request; leaves the user's live drawing preference intact.
int rr64_highlights_needs_detail(unsigned char *);
// Called only after complete private pair publication, before scratch reuse.
void rr64_highlights_capture_detail(unsigned char *live, unsigned char *prepared,
                                    unsigned bike_node, unsigned rider_node);
unsigned rr64_highlights_root_source(unsigned char *, unsigned node, unsigned record, unsigned original);
int rr64_highlights_scale_root_matrix(unsigned char *, unsigned node, unsigned record, unsigned matrix);
void rr64_highlights_crash(unsigned char *, unsigned bike);
// Successful native relocation path; race rank is not a continuity counter.
void rr64_highlights_recovery(unsigned char *, unsigned actor);
void rr64_highlights_draw_begin(unsigned char *);
void rr64_highlights_draw_end(unsigned char *, void *);
int rr64_highlights_actor(unsigned char *, unsigned node, unsigned tier);
unsigned rr64_highlights_lod(unsigned char *, unsigned node, unsigned original);
unsigned rr64_highlights_viewport_inset(unsigned original);
void rr64_highlights_actor_end(void);
unsigned rr64_highlights_hidden(unsigned char *, unsigned node, unsigned original);
void rr64_highlights_camera_origin(unsigned char *);
// After terrain/projection setup and before the native scene actor pass.
void rr64_highlights_traffic_draw(unsigned char *);
int rr64_highlights_weapon_draw(unsigned char *, void *, unsigned node);
void rr64_highlights_weapon_matrix(unsigned char *, unsigned record, unsigned matrix);
#ifdef __cplusplus
}
#endif
