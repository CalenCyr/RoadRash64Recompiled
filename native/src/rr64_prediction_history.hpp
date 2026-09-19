#pragma once
#include "rr64_prediction_frame.hpp"
#include "rr64_prediction_journal.hpp"
#include <span>
#include <vector>
#include <type_traits>

namespace rr64::prediction {
struct HistoricalStreaming {
    StreamingInput input;
    ResourceInventory schedule;
};
struct HistoricalReplay {
    std::uint64_t epoch=0;
    std::uint32_t round=0,acknowledged=0,last=0;
    std::vector<unsigned char> memory;
    FrameOutput baseline;
    std::vector<FrameInput> steps;
    // Operations between the previous completed update and each step's entry.
    std::vector<std::vector<HistoricalStreaming>> streaming;
};
struct ReplayedHistory {
    GuestJournal guest;
    std::vector<FrameOutput> native;
    std::vector<unsigned char> final_memory;
};
// Obtain one coherent historical baseline and all later input metadata without
// changing either journal or network acknowledgements. The caller must apply
// validated host state to this private image before executing the steps.
enum class ReplayAdmission { Invalid, Ready, ResourcePending };
bool prepare_history_replay(std::uint32_t round,std::uint32_t acknowledged,
    std::span<const authority::Command> commands,unsigned actor,HistoricalReplay&,
    ReplayAdmission *admission=nullptr);
// Host correction must already be applied to plan.memory. Retain each corrected
// state, so the next acknowledgement starts from corrected history too. Failure
// discards the private work and leaves the original plan/journal untouched.
template<class Prepare,class Execute,class Stream=std::nullptr_t>
bool evaluate_history_replay(const HistoricalReplay &plan,std::span<const unsigned char> rom,
    ReplayedHistory &out,Prepare prepare,Execute execute,Stream stream=nullptr){
    if(!plan.round || plan.memory.size()!=engine::kRdramSize || plan.last<plan.acknowledged ||
       plan.steps.size()!=std::size_t(plan.last-plan.acknowledged) ||
       (!plan.streaming.empty() && plan.streaming.size()!=plan.steps.size()))return false;
    try{
        Resources resources(plan.memory,rom);
        ReplayedHistory candidate;
        candidate.guest.reset(plan.round,plan.acknowledged);
        if(!candidate.guest.capture(plan.round,plan.acknowledged,resources.memory(),engine::kRdramSize))return false;
        candidate.native.reserve(plan.steps.size()+1);candidate.native.push_back(plan.baseline);
        auto native=plan.baseline;
        for(std::size_t i=0;i<plan.steps.size();++i){
            auto input=plan.steps[i];
            if(input.command.round!=plan.round || input.command.sequence!=std::uint64_t(plan.acknowledged)+1+i)return false;
            input.rules=native.rules;input.posts=native.posts;input.eject=native.eject;
            if(!plan.streaming.empty())for(const auto &event:plan.streaming[i]){
                if constexpr(std::is_same_v<Stream,std::nullptr_t>)return false;
                else if(!stream(resources,event.input,event.schedule))return false;
            }
            // Sequence zero is captured BEFORE the first update. Every other
            // baseline is an update completion: rebuild derived race order and
            // neighbors before the next update, just as the verified private
            // continuation does. Never copy the live roster into history.
            if((plan.acknowledged!=0 || i!=0) && !prepare(resources,input))return false;
            FrameOutput completed{};
            if(!execute(resources,input,completed) ||
               !candidate.guest.capture(plan.round,input.command.sequence,resources.memory(),engine::kRdramSize))return false;
            candidate.native.push_back(completed);native=completed;
        }
        candidate.final_memory.assign(resources.memory(),resources.memory()+engine::kRdramSize);
        out.guest.swap(candidate.guest);out.native.swap(candidate.native);out.final_memory.swap(candidate.final_memory);
        return true;
    }catch(const std::bad_alloc&){return false;}
     catch(const Resources::Invalid&){return false;}
     catch(const Resources::Blocked&){return false;}
}
// Validate live-state ownership first. Approval consumes the network ticket
// only AFTER all history allocation/validation succeeds. After approval, the
// journal swap cannot fail. Approval must not reenter or mutate game history.
// This commits historical storage only; it never writes to live RDRAM.
using CommitApproval=bool(*)(void*) noexcept;
bool commit_history_replay(const HistoricalReplay&,ReplayedHistory&,CommitApproval,void*);
}
