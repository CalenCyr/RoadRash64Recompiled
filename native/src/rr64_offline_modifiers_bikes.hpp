#pragma once

#ifdef __cplusplus
extern "C" {
#endif

// campaign=1 identifies the two native Big Game shops; campaign=0 is the
// Thrash/local player selector. These queries never grant saved unlock flags.
int rr64_offline_bikes_active(unsigned char* memory, unsigned campaign);
unsigned rr64_offline_bikes_count(unsigned char* memory, unsigned original, unsigned campaign);
unsigned rr64_offline_bikes_entry(unsigned char* memory, unsigned player, unsigned original);
unsigned rr64_offline_bikes_shop_table(unsigned char* memory, unsigned original, unsigned index);

// Clear the borrowed extended-heap address before the guest heap is reset.
void rr64_offline_bikes_reset(void);

#ifdef __cplusplus
}
#endif
