#include "rr64_engine_layout.hpp"
#include <vector>
#include <cstdlib>
#include <cstdio>
extern "C" void test_credit_weapon(unsigned char*, recomp_context*);
extern "C" void test_credit_impact(unsigned char*, recomp_context*);
extern "C" int rr64_valid_combat_statistics(unsigned address) {
    return rr64::engine::valid_combat_statistics(address);
}
unsigned notifications=0;
extern "C" void func_800636B0(unsigned char*, recomp_context* c) {
    if(c->r5!=8) std::abort();
    ++notifications;
}
int main() {
    using namespace rr64::engine;
    std::vector<unsigned char> memory(kRdramSize);
    auto* m=memory.data();
    constexpr unsigned rider=0x80100000, target=0x800D8570;
    unsigned cases=0;
    for(auto branch:{test_credit_weapon,test_credit_impact}) {
        for(unsigned index=0;index<18;++index) {
            const unsigned previous=index<14 ? target+index*0x118 :
                index==14 ? 0 : index==15 ? 0xffff0000u : index==16 ? target+1 : target+14*0x118;
            for(bool timer:{false,true}) {
                recomp_context c{};
                c.f_odd=&c.f0.u32h;
                c.r29=guest_address(0x807F0000);
                c.r16=guest_address(rider); c.r17=guest_address(target);
                c.r18=guest_address(target);
                c.r19=guest_address(target);
                c.f2.fl=timer?1.0f:0.0f; c.f20.fl=0;
                write_u32(m,target+0xE4,rider);
                write_u32(m,target+0xE0,0x80200000);
                write_u32(m,rider+0x10,previous);
                write_float(m,rider+0x18,c.f2.fl);
                if(index<14) write_u16(m,previous+0x3C,0);
                notifications=0;
                branch(m,&c);
                const unsigned expected=timer && index>0 && index<14;
                if(notifications!=expected) std::abort();
                if(index<14) {
                    std::uint16_t actual=0; read_u16(m,previous+0x3C,actual);
                    if(actual!=expected) std::abort();
                }
                ++cases;
            }
        }
    }
    std::printf("Combat credit: %u native branch cases passed\n",cases);
}
