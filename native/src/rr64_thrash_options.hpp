#pragma once

// Solo Thrash has its own native menu and settings. This bridge extends that
// menu without enabling split-screen, changing Big Game, or sharing its preset
// with local/online multiplayer. All callbacks run on the guest game thread;
// the active query is also safe for native presentation consumers.
#ifdef __cplusplus
extern "C" {
#endif
int rr64_thrash_options_active();
void rr64_thrash_options_mode(unsigned requested_mode);
void rr64_thrash_options_begin(unsigned char *memory);
int rr64_thrash_options_input(unsigned char *memory);
int rr64_thrash_options_navigation(unsigned char *memory);
unsigned rr64_thrash_options_table(unsigned stock);
void rr64_thrash_options_text(unsigned char *memory, unsigned row, unsigned buffer);
void rr64_thrash_options_finish(unsigned char *memory);
#ifdef __cplusplus
}
#endif
