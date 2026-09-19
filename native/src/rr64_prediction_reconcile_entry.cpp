#include "rr64_prediction_reconcile.hpp"
#include "librecomp/game.hpp"
#include <cstdio>
extern "C" void rr64_prediction_reconcile_step(unsigned char *m){
    // A temporarily unavailable resource baseline is retried against a newer
    // host frame. Unacknowledged inputs remain bounded by the history channel.
    static thread_local unsigned deferred=0;
    if(rr64::prediction::reconcile_movement(m,recomp::get_rom()))deferred=0;
    else if(++deferred==1 || deferred%60==0)
        std::fprintf(stderr,"[RR64-NET] reconciliation deferred updates=%u\n",deferred);
}
