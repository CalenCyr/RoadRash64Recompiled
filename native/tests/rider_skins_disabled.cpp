#include "rr64_rider_skin_menu.hpp"
// Campaign fixtures preserve the optional-mod-disabled branch. Enabled skin
// selection and persistence are exercised by RR64RiderSkinMenuSmoke.
extern "C" int rr64_rider_skin_cycle(unsigned char*,unsigned,int raw){return raw;}
extern "C" void rr64_rider_skin_restore(unsigned char*,unsigned,unsigned){}
extern "C" void rr64_rider_skin_remember(unsigned char*,unsigned){}
extern "C" void rr64_rider_skin_campaign_load(unsigned char*,unsigned){}
extern "C" void rr64_rider_skin_selection_hint(unsigned char*,void*){}
