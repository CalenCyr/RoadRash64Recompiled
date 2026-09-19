#pragma once
#include "rr64_prediction_replay.hpp"
#include "rr64_netplay.hpp"
namespace rr64::prediction {
inline SessionRules capture_session_rules(const netplay::Status& status){
    return {status.active,status.connected,status.authoritative,status.is_host,
        status.replicated_riders,status.local_slot,status.authority_humans};
}
// Only native gameplay rules use this view. Transport-facing hooks continue to
// see an inactive session during replay and cannot send or consume live events.
inline netplay::Status status_for_rules(){
    if(!active())return netplay::get_status();
    netplay::Status status;
    status.active=replay_rules.active;status.connected=replay_rules.connected;
    status.authoritative=replay_rules.authoritative;status.is_host=replay_rules.is_host;
    status.replicated_riders=replay_rules.replicated_riders;
    status.local_slot=replay_rules.local_slot;status.authority_humans=replay_rules.humans;
    return status;
}
}
