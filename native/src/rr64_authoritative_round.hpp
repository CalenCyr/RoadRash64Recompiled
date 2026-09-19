#pragma once
#include "rr64_authoritative_input.hpp"

namespace rr64::authority {
constexpr unsigned maximum_players=14, commands_per_packet=16;
struct InputBatch {
    std::uint32_t round=0,count=0;
    std::array<Command,commands_per_packet> commands{};
};
static_assert(sizeof(InputBatch)<1200);
struct Step {
    std::uint32_t round=0;
    std::uint64_t tick=0;
    std::array<Command,maximum_players> inputs{};
    std::array<std::uint16_t,maximum_players> presses{};
};
struct Stamp {
    std::uint32_t round=0;
    std::uint64_t tick=0;
    std::array<std::uint32_t,maximum_players> acknowledged{};
};

// Game-thread coordinator. The network hands validated batches to receive;
// native simulation consumes begin's immutable Step and calls finish afterward.
class HostRound {
    std::uint32_t round_=0,humans_=0;
    std::array<HostInput,maximum_players> queues_{};
    std::array<unsigned,maximum_players> missing_{};
    std::array<Command,maximum_players> held_{};
    Step staged_{};
    Stamp completed_{};
    bool running_=false;
    std::uint32_t disconnected_=0;
public:
    void disconnect(unsigned slot){
        if(slot==0 || slot>=maximum_players)return;
        disconnected_|=1u<<slot;
        // A currently executing step may finish normally. Its queue commit
        // must remain intact until then, so disconnect is applied at begin.
    }
    bool reset(std::uint32_t round,std::uint32_t humans) {
        if(!round || !humans || humans>>maximum_players)return false;
        *this={};round_=round;humans_=humans;completed_.round=round;
        for(auto &q:queues_)q.reset(round);
        return true;
    }
    bool receive(unsigned authenticated_slot,const InputBatch &batch) {
        if(authenticated_slot>=maximum_players || (disconnected_&(1u<<authenticated_slot)) || !(humans_&(1u<<authenticated_slot)) ||
           batch.round!=round_ || !batch.count || batch.count>commands_per_packet)return false;
        // Reject the whole batch before mutating the queue. Redundant already
        // processed entries are harmless, but future conflicting entries aren't.
        auto candidate=queues_[authenticated_slot];
        for(unsigned i=0;i<batch.count;++i) {
            auto c=batch.commands[i];
            if(!valid(c,round_))return false;
            if(authenticated_slot)c.buttons&=~std::uint16_t(0x1000); // host-only pause
            if(c.sequence<=candidate.processed())continue;
            if(!candidate.receive(c))return false;
        }
        queues_[authenticated_slot]=candidate;return true;
    }
    bool begin(Step &out) {
        if(!round_ || running_)return false;
        staged_={};staged_.round=round_;staged_.tick=completed_.tick+1;
        for(unsigned s=0;s<maximum_players;++s)if(humans_&(1u<<s)) {
            if(disconnected_&(1u<<s)){queues_[s].discard_pending();held_[s]={};continue;}
            Command next{};
            if(queues_[s].stage_latest(next,held_[s].buttons,staged_.presses[s])) {
                missing_[s]=0;held_[s]=next;held_[s].actions=0;staged_.inputs[s]=next;
                // A tap released inside the burst still occupies this native
                // update. The retained held state remains the newest sample,
                // so it releases on the following update instead of sticking.
                staged_.inputs[s].buttons|=staged_.presses[s];
            }
            else {
                // Never replay an edge or acknowledge a missing command. Hold
                // analog/held controls for two native steps, then neutralize.
                ++missing_[s];staged_.inputs[s]=held_[s];staged_.inputs[s].sequence=0;staged_.inputs[s].actions=0;
                if(missing_[s]>2) {staged_.inputs[s].buttons=0;staged_.inputs[s].x=staged_.inputs[s].y=0;}
            }
        }
        running_=true;out=staged_;return true;
    }
    bool finish(std::uint64_t tick) {
        if(!running_ || tick!=staged_.tick)return false;
        for(unsigned s=0;s<maximum_players;++s) {
            const auto sequence=staged_.inputs[s].sequence;
            if(sequence && !queues_[s].commit(sequence))return false;
            completed_.acknowledged[s]=queues_[s].processed();
        }
        completed_.tick=tick;running_=false;return true;
    }
    Stamp stamp() const{return completed_;}
};
}
