#include "rr64_prediction_order_stats.hpp"
#include "recomp.h"
#include <vector>
#include <algorithm>
#include <cstdlib>
#include <cstdio>
#include <limits>
extern "C" void native_neighbor_reference(unsigned char*,recomp_context*);
extern "C" void native_leader_record(unsigned char*,recomp_context*);
extern "C" void native_leader_finish(unsigned char*,recomp_context*);
static void check(bool value){if(!value)std::abort();}
int main(){
    using namespace rr64::engine;
    constexpr unsigned table=0x800d77a0,base=0x800d8570,stats=0x80300000,stack=0x807f0000;
    unsigned cases=0;
    for(unsigned count=1;count<=14;++count)for(unsigned variant=0;variant<8;++variant){
        std::vector<unsigned char> m(kRdramSize);
        write_float(m.data(),0x80006bbc,10000.0f);
        write_u32(m.data(),0x800d7648,variant==6?0:variant==7?0xffffffffu:count);
        for(unsigned i=0;i<count;++i){
            const unsigned slot=count-1-i; // Sorted order need not equal actor slot.
            write_u32(m.data(),table+i*8,base+slot*0x118);
            const float gap=variant==1?0.0f:variant==2?10.0f:float(i*i+1);
            write_float(m.data(),table+i*8+4,1000.0f-float(i)*gap);
            write_u32(m.data(),base+slot*0x118+0xe8,stats+slot*0x100);
            // Sentinels catch accidental writes to untouched boundary/finish fields.
            for(unsigned off=0;off<0x64;off+=4)write_u32(m.data(),stats+slot*0x100+off,0x13570000+off);
            write_u32(m.data(),stats+slot*0x100+0x50,variant==3 && i%2?1:variant==4?0x10000:0);
            write_u16(m.data(),stats+slot*0x100+0x48,variant==5?0:1);
            write_u16(m.data(),base+slot*0x118+0x26,variant==1?1:i%2);
        }
        auto reference=m;
        write_u32(reference.data(),0x800d7664,~0u);write_u32(reference.data(),0x800d7668,~0u);
        write_float(reference.data(),0x800d766c,0);
        for(unsigned i=0;i<count;++i){
            recomp_context c{};c.f_odd=&c.f0.u32h;c.r29=guest_address(stack);
            c.r17=guest_address(0x800d7620);c.r19=guest_address(table+i*8);c.r18=i;
            native_leader_record(reference.data(),&c);
        }
        {recomp_context c{};c.f_odd=&c.f0.u32h;c.r3=guest_address(0x800d7620);
            native_leader_finish(reference.data(),&c);}
        for(unsigned i=0;i<count;++i){
            unsigned actor=0,s=0,finished=0;
            read_u32(reference.data(),table+i*8,actor);read_u32(reference.data(),actor+0xe8,s);
            read_u32(reference.data(),s+0x50,finished);if(finished)continue;
            recomp_context c{};c.f_odd=&c.f0.u32h;c.r29=guest_address(stack);
            c.r16=guest_address(s);c.r18=i;c.r30=count;c.r23=guest_address(table);
            c.r20=guest_address(table+i*8+4);c.r22=guest_address(table+i*8-4);c.f24.fl=10000.0f;
            write_u32(reference.data(),stack+0x70,count-1);
            write_u32(reference.data(),stack+0x74,table+i*8+8);
            write_u32(reference.data(),stack+0x78,table+i*8+12);
            write_u32(reference.data(),stack+0x7c,table+i*8-8);
            native_neighbor_reference(reference.data(),&c);
        }
        check(rr64::prediction::refresh_order_neighbors(m.data(),count));
        // Reference stack is its only permitted extra write.
        std::fill(reference.begin()+(stack&0x7fffff),reference.begin()+(stack&0x7fffff)+0x80,0);
        check(m==reference);++cases;
        const auto valid=m;
        for(unsigned invalid=0;invalid<4;++invalid){
            auto bad=valid;
            if(invalid==0)write_u32(bad.data(),table,base+count*0x118);
            if(invalid==1)write_u32(bad.data(),base+(count-1)*0x118+0xe8,0x807ffffc);
            if(invalid==2)write_float(bad.data(),table+4,std::numeric_limits<float>::quiet_NaN());
            if(invalid==3)write_float(bad.data(),0x80006bbc,std::numeric_limits<float>::infinity());
            const auto before=bad;
            check(!rr64::prediction::refresh_order_neighbors(bad.data(),count) && bad==before);
        }
    }
    std::printf("Native neighbor equivalence: %u cases plus atomic rejection checks passed\n",cases);
}
