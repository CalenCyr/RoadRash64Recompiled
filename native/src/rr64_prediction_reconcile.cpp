#include "rr64_prediction_reconcile.hpp"
#include "rr64_prediction_traffic.hpp"
namespace rr64::prediction {
namespace {
bool current_native(FrameOutput &s){
    return capture_cop_rules(s.rules) && capture_cop_posts(s.posts) && capture_manual_eject(s.eject);
}
struct Commit {
    const HistoricalReplay *plan;
    ReplayedHistory *result;
    const FrameOutput *expected;
    std::uint64_t ticket;
};
bool consume_ticket(void *arg) noexcept {
    return netplay::authority_commit_replay(static_cast<Commit*>(arg)->ticket);
}
bool commit_history(void *arg) noexcept {
    auto &c=*static_cast<Commit*>(arg);
    FrameOutput now;
    if(!current_native(now) || now.rules!=c.expected->rules || now.posts!=c.expected->posts ||
       now.eject!=c.expected->eject)return false;
    return commit_history_replay(*c.plan,*c.result,consume_ticket,arg);
}
}
bool reconcile_movement(unsigned char *m,std::span<const unsigned char> rom){
    if(active() || !m || rom.empty())return false;
    const auto s=netplay::get_status();
    if(!s.active || !s.connected || !s.authoritative || s.is_host || s.host_disconnected ||
       s.phase!=netplay::Phase::Race || s.local_slot>=14)return false;
    netplay::AuthorityReplayPlan network;
    if(!netplay::authority_prepare_replay(network))return true; // no new completed host tick
    const auto &stamp=network.frame.stamp;
    const unsigned local=online_flow::mapped_slot(s.local_slot,s.local_slot,s.replicated_riders);
    try {
        HistoricalReplay plan;
        ReplayAdmission admission;
        if(!prepare_history_replay(stamp.round,stamp.acknowledged[s.local_slot],
            std::span(network.commands.data(),network.count),0x800d8570+local*0x118,plan,&admission))
            return admission==ReplayAdmission::ResourcePending; // no ticket/ACK consumed
        if(!plan.steps.empty()){
            const auto traffic=correct_traffic_baseline(plan.memory.data(),
                {stamp.round,stamp.tick,network.frame.traffic});
            if(traffic!=TrafficAdmission::Ready)return traffic==TrafficAdmission::TopologyPending;
        }
        MovementCorrection baseline;
        if(!baseline.prepare(plan.memory.data(),network.frame,s.local_slot,s.replicated_riders,true) ||
           !baseline.commit([](void*) noexcept {return true;},nullptr))return false;
        ReplayedHistory result;
        if(!evaluate_history_replay(plan,rom,result,replay_native_order,replay_native_frame,replay_native_streaming))return false;
        netplay::AuthorityFrame corrected;
        if(!capture_prediction_movement(result.final_memory.data(),stamp,s.local_slot,
                                       s.authority_humans,s.replicated_riders,corrected))return false;
        // Remote actors are rendered from the newest host frame. Only local
        // prediction can replace the local movement produced by this replay.
        for(unsigned slot=0;slot<14;++slot)if(slot!=s.local_slot){
            corrected.riders[slot].active=false;
            // Retired actors can carry outcomes without a visible pose. Clear
            // both channels so an older local replay cannot rewind their results.
            corrected.outcomes[slot].valid=0;
        }
        if(!corrected.riders[s.local_slot].active)return false;
        MovementCorrection live;
        FrameOutput expected;
        if(!current_native(expected) || !live.prepare(m,corrected,s.local_slot,s.replicated_riders,true))return false;
        // The journal carries every actor for private execution, but only the
        // local guest slot owns predicted native state. Global race rules and
        // remote sirens/eject protection may have advanced since this baseline.
        auto native=expected;
        native.posts[local]=result.native.back().posts[local];
        native.eject[local]=result.native.back().eject[local];
        if(!std::isfinite(native.rules.win_started) || native.rules.initial_cops>14 || native.rules.initial_racers>14)return false;
        for(const auto &post:native.posts)
            if(!std::isfinite(post.distance) || !std::isfinite(post.cue) || !std::isfinite(post.shout_started))return false;
        // Native eject pointers are local addresses retained from this journal,
        // but an allocator can still have recycled them since capture.
        if(native.eject[local].active){
            unsigned bike=0;
            if(!engine::read_u32(m,0x800d8570+local*0x118+0xe0,bike) || bike!=native.eject[local].bike ||
               !std::isfinite(native.eject[local].durability))return false;
        }
        Commit commit{&plan,&result,&expected,network.ticket};
        if(!live.commit(commit_history,&commit))return false;
        commit_cop_rules(native.rules);commit_cop_posts(native.posts);commit_manual_eject(native.eject);
        return true;
    }catch(const std::bad_alloc&){return false;}
}
}
