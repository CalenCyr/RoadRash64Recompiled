#pragma once
#include "rr64_engine_layout.hpp"
#include <span>
#include <vector>
#include <stdexcept>
#include <utility>
#include <array>

namespace rr64::prediction {
// A guest snapshot can omit a task currently held in the native worker's CPU
// registers. Account for every slot rather than silently restarting that task:
// it may already have sent its completion before returning the slot to the pool.
struct ResourceInventory {
    bool valid=false;
    std::uint32_t free=0,queued=0,priority=0,unrepresented=0;
    bool can_start_worker()const{return valid && !unrepresented;}
    static ResourceInventory inspect(unsigned char* memory) {
        ResourceInventory out;
        std::uint16_t free_count=0,head=0,tail=0;
        std::uint32_t count=0,first=0,capacity=0,messages=0;
        if(!engine::read_u16(memory,0x8009CE90,free_count) || free_count>32 ||
           !engine::read_u16(memory,0x8009CE92,head) || head>=32 ||
           !engine::read_u16(memory,0x8009CE94,tail) || tail>=32 ||
           !engine::read_u32(memory,0x800B1968,count) ||
           !engine::read_u32(memory,0x800B196C,first) ||
           !engine::read_u32(memory,0x800B1970,capacity) || !capacity || capacity>32 ||
           count>capacity || first>=capacity ||
           !engine::read_u32(memory,0x800B1974,messages) || (messages&3) ||
           !engine::valid_guest_range(messages,capacity*4))return out;
        std::uint32_t seen=0;
        const auto mark=[&](unsigned slot,std::uint32_t& group){
            if(slot>=32 || (seen&(1u<<slot)))return false;
            seen|=1u<<slot;group|=1u<<slot;return true;
        };
        for(unsigned i=0;i<free_count;++i){
            std::uint16_t slot=0;
            if(!engine::read_u16(memory,0x800B1580+2*i,slot) || !mark(slot,out.free))return {};
        }
        for(unsigned i=0;i<count;++i){
            std::uint32_t slot=0;
            if(!engine::read_u32(memory,messages+4*((first+i)%capacity),slot) || !mark(slot,out.queued))return {};
        }
        for(unsigned i=head;i!=tail;i=(i+1)%32){
            std::uint16_t slot=0;
            if(!engine::read_u16(memory,0x800B1530+2*i,slot) || !mark(slot,out.priority))return {};
        }
        out.unrepresented=~seen;out.valid=true;return out;
    }
};

// OS services for a disposable replay image. These never deliver messages to
// emulated threads or the live event queue. A blocked call is an explicit yield,
// not a fabricated completion. The caller must resume the appropriate worker.
class Resources {
public:
    struct Blocked { std::uint32_t queue; bool send; };
    struct Invalid : std::runtime_error { using std::runtime_error::runtime_error; };
    static constexpr std::uint32_t requests=0x800B1960, dma_done=0x800B1940;
    static constexpr std::uint32_t sync_done=0x800B1518, async_done=0x800B14C0;
private:
    std::vector<unsigned char> image_;
    std::span<const unsigned char> rom_;
    std::size_t remaining_=65536;
    struct Queue { std::uint32_t address,count,first,capacity,messages; };
    void operation() { if (!remaining_) throw Invalid("resource operation budget exhausted"); --remaining_; }
    static bool known(std::uint32_t q) {
        return q==requests || q==dma_done || q==sync_done || q==async_done;
    }
    Queue queue(std::uint32_t q) {
        if(!known(q)) throw Invalid("unregistered replay queue");
        Queue out{q,read(q+8),read(q+12),read(q+16),read(q+20)};
        if(!out.capacity || out.capacity>32 || out.count>out.capacity ||
           out.first>=out.capacity || (out.messages&3) ||
           !engine::valid_guest_range(out.messages,out.capacity*4))
            throw Invalid("invalid replay queue layout");
        return out;
    }
    void push(const Queue& q,std::uint32_t message) {
        write(q.messages+4*((q.first+q.count)%q.capacity),message);
        write(q.address+8,q.count+1);
    }
public:
    // Taking ownership prevents an accidental full-image correction of live
    // RDRAM. The ROM is immutable and must outlive this replay transaction.
    Resources(std::vector<unsigned char> image,std::span<const unsigned char> rom)
        :image_(std::move(image)),rom_(rom) {
        if(image_.size()!=engine::kRdramSize) throw Invalid("wrong replay image size");
    }
    unsigned char* memory(){return image_.data();}
    const auto& image()const{return image_;}
    // Consume a successfully completed private transaction without copying its
    // entire 8 MiB image. The moved-from service must not execute more work.
    std::vector<unsigned char> release_image() && noexcept {return std::move(image_);}
    std::span<const unsigned char> rom()const noexcept{return rom_;}
    void validate_worker(std::uint32_t stack_top) {
        auto inventory=ResourceInventory::inspect(memory());
        if(!inventory.can_start_worker() || !engine::valid_guest_range(stack_top-256,256))
            throw Invalid("resource worker state not replayable");
        for(auto q:{requests,dma_done,sync_done,async_done}) (void)queue(q);
        auto pending=inventory.queued|inventory.priority;
        for(unsigned slot=0;slot<32;++slot)if(pending&(1u<<slot)){
            auto task=0x800B15C0u+28*slot;
            auto destination=read(task+8),source=read(task+12),bytes=read(task+16),flag=read(task+20);
            auto physical=(source|0x10000000u)&0x1FFFFFFFu;
            if(flag && ((flag&1) || !engine::valid_guest_range(flag,2)))throw Invalid("invalid completion flag");
            if(bytes && (physical<0x10000000u || (physical&1) || (destination&7) ||
                !engine::valid_guest_range(destination,bytes) ||
                physical-0x10000000u>rom_.size() || bytes>rom_.size()-(physical-0x10000000u)))
                throw Invalid("invalid queued replay DMA");
            const auto overlaps=[&](unsigned start,unsigned length,unsigned other,unsigned extent){
                return length && static_cast<std::uint64_t>(start)<static_cast<std::uint64_t>(other)+extent &&
                    static_cast<std::uint64_t>(other)<static_cast<std::uint64_t>(start)+length;
            };
            if(overlaps(destination,bytes,stack_top-256,256) || (flag && overlaps(flag,2,stack_top-256,256)))
                throw Invalid("resource DMA overlaps private worker stack");
            // These are the worker's task records, ring and queues, not resource
            // payload storage. Copying into them would invalidate later bounds.
            if(overlaps(destination,bytes,0x800B12DC,0x6A0) || (flag && overlaps(flag,2,0x800B12DC,0x6A0)))
                throw Invalid("resource DMA overlaps queue bookkeeping");
        }
    }
    std::uint32_t read(std::uint32_t address) {
        std::uint32_t value=0;
        if((address&3) || !engine::read_u32(memory(),address,value)) throw Invalid("invalid replay word");
        return value;
    }
    void write(std::uint32_t address,std::uint32_t value) {
        if((address&3) || !engine::write_u32(memory(),address,value)) throw Invalid("invalid replay word");
    }
    int send(std::uint32_t address,std::uint32_t message,bool block) {
        operation();auto q=queue(address);
        if(q.count==q.capacity) {
            if(block)throw Blocked{address,true};
            return -1;
        }
        push(q,message);return 0;
    }
    int receive(std::uint32_t address,std::uint32_t destination,bool block) {
        operation();auto q=queue(address);
        if(destination && ((destination&3) || !engine::valid_guest_range(destination,4)))
            throw Invalid("invalid replay message destination");
        if(!q.count) {
            if(block)throw Blocked{address,false};
            return -1;
        }
        auto message=read(q.messages+4*q.first);
        if(destination)write(destination,message);
        write(address+8,q.count-1);write(address+12,(q.first+1)%q.capacity);return 0;
    }
    // Match PI's ROM address mapping, but check the entire transfer before any
    // mutation. DMA completion stays inside this image. No SRAM/device writes.
    void dma(std::uint32_t source,std::uint32_t destination,std::uint32_t bytes,
             std::uint32_t completion,std::uint32_t message,std::uint32_t direction) {
        operation();
        auto physical=(source|0x10000000u)&0x1FFFFFFFu;
        if(direction || physical<0x10000000u || (physical&1) || (destination&7) ||
           !engine::valid_guest_range(destination,bytes))throw Invalid("invalid replay DMA");
        auto offset=physical-0x10000000u;
        if(offset>rom_.size() || bytes>rom_.size()-offset || completion!=dma_done)
            throw Invalid("replay DMA outside ROM");
        auto q=queue(completion);
        if(q.count==q.capacity)throw Invalid("unconsumed replay DMA completion");
        for(std::uint32_t i=0;i<bytes;++i) image_[((destination-engine::kRdramBegin)+i)^3]=rom_[offset+i];
        push(q,message);
    }
};
}
