#include "rr64_prediction_resources.hpp"
#include "rr64_prediction_resource_worker.hpp"
#include <cstdio>
#include <cstdlib>
#include <algorithm>
#include <source_location>
using rr64::prediction::Resources;
static Resources* services;
static std::vector<unsigned> transferred;
static void check(bool v,std::source_location where=std::source_location::current()){
    if(!v){std::fprintf(stderr,"resource check failed line %u\n",where.line());std::fflush(stderr);std::exit(1);}
}
extern "C" void func_8000C720(uint8_t*,recomp_context*);
extern "C" void func_8000C7D0(uint8_t*,recomp_context*);
extern "C" void osSetIntMask_recomp(uint8_t*,recomp_context* c){c->r2=1;}
extern "C" void osInvalDCache_recomp(uint8_t*,recomp_context*){}
extern "C" void osSendMesg_recomp(uint8_t*,recomp_context* c){c->r2=S32(services->send(c->r4,c->r5,c->r6!=0));}
extern "C" void osRecvMesg_recomp(uint8_t*,recomp_context* c){
    c->r2=S32(services->receive(c->r4,c->r5,c->r6!=0));
}
extern "C" void osPiStartDma_recomp(uint8_t*,recomp_context* c){
    auto sp=static_cast<unsigned>(c->r29);
    services->dma(c->r7,services->read(sp+16),services->read(sp+20),services->read(sp+24),c->r4,c->r6);
    transferred.push_back(c->r7);c->r2=0;
}
static void initialize(Resources& r){
    unsigned buffer=0x800B2000;
    for(auto q:{Resources::requests,Resources::dma_done,Resources::sync_done,Resources::async_done}){
        r.write(q+8,0);r.write(q+12,0);r.write(q+16,32);r.write(q+20,buffer);buffer+=128;
    }
    rr64::engine::write_u16(r.memory(),0x8009CE90,32);
    for(unsigned i=0;i<32;++i)rr64::engine::write_u16(r.memory(),0x800B1580+2*i,i);
}
static unsigned task(Resources& r,unsigned flags,unsigned source,unsigned dest,unsigned bytes){
    recomp_context c{};c.r29=rr64::engine::guest_address(0x807FF000);
    func_8000C720(r.memory(),&c);unsigned slot=c.r2;check(slot<32);
    auto p=0x800B15C0+28*slot;
    rr64::engine::write_u16(r.memory(),p+6,flags);
    r.write(p+8,dest);r.write(p+12,source);r.write(p+16,bytes);
    r.write(p+20,0x800B3000+2*slot);rr64::engine::write_u16(r.memory(),p+24,0x400+slot);
    check(r.send(Resources::requests,slot,false)==0);return slot;
}
static void drain(Resources& r){
    check(rr64::prediction::ResourceInventory::inspect(r.memory()).can_start_worker());
    recomp_context c{};c.r29=rr64::engine::guest_address(0x807FE000);
    try{func_8000C7D0(r.memory(),&c);check(false);}
    catch(const Resources::Blocked& b){check(b.queue==Resources::requests && !b.send);}
}
int main() try {
    // Synthetic ROM only; independently check expected bytes and priority order.
    std::vector<unsigned char> rom(0x10000),live(rr64::engine::kRdramSize);
    for(unsigned i=0;i<rom.size();++i)rom[i]=static_cast<unsigned char>((i*13)^(i>>8));
    Resources r(live,rom);services=&r;initialize(r);
    auto normal=task(r,0,0x1000,0x80200000,0x4010);
    auto urgent=task(r,1,0x6000,0x80300000,0x20);
    Resources production(r.image(),rom);
    recomp_context worker_context{};worker_context.r29=rr64::engine::guest_address(0x807FE000);worker_context.f_odd=&worker_context.f0.u32h;
    rr64::prediction::CpuContext worker_entry;check(worker_entry.capture(worker_context));
    check(rr64::prediction::drain_resource_worker(production,worker_entry)==rr64::prediction::ResourceDrain::Idle);
    drain(r);
    check(production.image()==r.image());
    check((transferred==std::vector<unsigned>{0x6000,0x1000,0x5000}));
    for(unsigned i=0;i<0x4010;++i)check(r.image()[(0x200000+i)^3]==rom[0x1000+i]);
    for(unsigned i=0;i<0x20;++i)check(r.image()[(0x300000+i)^3]==rom[0x6000+i]);
    std::uint16_t value=0;
    check(rr64::engine::read_u16(r.memory(),0x8009CE90,value) && value==32);
    for(auto slot:{normal,urgent})check(rr64::engine::read_u16(r.memory(),0x800B3000+2*slot,value) && value==0x400+slot);
    check(r.read(Resources::sync_done+8)==1 && r.read(Resources::async_done+8)==1);
    check(rr64::prediction::ResourceInventory::inspect(r.memory()).free==0xFFFFFFFFu);
    // Stop at an observed background-work boundary, not at queue exhaustion.
    // A second normal request remains pending and its payload stays untouched.
    initialize(r);
    auto first=task(r,0,0x1000,0x80400000,16);
    auto second=task(r,0,0x2000,0x80500000,16);
    auto target=rr64::prediction::ResourceInventory::inspect(r.memory());
    target.free|=1u<<first;target.queued=0;target.priority=1u<<second;
    check(rr64::prediction::drain_resource_worker(r,worker_entry,&target)==rr64::prediction::ResourceDrain::Reached);
    for(unsigned i=0;i<16;++i){check(r.image()[(0x400000+i)^3]==rom[0x1000+i]);check(r.image()[(0x500000+i)^3]==0);}
    check(r.read(Resources::async_done+8)==1);
    check(rr64::prediction::ResourceInventory::inspect(r.memory()).priority==(1u<<second));
    auto paused=r.image();
    check(rr64::prediction::drain_resource_worker(r,worker_entry,&target)==rr64::prediction::ResourceDrain::Reached);
    check(r.read(Resources::async_done+8)==1);
    check(rr64::prediction::drain_resource_worker(r,worker_entry)==rr64::prediction::ResourceDrain::Idle);
    for(unsigned i=0;i<16;++i)check(r.image()[(0x500000+i)^3]==rom[0x2000+i]);
    // Completed work cannot be undone to satisfy an older schedule.
    check(rr64::prediction::drain_resource_worker(r,worker_entry,&target)==rr64::prediction::ResourceDrain::Invalid);
    // Missing worker-held jobs and duplicate ownership are distinct; neither
    // is safe to resolve by inventing a ROM completion.
    initialize(r);
    rr64::engine::write_u16(r.memory(),0x8009CE90,31);
    auto inventory=rr64::prediction::ResourceInventory::inspect(r.memory());
    check(inventory.valid && inventory.unrepresented==0x80000000u && !inventory.can_start_worker());
    auto held_image=r.image();
    check(rr64::prediction::drain_resource_worker(r,worker_entry)==rr64::prediction::ResourceDrain::UnrepresentedTask);
    check(held_image==r.image());
    r.send(Resources::requests,31,false);
    check(rr64::prediction::ResourceInventory::inspect(r.memory()).can_start_worker());
    r.send(Resources::requests,31,false);
    check(!rr64::prediction::ResourceInventory::inspect(r.memory()).valid);
    check(std::all_of(live.begin(),live.end(),[](auto v){return v==0;}));
    // Invalid metadata is rejected before the generated worker touches memory.
    for(unsigned bad=0;bad<3;++bad){
        initialize(r);auto slot=task(r,0,0x1000,0x80200000,32);
        auto p=0x800B15C0+28*slot;
        if(bad==0)r.write(p+20,0x80800000);
        if(bad==1)r.write(p+8,0x807FDFC0);
        if(bad==2)r.write(p+8,0x800B1530);
        auto original=r.image();
        check(rr64::prediction::drain_resource_worker(r,worker_entry)==rr64::prediction::ResourceDrain::Invalid);
        check(original==r.image());
    }
    // The stock one-entry synchronous completion queue can block the worker.
    // It must not be reported as an idle/successful drain or restarted blindly.
    initialize(r);r.write(Resources::sync_done+16,1);
    task(r,1,0x1000,0x80200000,16);task(r,1,0x2000,0x80300000,16);
    check(rr64::prediction::drain_resource_worker(r,worker_entry)==rr64::prediction::ResourceDrain::Blocked);
    check(!rr64::prediction::ResourceInventory::inspect(r.memory()).can_start_worker());
    // Queue wrap, full/empty behavior and invalid transfers cannot fabricate
    // completions or modify an image on rejection.
    initialize(r);
    for(unsigned i=0;i<32;++i)check(r.send(Resources::requests,i,false)==0);
    check(r.send(Resources::requests,99,false)==-1);
    try{r.send(Resources::requests,99,true);check(false);}catch(const Resources::Blocked& b){check(b.send);}
    for(unsigned i=0;i<32;++i){check(r.receive(Resources::requests,0x800B4000,false)==0);check(r.read(0x800B4000)==i);}
    check(r.receive(Resources::requests,0,false)==-1);
    for(unsigned i=0;i<80;++i){r.send(Resources::requests,i,false);r.receive(Resources::requests,0x800B4000,false);check(r.read(0x800B4000)==i);}
    auto before=r.image();
    for(unsigned invalid=0;invalid<5;++invalid){
        try{
            r.dma(invalid==0?0xFFFFFFF0:invalid==1?1:0,invalid==2?0x807FFFF8:0x80200000,
                  0x100,invalid==3?Resources::requests:Resources::dma_done,0,invalid==4?1:0);
            check(false);
        }catch(const Resources::Invalid&){}
        check(r.image()==before);
    }
    try{r.receive(0x800B5000,0,false);check(false);}catch(const Resources::Invalid&){}
    check(r.image()==before);
    std::puts("Original ROM worker: urgent ordering, multi-chunk copy, task release, private completions and rejection checks passed");
}catch(const std::exception& e){std::fprintf(stderr,"resource error: %s\n",e.what());return 1;}
