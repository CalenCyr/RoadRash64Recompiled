#pragma once
#ifdef __cplusplus
extern "C" {
#endif
unsigned rr64_campaign_ending_exit(unsigned char *rdram, unsigned original_mode);
void rr64_campaign_results_begin(unsigned char *rdram);
int rr64_campaign_repeat_completion(unsigned char *rdram);
int rr64_campaign_finish_menu(unsigned char *rdram);
void rr64_campaign_restore_unlocks(unsigned char *rdram, unsigned record);
unsigned rr64_campaign_shop_promotion(unsigned char *rdram, unsigned native_ready);
int rr64_campaign_bonus_active(unsigned char *rdram);
unsigned rr64_campaign_table_address(unsigned char *rdram, unsigned native_address);
void rr64_campaign_bonus_menu(unsigned char *rdram);
void rr64_campaign_bonus_descriptor(unsigned char *rdram);
unsigned rr64_campaign_bonus_route(unsigned char *rdram, unsigned native_route);
void rr64_campaign_bonus_label(unsigned char *rdram, unsigned buffer);
#ifdef __cplusplus
}
#endif
