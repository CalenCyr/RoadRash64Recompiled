#pragma once

// Existing isolated actor tests exercise ordinary rendering. Detail-recording
// integration fixtures define RR64_TEST_HIGHLIGHT_DETAIL and supply real probes.
#ifndef RR64_TEST_HIGHLIGHT_DETAIL
extern "C" {
int rr64_highlights_needs_detail(unsigned char *) { return 0; }
void rr64_highlights_capture_detail(unsigned char *, unsigned char *, unsigned, unsigned) {}
void rr64_highlights_recovery(unsigned char *, unsigned) {}
unsigned rr64_highlights_root_source(unsigned char *, unsigned, unsigned, unsigned original) { return original; }
int rr64_highlights_scale_root_matrix(unsigned char *, unsigned, unsigned, unsigned) { return 0; }
}
#endif
