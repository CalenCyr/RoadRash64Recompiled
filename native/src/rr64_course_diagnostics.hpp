#pragma once
#ifdef __cplusplus
extern "C" {
#endif
// Opt-in, bounded first-draw evidence for a private imported-course test.
void rr64_course_diagnostics_draw(unsigned char *memory);
void rr64_course_diagnostics_begin(void);
void rr64_course_diagnostics_observe(unsigned char *memory, unsigned record);
#ifdef __cplusplus
}
#endif
