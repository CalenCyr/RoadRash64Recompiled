#include "rr64_engine_layout.hpp"
#include <vector>
#include <cstdlib>
#include <cstdio>
extern "C" void test_credit_weapon(unsigned char*, recomp_context*);
extern "C" void test_credit_impact(unsigned char*, recomp_context*);
extern "C" void func_8005FD80(unsigned char*, recomp_context*);
extern "C" void func_80063B50(unsigned char*, recomp_context*);
extern "C" void func_80064430(unsigned char*, recomp_context*);
extern "C" void test_unguarded_scenery(unsigned char*, recomp_context*);
extern "C" int rr64_valid_combat_statistics(unsigned address) {
    return rr64::engine::valid_combat_statistics(address);
}
unsigned notifications=0, expected_event=8, invalid_credit_attempts=0;
extern "C" void func_800636B0(unsigned char*, recomp_context* c) {
    if(c->r5!=expected_event) std::abort();
    ++notifications;
}

// Negative-control probes stop invalid accesses before touching guest memory.
// Valid inputs still execute the native helpers, for exact memory comparison.
extern "C" void test_statistics_probe(unsigned char* m, recomp_context* c) {
    if (!rr64_valid_combat_statistics(static_cast<unsigned>(c->r4))) {
        ++invalid_credit_attempts;
        return;
    }
    func_80063B50(m, c);
}
extern "C" void test_mayhem_probe(unsigned char* m, recomp_context* c) {
    if (!rr64_valid_combat_statistics(static_cast<unsigned>(c->r4) + 0x2Cu)) {
        ++invalid_credit_attempts;
        return;
    }
    func_80064430(m, c);
}

static unsigned check_scenery_credit() {
    using namespace rr64::engine;
    constexpr unsigned first_actor=0x800D8570u, object=0x80110000u;
    constexpr unsigned mayhem_record=0x80120000u, stack=0x807F0000u;
    constexpr unsigned mayhem_disabled=0x800D7678u, mode=0x800D8524u;
    unsigned cases=0, negative_controls=0;
    expected_event=0x18;
    // All fourteen racer slots, then null/sentinel, misaligned, out-of-pool,
    // wrapped actor+2C and a readable RDRAM address that is not a racer record.
    for (unsigned index=0; index<20; ++index) {
        const unsigned actor=index<14 ? first_actor+index*0x118u :
            index==14 ? 0u : index==15 ? 0xFFFF0000u :
            index==16 ? first_actor+1u : index==17 ? first_actor+14*0x118u :
            index==18 ? 0xFFFFFFD4u : 0x80000000u;
        for (float remaining : {1.0f, 0.0f, -1.0f}) {
            // Normal race; active Mayhem counter; disabled Mayhem counter.
            for (unsigned mode_case=0; mode_case<3; ++mode_case) {
                std::vector<unsigned char> memory(kRdramSize);
                auto* m=memory.data();
                write_float(m, object+0x320, remaining);
                write_u32(m, object+0x10, 7);
                write_u32(m, mode, mode_case==0 ? 0 : 5);
                write_u16(m, mayhem_disabled, mode_case==2 ? 1 : 0);
                write_u32(m, mayhem_record+0x58, 9);
                for (unsigned i=0; i<14; ++i) {
                    write_u16(m, first_actor+i*0x118+0x5C, 11);
                    write_u32(m, first_actor+i*0x118+0xE8, mayhem_record);
                }
                recomp_context c{};
                c.f_odd=&c.f0.u32h;
                c.r4=guest_address(actor); c.r5=guest_address(object);
                c.r29=guest_address(stack); c.r31=0x12345678;
                c.r16=0x23456789; c.r17=0x3456789A; c.f20.fl=123.0f;
                auto old_memory=memory;
                auto old_context=c;
                old_context.f_odd=&old_context.f0.u32h;
                notifications=0;
                func_8005FD80(m, &c);
                const unsigned credit=index<14 && remaining>0;
                if (notifications!=credit || c.r29!=guest_address(stack) ||
                    c.r31!=0x12345678 || c.r16!=0x23456789 ||
                    c.r17!=0x3456789A || c.f20.fl!=123.0f) std::abort();
                float after=0;
                unsigned state=0, mayhem=0;
                read_float(m,object+0x320,after);
                read_u32(m,object+0x10,state);
                read_u32(m,mayhem_record+0x58,mayhem);
                if (after!=(remaining>0 ? 0 : remaining) ||
                    state!=(remaining>0 ? 2u : 7u) ||
                    mayhem!=9u+(credit && mode_case==1)) std::abort();
                for (unsigned i=0; i<14; ++i) {
                    std::uint16_t count=0;
                    read_u16(m,first_actor+i*0x118+0x5C,count);
                    if (count!=11u+(credit && i==index)) std::abort();
                }
                // A second contact cannot double-award or destroy again.
                c.r4=guest_address(actor); c.r5=guest_address(object);
                func_8005FD80(m, &c);
                if (notifications!=credit) std::abort();
                notifications=0; invalid_credit_attempts=0;
                test_unguarded_scenery(old_memory.data(), &old_context);
                if (notifications!=credit) std::abort();
                const unsigned old_invalid=index>=14 && remaining>0 ? 2 : 0;
                if (invalid_credit_attempts!=old_invalid) std::abort();
                negative_controls+=old_invalid!=0;
                // Preserving the old valid behavior includes both native
                // credit writes, destruction, and guest stack/register saves.
                if (old_memory!=memory) std::abort();
                ++cases;
            }
        }
    }
    if (negative_controls!=18) std::abort();
    std::printf("Scenery credit: %u native handler cases, %u invalid-attribution negative controls passed\n",
                cases, negative_controls);
    return cases;
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
    check_scenery_credit();
}
