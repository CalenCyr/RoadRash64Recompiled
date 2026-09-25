#pragma once

// Called only by the original Thrash/multiplayer route selectors and the menu
// presentation pass. A named group never becomes a native level/table index.
#ifdef __cplusplus
extern "C" {
#endif
void rr64_race_pack_menu_begin(unsigned char *memory);
// init_heap can reuse the same host RDRAM address for a new game session.
// Discard guest addresses here; their old heap owns allocation reclamation.
void rr64_race_pack_menu_reset_session(void);
int rr64_race_pack_menu_input(unsigned char *memory, unsigned multiplayer);
// Initializes the existing option table even when a course consumes input.
int rr64_race_pack_menu_options_input(unsigned char *memory, unsigned multiplayer);
void rr64_race_pack_menu_text(unsigned char *memory, unsigned row, unsigned buffer);
void rr64_race_pack_menu_end(unsigned char *memory);
int rr64_race_pack_menu_preview(unsigned char *memory);
void rr64_race_pack_menu_draw(unsigned char *memory);
#ifdef __cplusplus
}
#endif
