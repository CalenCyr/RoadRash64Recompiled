#pragma once
#include <array>
#include <cstdint>
#include <cstddef>

namespace rr64::authority {
constexpr std::uint8_t action_eject=1;
constexpr std::uint8_t allowed_actions=action_eject;
// One command is sampled for a local native update, never a rendered frame.
// Host updates can consume a contiguous burst of samples when client cadence
// is faster or packets arrive together. Button edges are retained separately.
// Slot identity comes from the authenticated connection, not this payload.
struct Command {
    std::uint32_t round=0, sequence=0;
    std::uint16_t buttons=0;
    std::int8_t x=0,y=0;
    std::uint8_t actions=0; // Edges: never repeated for a held/missing command.
    bool operator==(const Command&) const = default;
};
constexpr std::size_t history_capacity=256;
inline bool valid(const Command &c,std::uint32_t round) {
    return round && c.round==round && c.sequence && c.x!=-128 && c.y!=-128 && !(c.actions&~allowed_actions);
}

// Receiving input never acknowledges simulation. The caller must commit only
// after the native step has completed with the staged command.
class HostInput {
    std::array<Command,history_capacity> pending_{};
    std::uint32_t round_=0,processed_=0;
    Command staged_{};
public:
    void reset(std::uint32_t round){*this={};round_=round;}
    std::uint32_t processed() const{return processed_;}
    void discard_pending(){pending_={};staged_={};}
    bool receive(const Command &c) {
        if(!valid(c,round_) || c.sequence<=processed_ ||
           std::uint64_t(c.sequence)-processed_>history_capacity) return false;
        auto &entry=pending_[c.sequence%history_capacity];
        if(entry.sequence==c.sequence) return entry==c;
        entry=c;return true;
    }
    bool stage(Command &c) {
        const auto &next=pending_[(processed_+1)%history_capacity];
        if(next.sequence!=processed_+1 || next.round!=round_) return false;
        staged_=next;c=staged_;return true;
    }
    bool stage_latest(Command &c,std::uint16_t previous,std::uint16_t &presses) {
        Command latest{};std::uint8_t actions=0;presses=0;
        for(std::uint64_t sequence=std::uint64_t(processed_)+1;
            sequence<=std::uint64_t(processed_)+history_capacity && sequence<=UINT32_MAX;++sequence){
            const auto &sample=pending_[sequence%history_capacity];
            if(sample.sequence!=sequence || sample.round!=round_)break;
            presses|=sample.buttons&~previous;previous=sample.buttons;
            actions|=sample.actions;latest=sample;
        }
        if(!latest.sequence)return false;
        staged_=latest;c=latest;c.actions=actions;return true;
    }
    bool commit(std::uint32_t sequence) {
        if(!sequence || staged_.sequence!=sequence || sequence<=processed_) return false;
        for(std::uint64_t i=std::uint64_t(processed_)+1;i<=sequence;++i)pending_[i%history_capacity]={};
        processed_=sequence;staged_={};return true;
    }
};

class ClientHistory {
    std::array<Command,history_capacity> commands_{};
    std::uint32_t round_=0,next_=1,ack_=0;
    std::uint64_t server_tick_=0;
public:
    void reset(std::uint32_t round){*this={};round_=round;}
    std::size_t pending() const{return next_-1-ack_;}
    std::uint32_t acknowledged() const{return ack_;}
    std::uint32_t last_sequence() const{return next_-1;}
    std::uint64_t reconciled_tick() const{return server_tick_;}
    bool replay_commands(std::uint32_t round,std::uint64_t tick,std::uint32_t ack,
                         std::array<Command,history_capacity> &out,std::size_t &count) const {
        if(round!=round_ || !round || tick<=server_tick_ || ack<ack_ || ack>=next_)return false;
        count=next_-1-ack;
        for(std::size_t i=0;i<count;++i)out[i]=commands_[(ack+1+i)%history_capacity];
        return true;
    }
    // Called only after an isolated native replay has succeeded. The high-water
    // mark prevents losing input appended while that replay was being prepared.
    bool commit_replay(std::uint32_t round,std::uint64_t tick,std::uint32_t ack,std::uint32_t last) {
        if(round!=round_ || !round || tick<=server_tick_ || ack<ack_ || ack>=next_ || last!=next_-1)return false;
        ack_=ack;server_tick_=tick;return true;
    }
    bool append(std::uint16_t buttons,std::int8_t x,std::int8_t y,Command &out,std::uint8_t actions=0) {
        if((actions&~allowed_actions) || !round_ || pending()==history_capacity || next_==UINT32_MAX || x==-128 || y==-128) return false;
        out={round_,next_++,buttons,x,y,actions};commands_[out.sequence%history_capacity]=out;return true;
    }
    // Include oldest outstanding commands AND fresh controls. Sending only the
    // oldest window limits throughput to window/RTT on delayed links; sending
    // only the newest window can permanently lose a short tap at the first gap.
    template<std::size_t N> std::size_t resend(std::array<Command,N> &out) const {
        const auto count=pending()<N?pending():N;
        const auto oldest=pending()<=N?count:(N+1)/2;
        for(std::size_t i=0;i<oldest;++i)out[i]=commands_[(ack_+1+i)%history_capacity];
        for(std::size_t i=oldest;i<count;++i)out[i]=commands_[(next_-(count-i))%history_capacity];
        return count;
    }
    template<class State,class Replay>
    bool reconcile(std::uint32_t round,std::uint64_t tick,std::uint32_t ack,
                   const State &authoritative,State &predicted,Replay replay) {
        if(round!=round_ || !round || tick<=server_tick_ || ack<ack_ || ack>=next_) return false;
        // Work on a copy: a caller rejecting replay cannot leave a half-restored
        // prediction or discard the input history needed for recovery.
        State corrected=authoritative;
        for(std::uint32_t i=ack+1;i<next_;++i)
            if(!replay(corrected,commands_[i%history_capacity],true)) return false;
        predicted=corrected;ack_=ack;server_tick_=tick;return true;
    }
};
}
