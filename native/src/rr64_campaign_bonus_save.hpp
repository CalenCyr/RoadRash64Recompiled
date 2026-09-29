#pragma once

#ifdef __cplusplus
extern "C" {
#endif

void rr64_campaign_begin_bonus(unsigned char *rdram);
void rr64_campaign_bonus_new_profile(unsigned char *rdram, unsigned record);
unsigned rr64_campaign_qualification_word(unsigned char *rdram, unsigned address,
                                          unsigned native_word);
unsigned rr64_campaign_qualification_store(unsigned char *rdram, unsigned address,
                                           unsigned new_word);
void rr64_campaign_bonus_scan(unsigned char *rdram);
void rr64_campaign_bonus_capture(unsigned char *rdram, unsigned record);
int rr64_campaign_bonus_record(unsigned char *rdram, unsigned record);
void rr64_campaign_bonus_load(unsigned char *rdram, unsigned slot, unsigned destination);
void rr64_campaign_bonus_save(unsigned char *rdram, unsigned source, unsigned buffer);

#ifdef __cplusplus
}
#endif
