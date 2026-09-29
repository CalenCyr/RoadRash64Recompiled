#pragma once
#include "rr64_engine_layout.hpp"
#include "rr64_authoritative_input.hpp"
#include <array>
#include <deque>
#include <memory>
#include <vector>
#include <new>
#include <cstring>
#include <cstdint>
#include <utility>

namespace rr64::prediction {
// Historical guest memory only, not host state or native C++ state. Immutable
// pages share unchanged data across input steps; pending steps are never evicted
// silently. Game-thread ownership is required. Not connected to live play yet.
class GuestJournal {
    static constexpr std::size_t page_size=4096;
    static constexpr std::size_t page_count=engine::kRdramSize/page_size;
    struct Page {std::array<unsigned char,page_size> bytes;};
    struct Budget {std::size_t used=0,limit=0;};
    using PageRef=std::shared_ptr<const Page>;
    struct Frame {std::uint32_t sequence=0;std::vector<PageRef> pages;};
    std::shared_ptr<Budget> budget_;
    std::deque<Frame> frames_;
    std::uint32_t round_=0;
    std::uint32_t baseline_sequence_=0;
    PageRef copy_page(const unsigned char *source){
        auto *page=new Page;
        std::memcpy(page->bytes.data(),source,page_size);
        ++budget_->used;
        return PageRef(page,[budget=budget_](const Page *p){delete p;--budget->used;});
    }
public:
    explicit GuestJournal(std::size_t maximum_data_bytes=128u*1024u*1024u)
        :budget_(std::make_shared<Budget>(Budget{0,maximum_data_bytes/page_size})){}
    GuestJournal(const GuestJournal&)=delete;
    GuestJournal& operator=(const GuestJournal&)=delete;
    // A correction starts at the acknowledged command, which need not be zero.
    // Build a replacement journal separately; only swap after replay commits.
    void reset(std::uint32_t round,std::uint32_t baseline_sequence=0){
        frames_.clear();round_=round;baseline_sequence_=baseline_sequence;
    }
    void swap(GuestJournal &other)noexcept{
        budget_.swap(other.budget_);frames_.swap(other.frames_);
        std::swap(round_,other.round_);std::swap(baseline_sequence_,other.baseline_sequence_);
    }
    std::size_t data_bytes()const{return budget_->used*page_size;}
    std::size_t metadata_bytes()const{return frames_.size()*page_count*sizeof(PageRef);}
    std::size_t frames()const{return frames_.size();}
    bool contains(std::uint32_t round,std::uint32_t sequence)const{
        if(!round || round!=round_)return false;
        for(const auto &frame:frames_)if(frame.sequence==sequence)return true;
        return false;
    }
    bool capture(std::uint32_t round,std::uint32_t sequence,const unsigned char *memory,std::size_t size){
        if(!memory || size!=engine::kRdramSize || !round || round!=round_ ||
           frames_.size()>=authority::history_capacity+1)return false;
        if(frames_.empty()?sequence!=baseline_sequence_:(frames_.back().sequence==UINT32_MAX || sequence!=frames_.back().sequence+1))return false;
        std::array<bool,page_count> changed{};std::size_t changes=0;
        for(std::size_t p=0;p<page_count;++p){
            changed[p]=frames_.empty() || std::memcmp(memory+p*page_size,frames_.back().pages[p]->bytes.data(),page_size)!=0;
            changes+=changed[p];
        }
        if(changes>budget_->limit || budget_->used>budget_->limit-changes)return false;
        try {
            Frame candidate{};candidate.sequence=sequence;candidate.pages.reserve(page_count);
            for(std::size_t p=0;p<page_count;++p)
                candidate.pages.push_back(changed[p]?copy_page(memory+p*page_size):frames_.back().pages[p]);
            frames_.push_back(std::move(candidate));
        } catch(const std::bad_alloc&){return false;}
        return true;
    }
    bool restore(std::uint32_t round,std::uint32_t sequence,unsigned char *memory,std::size_t size)const{
        if(!memory || size!=engine::kRdramSize || !round || round!=round_)return false;
        for(const auto &frame:frames_)if(frame.sequence==sequence){
            for(std::size_t p=0;p<page_count;++p)std::memcpy(memory+p*page_size,frame.pages[p]->bytes.data(),page_size);
            return true;
        }
        return false;
    }
    // Keep the acknowledged state as a replay baseline, plus every later step.
    bool retire_before(std::uint32_t round,std::uint32_t sequence){
        if(!round || round!=round_ || frames_.empty() || sequence<frames_.front().sequence || sequence>frames_.back().sequence)return false;
        while(frames_.front().sequence<sequence)frames_.pop_front();
        return true;
    }
};
}
