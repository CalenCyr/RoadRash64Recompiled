#pragma once

#ifdef __cplusplus
extern "C" {
#endif

// Race construction only: choose an actual bike model independently of the
// player's family, retaining a native donor profile with matching performance.
unsigned rr64_ai_bike_profile(unsigned char *rdram, void *context, unsigned profile);

#ifdef __cplusplus
}
#endif
