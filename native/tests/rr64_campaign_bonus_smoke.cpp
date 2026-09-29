// Native campaign control and arithmetic, with rendering/model/audio IO stubbed.
// ROM metadata is read privately at execution and never embedded in the test.
#include "rr64_engine_layout.hpp"
#include "rr64_native.hpp"
#include "rr64_campaign_bonus_save.hpp"
#include "rr64_netplay.hpp"
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iterator>
#include <vector>

extern "C" {
void bonus_native_route(unsigned char*, recomp_context*);
void bonus_native_postrace(unsigned char*, recomp_context*);
void bonus_native_ai_pool(unsigned char*, recomp_context*);
void stock_bonus_native_route(unsigned char*, recomp_context*);
void stock_bonus_native_postrace(unsigned char*, recomp_context*);
void stock_func_80073658(unsigned char*, recomp_context*);
void stock_func_80072E74(unsigned char*, recomp_context*);
void func_80073658(unsigned char*, recomp_context*);
void func_80073728(unsigned char*, recomp_context*);
void func_80072E74(unsigned char*, recomp_context*);
void func_80072A14(unsigned char*, recomp_context*);
void func_800516B8(unsigned char*, recomp_context*);
void bonus_load_8002C3D8(unsigned char*, recomp_context*);
void bonus_load_8002C718(unsigned char*, recomp_context*);
void bonus_load_8007290C(unsigned char*, recomp_context*);
void bonus_load_800729B4(unsigned char*, recomp_context*);
void bonus_load_8002DE20(unsigned char*, recomp_context*);
void bonus_load_8002DE4C(unsigned char*, recomp_context*);
void bonus_load_8002DE84(unsigned char*, recomp_context*);
void bonus_load_80073994(unsigned char*, recomp_context*);
void bonus_load_80073A34(unsigned char*, recomp_context*);
}
namespace {
using namespace rr64::engine;
constexpr unsigned profile=0x800D6A40, descriptor=0x800D8520, actor=0x800D8570;
unsigned checks=0, completed=0, advanced=0, preview=0, mode=0;
unsigned allocated_ai=0;
bool online_active=false;
unsigned word(unsigned char* m, unsigned a) { unsigned v=0; read_u32(m,a,v); return v; }
unsigned half(unsigned char* m, unsigned a) { std::uint16_t v=0; read_u16(m,a,v); return v; }
void check(bool b,const char* message) { ++checks; if(!b) { std::fprintf(stderr,"FAIL %s\n",message); std::exit(1); } }
std::vector<unsigned char> bytes(unsigned char* m,unsigned a,unsigned n) {
    std::vector<unsigned char> v(n); for(unsigned i=0;i<n;++i) read_u8(m,a+i,v[i]); return v;
}
recomp_context context() { recomp_context c{}; c.r29=S32(0x807F0000); c.r31=0x12345678; return c; }
void postrace_context(recomp_context& c) { c=context(); c.r19=1; c.r21=S32(profile); c.r22=S32(0x800D0000); }
}
extern "C" void func_80072880(unsigned char*,recomp_context*) {}
extern "C" void func_80057F88(unsigned char*,recomp_context*) {}
extern "C" void func_80057FC0(unsigned char*,recomp_context*) {}
extern "C" void func_80057FF8(unsigned char*,recomp_context*) {}
extern "C" void func_80058CF0(unsigned char*,recomp_context*) {}
extern "C" void func_8002FAB8(unsigned char*,recomp_context*) {}
extern "C" void func_800796F8(unsigned char*,recomp_context*) {}
extern "C" void func_8006CB8C(unsigned char*,recomp_context*) {}
extern "C" void func_8005F3B4(unsigned char*,recomp_context*) {}
extern "C" void func_8006CF60(unsigned char*,recomp_context*) {}
extern "C" void func_800727A0(unsigned char*,recomp_context* c) { c->r2=7; }
extern "C" void func_8001A5D8(unsigned char*,recomp_context* c) { c->r2=0; }
extern "C" void func_800696E0(unsigned char*,recomp_context* c) { preview=unsigned(c->r4); c->f0.fl=1.0f; }
extern "C" void func_8001A46C(unsigned char* m,recomp_context* c) {
    for(unsigned i=0;i<unsigned(c->r6);++i) { std::uint8_t b; read_u8(m,unsigned(c->r5)+i,b); write_s8(m,unsigned(c->r4)+i,b); }
}
extern "C" void func_8001A440(unsigned char*,recomp_context*) {}
extern "C" unsigned rr64_local_bike_level(unsigned original) { return original; }
extern "C" int rr64_custom_cop_active() { return 0; }
extern "C" void rr64_custom_cop_ai_pool(unsigned char*,void*) {}
extern "C" unsigned rr64_online_bike_profile(unsigned char*,unsigned,unsigned donor) { return donor; }
namespace rr64::netplay { Status get_status() { Status s{}; s.active=online_active; return s; } }
extern "C" void func_8001A590(unsigned char*,recomp_context* c) { c->f0.fl=0.5f; }
extern "C" void func_80051E24(unsigned char* m,recomp_context* c) {
    const unsigned row=unsigned(c->r5), racer=unsigned(c->r4);
    check(row>=0x800A3460 && row<0x800A3460+160*16 && (row-0x800A3460)%16==0,"native AI assignment must have valid donor");
    check(racer>=actor && racer<actor+14*0x118,"native AI destination within14 actors");
    std::uint8_t bike=0; read_u8(m,row+10,bike);
    check(bike==25 || bike==26,"full native AI construction uses Insanity model");
    ++allocated_ai;
}
extern "C" void rr64_local_options_reset_race() {}
extern "C" void rr64_achievement_campaign_completed(unsigned char*) { ++completed; }
extern "C" void rr64_achievement_campaign_level_advanced(unsigned char*,unsigned) { ++advanced; }
extern "C" unsigned rr64_online_postrace_route_mode(unsigned char*,unsigned m) { mode=m; return m; }
extern "C" void rr64_trace_race_end(unsigned char*,unsigned,unsigned) {}
extern "C" void rr64_thrash_options_mode(unsigned) {}
extern "C" void rr64_online_menu_mode_changed(unsigned) {}
extern "C" void rr64_rival_engine_mode(unsigned char*,void*,unsigned) {}
extern "C" unsigned rr64_offline_bikes_count(unsigned char*,unsigned original,unsigned) { return original; }
extern "C" unsigned rr64_offline_bikes_shop_table(unsigned char*,unsigned original,unsigned) { return original; }

