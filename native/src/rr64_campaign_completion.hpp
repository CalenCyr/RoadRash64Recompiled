#pragma once
#ifdef __cplusplus
extern "C" {
#endif
unsigned rr64_campaign_ending_exit(unsigned char *rdram, unsigned original_mode);
int rr64_campaign_finish_menu(unsigned char *rdram);
void rr64_campaign_restore_unlocks(unsigned char *rdram, unsigned record);
#ifdef __cplusplus
}
#endif
