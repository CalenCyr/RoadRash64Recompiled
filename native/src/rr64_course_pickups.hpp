#pragma once

#ifdef __cplusplus
extern "C" {
#endif
// Suppress every native floor pickup (1..3 power-up/repair, 4..16 weapons)
// on imported courses. Historical hook names are retained. Question boxes
// and combat theft call inventory directly and do not pass through this policy.
int rr64_course_loose_weapon(unsigned type);
int rr64_course_loose_weapon_record(unsigned char *rdram, unsigned record);
#ifdef __cplusplus
}
#endif
