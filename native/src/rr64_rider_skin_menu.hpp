#pragma once
#ifdef __cplusplus
extern "C" {
#endif
int rr64_rider_skin_cycle(unsigned char *, unsigned slot, int raw);
void rr64_rider_skin_clear_menu(unsigned char *);
void rr64_rider_skin_race_begin(unsigned char *);
void rr64_rider_skin_spawn(unsigned char *, unsigned canonical);
unsigned rr64_rider_skin_actor_selection(unsigned char *, unsigned node);
unsigned rr64_rider_skin_preview_selection(unsigned char *, unsigned graph);
void rr64_rider_skin_selection_hint(unsigned char *, void *context);
void rr64_rider_skin_restore(unsigned char *, unsigned slot, unsigned new_campaign);
void rr64_rider_skin_remember(unsigned char *, unsigned slot);
void rr64_rider_skin_remember_multiplayer(unsigned char *);
void rr64_rider_skin_campaign_load(unsigned char *, unsigned record);
#ifdef __cplusplus
}
#endif