int main(int argc,char** argv) {
    check(argc==2,"private USA ROM path required");
    std::ifstream file(argv[1],std::ios::binary);
    std::vector<unsigned char> rom((std::istreambuf_iterator<char>(file)),{});
    check(rom.size()>=0x100000,"private ROM unavailable");
    std::vector<unsigned char> memory(kRdramSize);
    auto* m=memory.data();
    const auto seed=[&](unsigned a,unsigned size) {
        const unsigned off=a-0x80000000u+0xC00u;
        check(off+size<=rom.size(),"private metadata range");
        for(unsigned i=0;i<size;++i) write_s8(m,a+i,rom[off+i]);
    };
    seed(0x800A0000,0x7800); seed(0x80007000,0x400); seed(0x80004E80,0x30);
    write_u32(m,0x800A6524,0x80600000); write_u32(m,0x800A64E8,0x80610000);
    write_u32(m,profile+0x20,1); write_u32(m,profile+0x14,25);
    write_u32(m,profile+0x50,0x11111111); write_u32(m,profile+0x54,0x00010002);
    const auto original_tables=bytes(m,0x800A68C8,0x740C-0x68C8);
    const unsigned routes[]={43,44,45,47,48,54,37,46};

    // Drive the native clamp, full 72-byte donor copy, route ID and preview
    // pointer construction for every ordinary and bonus selection.
    for(unsigned level=0;level<=5;++level) {
        write_u32(m,profile+0x3C,level);
        const unsigned count=level==0 ? 6:8;
        for(unsigned selected=0;selected<count;++selected) {
            write_u32(m,0x800A6680,selected); write_u32(m,0x800A6684,~0u);
            auto expected=memory;
            auto c=context(), stock=context();
            if(level<5) stock_bonus_native_route(expected.data(),&stock);
            bonus_native_route(m,&c);
            const unsigned donor=word(m,0x800A73E4+std::min(level,4u)*4)+selected*0x48;
            if(level<5) {
                check(bytes(m,descriptor,0x48)==bytes(expected.data(),descriptor,0x48),"ordinary native descriptor changed");
                check(word(m,0x800A64D8)==word(expected.data(),0x800A64D8),"ordinary preview changed");
            } else {
                check(word(m,descriptor)==6 && word(m,descriptor+4)==1 && word(m,descriptor+0x28)==7,"bonus descriptor tier/type/AI donor pool");
                check(half(m,descriptor+8)==routes[selected] && word(m,0x800A64D8)==routes[selected] && word(m,0x800A64DC)==routes[selected],"bonus actual route IDs");
                check(preview==0x80600000+routes[selected]*76,"bonus native preview/name record");
                check(bytes(m,descriptor+0xC,0x1C)==bytes(m,donor+0xC,0x1C) && bytes(m,descriptor+0x2C,0x1C)==bytes(m,donor+0x2C,0x1C),"bonus inherited Level5 economics/population");
                check(word(m,0x800A6690)==6,"bonus native track tier");
            }
        }
        for(unsigned invalid : {~0u,count,100u}) {
            write_u32(m,0x800A6680,invalid); write_u32(m,0x800A6684,~0u);
            auto c=context(); bonus_native_route(m,&c);
            check(word(m,0x800A6680)==(invalid==~0u ? count-1:0),"native selection wraps within chapter");
        }
    }

    // Native rider-pool filter must select the actual Insanity model donors,
    // not ordinary Level 5 racers accidentally carried in the donor profile.
    write_u32(m,profile+0x3C,5); write_u32(m,0x800A6680,0); write_u32(m,0x800A6684,~0u);
    auto c=context(); bonus_native_route(m,&c);
    c=context(); c.r23=S32(descriptor);
    for(unsigned i=0;i<0x200;++i) write_s8(m,unsigned(c.r29)+0x48+i,0);
    bonus_native_ai_pool(m,&c);
    unsigned donors=0;
    for(unsigned gang=0;gang<8;++gang) {
        const unsigned count=word(m,0x807F0048+gang*4);
        check(count<=15,"native donor group bound");
        for(unsigned i=0;i<count;++i) {
            const unsigned row=word(m,0x807F0098+(gang*15+i)*4);
            std::uint8_t tier=0,bike=0; read_u8(m,row+8,tier); read_u8(m,row+10,bike);
            check(tier==7 && (bike==25 || bike==26),"native AI pool uses Insanity models"); ++donors;
        }
    }
    check(donors>0,"native Insanity donor pool is populated");
    // The demand repair is scoped to the offline bonus race, including its
    // pending initialization. A saved bonus profile alone is insufficient.
    for(unsigned scenario=0;scenario<6;++scenario) {
        write_u32(m,profile+0x3C,scenario==2 ? 4:5);
        write_u32(m,descriptor+0x28,scenario==3 ? 4:7);
        write_u32(m,globals::main_mode,scenario==1 || scenario==4 ? 0x2F:0x18);
        write_u32(m,globals::pending_mode,scenario==1 ? 0x18:0x2F);
        online_active=scenario==5; c=context();
        for(unsigned i=0;i<0x50;++i) write_s8(m,0x807F0048+i,0);
        write_u32(m,0x807F0048+4,3); write_u32(m,0x807F0070+8,2);
        const auto before=bytes(m,0x807F0048,0x50);
        rr64_local_bike_ai_pool(m,&c);
        if(scenario<2) check(word(m,0x807F0074)==2 && word(m,0x807F0078)==0,"offline bonus redirects unsupported donor demands");
        else check(bytes(m,0x807F0048,0x50)==before,"bonus AI guard changed ordinary/menu/online demands");
    }
    online_active=false;
    unsigned ai_cases=0;
    for(unsigned track=0;track<8;++track) for(unsigned bike:{25u,26u}) for(unsigned family=1;family<=6;++family) {
        write_u32(m,globals::main_mode,0x18); write_u32(m,globals::pending_mode,0x18);
        write_u32(m,profile+0x3C,5); write_u32(m,0x800A6680,track); write_u32(m,0x800A6684,~0u);
        c=context(); bonus_native_route(m,&c);
        for(unsigned i=0;i<14*0x118;++i) write_s8(m,actor+i,0);
        write_u32(m,actor+0x18,bike); write_u32(m,actor+0x20,family);
        write_u16(m,actor+0x24,1); write_u16(m,actor+0x26,0);
        c=context(); c.r4=S32(descriptor); c.r5=0x3E800000; c.r6=0x3F400000; c.r7=0x3F000000;
        write_u32(m,unsigned(c.r29)+0x10,2); allocated_ai=0;
        func_800516B8(m,&c);
        unsigned population=0; for(unsigned i=0;i<8;++i) population+=half(m,descriptor+0x2C+i*2);
        check(allocated_ai==population-1 && allocated_ai>0 && allocated_ai<14,"native donor demands cover whole campaign population");
        check(word(m,actor+0x18)==bike,"native AI assignment preserved selected human model");
        check(unsigned(c.r29)==0x807F0000 && c.r31==0x12345678,"native full AI producer preserves context");
        ++ai_cases;
    }

    // Execute each actual fee/shop load with its real address register. This
    // catches correct helper logic wired to a wrong native register or table.
    struct Load { void(*call)(unsigned char*,recomp_context*); unsigned base,in,out,bonus; };
    const Load loads[]={
        {bonus_load_8002C3D8,0x800A68B4,2,6,4}, {bonus_load_8002C718,0x800A68A0,2,6,4},
        {bonus_load_8007290C,0x800A68B4,5,8,4}, {bonus_load_800729B4,0x800A68A0,4,8,4},
        {bonus_load_8002DE20,0x800A66B8,3,2,6}, {bonus_load_8002DE4C,0x800A66B8,3,2,6},
        {bonus_load_8002DE84,0x800A6854,3,5,6}, {bonus_load_80073994,0x800A6854,2,4,6},
        {bonus_load_80073A34,0x800A6854,3,4,6}};
    const auto reg=[](recomp_context& v,unsigned r)->gpr& {
        switch(r) { case 2:return v.r2; case 3:return v.r3; case 4:return v.r4;
            case 5:return v.r5; case 6:return v.r6; default:return v.r8; }
    };
    for(unsigned level=0;level<=5;++level) for(const auto& load:loads) {
        write_u32(m,profile+0x3C,level); c=context();
        reg(c,load.in)=S32(load.base+level*4); load.call(m,&c);
        const unsigned index=level==5 ? load.bonus:level;
        check(unsigned(reg(c,load.out))==word(m,load.base+index*4),"actual native fee/shop load bounded to correct tier");
    }
    write_u32(m,profile+0x3C,4); c=context(); c.r2=S32(0x800A6854+5*4);
    bonus_load_80073994(m,&c);
    check(unsigned(c.r4)==word(m,0x800A6854+6*4),"completed Level5 next shop initializes Insanity");
    write_u32(m,profile+0x3C,5);

    // Both purchased variants pass unchanged through the native race setup.
    for(unsigned bike : {25u,26u}) {
        write_u32(m,profile+0x14,bike); c=context(); func_80073728(m,&c);
        check(word(m,actor+0x18)==bike && word(m,0x800A6418)==6,"native human bike and race tier");
        check(word(m,0x800A656C)>1 && word(m,0x800A656C)<=14,"native campaign population stays within14");
        check(unsigned(c.r29)==0x807F0000,"native race stack preserved");
    }

    const auto set_results=[&](unsigned level,unsigned value) {
        write_u32(m,profile+0x3C,level);
        if(level==5) {
            rr64_campaign_begin_bonus(m);
            rr64_campaign_qualification_store(m,profile+0x54,value);
        } else write_u32(m,profile+0x40+level*4,value);
    };
    const auto results=[&](unsigned level) {
        const unsigned address=profile+0x40+level*4;
        return rr64_campaign_qualification_word(m,address,word(m,address));
    };
    // Native menu scans all eight nibbles, including the most significant one.
    for(unsigned level=0;level<=5;++level) {
        const unsigned count=level==0 ? 6:8;
        for(unsigned missing=0;missing<=count;++missing) {
            const unsigned value=missing<count ? 0x11111111u & ~(15u<<(missing*4)):0x11111111u;
            set_results(level,value); write_u32(m,0x800A6680,(missing+1)%count);
            auto before=bytes(m,profile,0xF8); c=context(); func_80073658(m,&c);
            check(word(m,0x800A6680)==(missing<count ? missing:0),"native first unqualified/replay selection");
            check(bytes(m,profile,0xF8)==before && results(level)==value,"menu changed progress or inventory");
        }
    }
    // Stock ordinary behavior is byte-identical through real prize and writes.
    write_u32(m,actor+0xE8,0x80620000); write_u32(m,0x80620044,1);
    write_u32(m,actor+0x28,13);
    for(unsigned level=0;level<5;++level) {
        set_results(level,0x87654321); write_u32(m,0x800A6680,2);
        write_u32(m,0x800A6684,~0u); c=context(); bonus_native_route(m,&c);
        auto expected=memory; auto stock=context(); stock_func_80072E74(expected.data(),&stock);
        c=context(); func_80072E74(m,&c);
        check(bytes(m,profile,0xF8)==bytes(expected.data(),profile,0xF8),"ordinary prize/qualification differs from stock");
    }
    // Every repeat counter, finish branch and course passes through the actual
    // native load/clear/store/OR/store sequence, preserving profile+54 aliases.
    unsigned prize_cases=0;
    for(unsigned track=0;track<8;++track) for(unsigned old=0;old<16;++old) for(unsigned place=0;place<4;++place) {
        const unsigned shift=track*4;
        const unsigned before=(0x87654321u & ~(15u<<shift)) | (old<<shift);
        set_results(5,before);
        write_u32(m,profile+0x54,0x00120034); write_u32(m,profile+0x38,1000);
        write_u32(m,profile+0x28,10); write_u32(m,profile+0x34,20);
        const auto inventory=bytes(m,profile+0x54,0xA2);
        const auto base_progress=bytes(m,profile+0x40,0x14);
        write_u32(m,0x800A6680,track); write_u32(m,0x800A6684,~0u);
        c=context(); bonus_native_route(m,&c);
        const unsigned prize=half(m,descriptor+0xC+place*2)/(old+1);
        write_u32(m,0x80620044,place);
        c=context(); func_80072E74(m,&c);
        const unsigned after=place<3 ? (before&~(15u<<shift))|(std::min(old+1,15u)<<shift):before;
        check(results(5)==after,"native bonus qualification load/store and saturation");
        check(bytes(m,profile+0x54,0xA2)==inventory,"bonus qualification corrupted existing inventory/statistics");
        check(bytes(m,profile+0x40,0x14)==base_progress,"bonus qualification changed original chapters");
        check(word(m,profile+0x38)==1000+prize+35 && word(m,0x800A66A0)==prize,"bonus Level5 prize/repeat divisor/combat economics");
        check(word(m,profile+0x28)==11 && word(m,profile+0x34)==33,"native race/distance statistics");
        check(word(m,profile+0x3C)==5 && unsigned(c.r29)==0x807F0000 && c.r31==0x12345678,"result kept chapter and context");
        // Alternate result routine awards the same divided money but never
        // qualifies. Its separate qualification read hook must also be correct.
        set_results(5,before); write_u32(m,profile+0x38,1000);
        c=context(); func_80072A14(m,&c);
        check(results(5)==before && word(m,profile+0x38)==1000+prize+35,"alternate native result prize and unchanged qualification");
        check(word(m,profile+0x54)==0x00120034,"alternate result preserves aliased word");
        ++prize_cases;
    }
    // Run all eight first-time qualifications, accept each result and replay
    // after the eighth. No synthetic achievement or second ending is emitted.
    set_results(5,0); write_u32(m,0x800A1810,0x35); write_u16(m,0x8009CDA8,0x9000);
    write_u32(m,0x80620044,0); completed=advanced=0;
    for(unsigned track=0;track<8;++track) {
        write_u32(m,0x800A6680,track); write_u32(m,0x800A6684,~0u);
        c=context(); bonus_native_route(m,&c);
        c=context(); func_80072E74(m,&c);
        postrace_context(c); mode=0; bonus_native_postrace(m,&c);
        check(mode==0x2F && word(m,0x800A1814)==0x2F && word(m,profile+0x3C)==5,"bonus result returns to campaign through native mode table");
        check(completed==0 && advanced==0,"bonus emitted original completion/advancement achievement");
        c=context(); func_80073658(m,&c);
        check(word(m,0x800A6680)==(track==7 ? 0:track+1),"bonus sequence selects next course or replay");
    }
    check(results(5)==0x11111111,"all eight bonus qualifications retained");
    // Original Level 5 still enters the real ending mode and completion hook.
    set_results(4,0x11111111); completed=advanced=mode=0; postrace_context(c); bonus_native_postrace(m,&c);
    check(mode==0x38 && completed==1 && advanced==0 && word(m,profile+0x3C)==4,"original Level5 ending/completion changed");
    for(unsigned missing=0;missing<8;++missing) {
        set_results(4,0x11111111u&~(15u<<(missing*4))); completed=advanced=mode=0;
        postrace_context(c); bonus_native_postrace(m,&c);
        check(mode==0x2F && completed==0 && advanced==0,"incomplete original campaign triggered ending");
    }
    check(bytes(m,0x800A68C8,0x740C-0x68C8)==original_tables,"shared campaign route tables mutated");
    std::printf("{\"passed\":true,\"checks\":%u,\"nativeDonors\":%u,\"nativeAiCases\":%u,\"nativePrizeCases\":%u,\"gameLaunched\":false}\n",checks,donors,ai_cases,prize_cases);
}
