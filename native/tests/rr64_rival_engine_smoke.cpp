#include "rr64_rival_engine.hpp"
#include "rr64_engine_layout.hpp"
#include "rr64_netplay.hpp"
#include "rr64_prediction_replay.hpp"
#include <array>
#include <atomic>
#include <bit>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <new>
#include <vector>

extern "C" {
#define DECL(n) void func_##n(unsigned char*,recomp_context*);
DECL(800571DC) DECL(800597A0) DECL(8005980C) DECL(80080450) DECL(80082798)
DECL(800806B4) DECL(80080768) DECL(800807C0) DECL(80080820) DECL(80080890)
DECL(80083210) DECL(80083408) DECL(800817C0)
#undef DECL
void stock_func_800571DC(unsigned char*,recomp_context*);
void stock_func_8005980C(unsigned char*,recomp_context*);
void rival_native_pan_coefficients(unsigned char*,recomp_context*);
void rival_host_stereo(std::int16_t*,std::size_t,std::int16_t*);
void rr64_rival_engine_audio_reset();
// A native divide trap is a test failure, never an ignored arithmetic error.
void do_break(std::uint32_t vram) {
    std::fprintf(stderr, "Unexpected native break at %08x\n", unsigned(vram));
    std::abort();
}
}
namespace {
std::atomic<unsigned long long> allocations{0};
unsigned checks=0, native_pan=0;int highlights=0;
rr64::netplay::PhysicsRules rules{};
constexpr unsigned actors=0x800D8570,stack=0x807FF000,pool_base=0x80600000;
constexpr unsigned bank=0x800C1510,voice_stride=0x13C;
std::vector<unsigned char> initial,memory;
unsigned char *m=nullptr;
void check(bool v,const char*name){++checks;if(!v){std::fprintf(stderr,"FAIL %s\n",name);std::exit(1);}}
unsigned word(unsigned a){unsigned v=0;rr64::engine::read_u32(m,a,v);return v;}
unsigned half(unsigned a){std::uint16_t v=0;rr64::engine::read_u16(m,a,v);return v;}
void put(unsigned a,unsigned v){rr64::engine::write_u32(m,a,v);}
void shortword(unsigned a,unsigned v){rr64::engine::write_u16(m,a,std::uint16_t(v));}
void scalar(unsigned a,float v){put(a,std::bit_cast<unsigned>(v));}
float value(unsigned a){return std::bit_cast<float>(word(a));}
unsigned actor(unsigned s){return actors+s*0x118;}
unsigned bike(unsigned s){return 0x80100000+s*rr64::engine::bike::stride;}
unsigned body(unsigned s){return 0x80300000+s*rr64::engine::rider::stride;}
unsigned cache(unsigned s){return rr64::engine::racer_audio::cache+s*rr64::engine::racer_audio::row_stride;}
unsigned row(unsigned i){return pool_base+i*voice_stride;}
unsigned row_for(unsigned handle){for(unsigned i=4;i<word(0x800DF6E4);++i)if(word(row(i)+4)&&word(row(i)+0x44)==handle)return row(i);return 0;}
unsigned active(){unsigned n=0;for(unsigned i=4;i<word(0x800DF6E4);++i)n+=word(row(i)+4)!=0;return n;}
unsigned managed(){unsigned n=0;for(unsigned i=0;i<14;++i)n+=row_for(word(cache(i)))!=0;return n;}
void vec(unsigned a,float x,float y,float z=0){scalar(a,x);scalar(a+4,y);scalar(a+8,z);}
void position(unsigned slot,float x,float y,float z=0){
    vec(bike(slot)+rr64::engine::bike::body_position,x,y,z);vec(bike(slot)+0x16C,x,y,z);
    vec(body(slot)+0x8C,x,y,z);vec(bike(slot)+0x8C,x,y,z);
    vec(bike(slot)+rr64::engine::bike::front_wheel_position,x,y+1,z);
    vec(bike(slot)+rr64::engine::bike::rear_wheel_position,x,y-1,z);
}
recomp_context context(unsigned arg=0){recomp_context c{};c.r29=rr64::engine::guest_address(stack);c.r4=rr64::engine::guest_address(arg);return c;}
using Fn=void(*)(unsigned char*,recomp_context*);
void call(Fn f,unsigned arg){auto c=context(arg);f(m,&c);check(c.r29==rr64::engine::guest_address(stack),"native stack restored");}
void settle_releases(){
    // Models the eventual audio worker release only; allocation/update/stop
    // requests themselves execute the actual native code above.
    for(unsigned i=4;i<word(0x800DF6E4);++i)
        if(word(row(i)+4)&&word(row(i)+0x10)!=~0u){put(row(i)+4,0);put(row(i)+0x44,0);}
}
void frame(float seconds=1.f/60.f,bool releases=true){
    scalar(0x800A1820,value(0x800A1820)+seconds);put(0x800A1830,word(0x800A1830)+1);
    auto c=context();auto before=c;rr64_rival_engine_frame(m,&c);
    check(std::memcmp(&c,&before,sizeof(c))==0,"manager preserves caller registers");
    if(releases)settle_releases();
}
void frames(unsigned n=20){for(unsigned i=0;i<n;++i)frame();}
void reset(unsigned effect_count=16,unsigned humans=1){
    if(m){auto c=context();rr64_rival_engine_mode(m,&c,0);}
    rr64_rival_engine_audio_reset();rules={};highlights=0;
    memory=initial;m=memory.data();
    put(0x800DF6E4,effect_count+4);put(0x800DF6EC,pool_base);put(0x800DF6F0,pool_base+4*voice_stride);
    put(0x800DF6F4,60);put(0x800DF700,1);put(0x800DF718,bank);put(0x800DF71C,0);
    put(rr64::engine::globals::main_mode,9);put(rr64::engine::globals::pending_mode,9);
    put(rr64::engine::local_race::humans,humans);scalar(0x800A1820,10);scalar(0x800A1818,100);put(0x800A1830,100);
    for(unsigned i=0;i<4;++i){put(0x800A657C+i*4,i);vec(0x800B7418+i*0x24,0,0);vec(0x800B7424+i*0x24,0,1);}
    for(unsigned s=0;s<14;++s){
        put(actor(s),s);put(actor(s)+8,s<humans?s:~0u);shortword(actor(s)+0x24,1);
        put(actor(s)+0xE0,bike(s));put(actor(s)+0xE4,body(s));
        put(bike(s)+4,actor(s));put(body(s)+4,actor(s));
        put(bike(s)+rr64::engine::bike::rider_pointer,body(s));put(body(s)+rr64::engine::rider::bike_pointer,bike(s));
        shortword(bike(s)+rr64::engine::bike::rider_attached,1);shortword(body(s)+rr64::engine::rider::bike_attached,1);
        put(bike(s),0);scalar(bike(s)+0xC,600);scalar(bike(s)+0x490,0);scalar(bike(s)+0x494,0);scalar(bike(s)+0x498,10);
        position(s,s?1000.f+float(s):0,0);call(func_800597A0,bike(s));
    }
    rr64::rival_engine::set_volume_percent(35);
}
unsigned start_native(unsigned effect=0xEF,unsigned priority=100){auto c=context(effect);c.r5=100;c.r6=128;c.r7=0;put(stack+16,priority);func_80080450(m,&c);return unsigned(c.r2);}
void child_voice(unsigned parent){auto c=context(parent);c.r5=rr64::engine::guest_address(0x807E0300);m[0x7E0300^3]=0x80;m[0x7E0301^3]=0xEF;func_80083408(m,&c);check(c.r29==rr64::engine::guest_address(stack),"native child allocator restores stack");}
std::array<unsigned,2> coefficients(unsigned effective_pan){
    constexpr unsigned env=0x807E0000;auto c=context();c.r16=rr64::engine::guest_address(env);c.r21=rr64::engine::guest_address(0x800A8360);c.r8=127;
    shortword(env+0x58,effective_pan);shortword(env+0x5A,32767);rival_native_pan_coefficients(m,&c);
    return {half(env+0x5C),half(env+0x5E)};
}
unsigned effective_pan(unsigned voice){
    // Actual authored engine pan command, then actual mixer update logic.
    auto c=context(voice);c.r5=rr64::engine::guest_address(0x807E0100);m[0x7E0100^3]=127;func_80083210(m,&c);
    c=context(voice);c.r5=4;func_800817C0(m,&c);return native_pan;
}
}
void* operator new(std::size_t n){allocations.fetch_add(1,std::memory_order_relaxed);if(auto*p=std::malloc(n?n:1))return p;throw std::bad_alloc();}
void operator delete(void*p) noexcept {std::free(p);}
void operator delete(void*p,std::size_t) noexcept {std::free(p);}
void* operator new[](std::size_t n){return ::operator new(n);}
void operator delete[](void*p) noexcept {std::free(p);}
namespace rr64::netplay {PhysicsRules get_physics_rules(){return rules;}}
extern "C" int rr64_highlights_presenting(){return highlights;}
extern "C" void _nsqrtf(unsigned char*,recomp_context*c){c->f0.fl=std::sqrt(c->f12.fl);}
extern "C" void func_80087070(unsigned char*,recomp_context*){}
extern "C" void func_80086F50(unsigned char*,recomp_context*c){native_pan=unsigned(c->r5);}
// Other sustained racers' producers are outside this fixture's engine scope.
extern "C" void func_80056F48(unsigned char*,recomp_context*){}
extern "C" void func_80057578(unsigned char*,recomp_context*){}
extern "C" void func_800576D4(unsigned char*,recomp_context*){}
extern "C" void func_80057860(unsigned char*,recomp_context*){}

int main(int argc,char**argv){
    check(argc==2,"private ROM argument");std::ifstream input(argv[1],std::ios::binary);
    std::vector<unsigned char> rom((std::istreambuf_iterator<char>(input)),{});
    check(rom.size()>0x1E18380&&rom[0]==0x80&&rom[1]==0x37,"supported big endian ROM");
    initial.resize(8*1024*1024);m=initial.data();
    for(unsigned i=0x400;i<0xD0000;++i)m[i^3]=rom[i+0xC00];
    for(unsigned i=0;i<0x12220;++i)m[((bank&0x7FFFFF)+i)^3]=rom[0x1E06160+i];
    for(unsigned effect=0;effect<word(bank);++effect)put(bank+0x18+effect*8,bank+word(bank+0x18+effect*8));
    // Keep the bank initialized without opening its sample bank or a device.
    put(bank+0x10,0);m=nullptr;reset();
    check(rr64::rival_engine::get_volume_percent()==35,"default rival volume");
    for(double bad:{-5.,200.,double(NAN)}){rr64::rival_engine::set_volume_percent(bad);check(rr64::rival_engine::get_volume_percent()>=0&&rr64::rival_engine::get_volume_percent()<=100,"bounded volume setting");}
    // Negative control proves the native dispatcher excludes AI engines.
    reset();position(1,8,0);call(stock_func_8005980C,bike(1));check(active()==0,"stock AI gate has no engine");
    frames();
    check(active()==1&&row_for(word(cache(1))),"managed AI uses real native engine producer");
    check(word(row_for(word(cache(1)))+0x48)==0,"managed engine lower priority than native effects");
    check(word(cache(0))==~0u,"local human is never duplicated");
    // A playing handle receives continuous native pan/volume updates.
    const auto handle=word(cache(1));const auto right=coefficients(effective_pan(row_for(handle)));
    check(right[1]>right[0],"right source reaches physical right coefficient");
    shortword(0x807E0200,right[0]);shortword(0x807E0202,right[1]);std::array<std::int16_t,2> host{};
    rival_host_stereo(reinterpret_cast<std::int16_t*>(m+0x7E0200),2,host.data());
    check(host[0]==right[0]&&host[1]==right[1]&&host[1]>host[0],"production host endian correction preserves logical left/right");
    position(1,-8,0);frames();check(word(cache(1))==handle,"crossing listener retains engine handle");
    const auto left=coefficients(effective_pan(row_for(handle)));check(left[0]>left[1],"left source reaches physical left coefficient");
    const auto near_gain=half(row_for(handle)+0x9E);position(1,-60,0);frames();check(half(row_for(handle)+0x9E)<near_gain,"distance reduces native engine gain");
    position(1,-200,0);frames();check(active()==0,"out-of-range engine fades and releases");
    // Clock-based fades are monotonic and do not allocate a second voice.
    reset();position(1,0,15);unsigned old_gain=0;for(unsigned i=0;i<5;++i){frame(.01f);auto h=word(cache(1));check(row_for(h)&&half(row_for(h)+0x9E)>=old_gain,"fade in monotonic");old_gain=half(row_for(h)+0x9E);check(active()==1,"fade keeps one engine voice");}
    rr64::rival_engine::set_volume_percent(0);frames();check(active()==0,"zero slider fades to disabled");
    // Start cues take a different native early-return path than steady loops.
    reset();put(bike(1),12);position(1,8,0);scalar(0x800A1818,0);scalar(0x800A1820,0);frame();
    check(row_for(word(cache(1)))!=0,"native start cue can begin quietly");
    frames(4);auto cue=word(cache(1));auto cue_gain=half(row_for(cue)+0x9E);rr64::rival_engine::set_volume_percent(5);frame(.01f);
    check(word(cache(1))==cue&&half(row_for(cue)+0x9E)<cue_gain,"native start cue follows fade while retaining handle");
    // Every opponent participates in selection, while active engine rows stay bounded.
    for(unsigned effects:{8u,16u}){reset(effects);for(unsigned s=1;s<14;++s)position(s,float(s),10);frames();check(managed()==3&&active()==3,"crowded race bounded at three engines");
        for(unsigned s=1;s<14;++s)position(s,1000+float(s),0);position(13,0,5);frames(30);check(row_for(word(cache(13)))&&active()==1,"later AI can become audible after selection changes");}
    // Actual native allocator: reserve free rows, never steal a higher-priority effect.
    reset(8);std::array<unsigned,4> protected_handles{};for(auto &h:protected_handles)h=start_native();
    position(1,0,5);frames();check(active()==4&&word(cache(1))==0,"four free voices reserved");
    for(auto h:protected_handles)check(row_for(h)!=0,"native impact handles survive admission denial");
    reset(8);for(unsigned i=0;i<3;++i)start_native();for(unsigned s=1;s<5;++s)position(s,0,float(s));frames();check(active()==4&&managed()==1,"last admissible engine leaves four free effect voices");
    // Stop is a request, not a free row: a queued release must count against admission.
    auto pending=word(cache(1));auto c=context(pending);c.r5=0;func_800806B4(m,&c);
    check(row_for(pending)&&active()==4,"actual native stop retains voice until audio worker");frame(1.f/60.f,false);check(active()==4,"queued releases are not treated as free");
    // Native script fanout shares a handle; budget counts rows, not handles.
    reset();position(1,8,0);frames();auto parent=row_for(word(cache(1)));auto parent_handle=word(cache(1));
    child_voice(parent);child_voice(parent);check(active()==3,"owned child voices consume row budget");
    for(unsigned i=4;i<7;++i)check(word(row(i)+0x44)==parent_handle&&word(row(i)+0x48)==0,"child adopts owned parent handle at low priority");
    child_voice(parent);check(active()==3,"owned native child allocation cannot exceed three rows");
    put(cache(1),~0u);auto orphan_context=context();rr64_rival_engine_mode(m,&orphan_context,0);settle_releases();check(active()==0,"cleanup releases owned orphan and all child rows");
    // Existing local split-screen player engines stay native and AI is shared.
    reset(16,4);for(unsigned s=0;s<4;++s){position(s,float(s)*100,0);call(func_8005980C,bike(s));}
    const std::array<unsigned,4> human_handles={word(cache(0)),word(cache(1)),word(cache(2)),word(cache(3))};
    position(13,301,5);frames();check(active()==5&&row_for(word(cache(13))),"AI uses closest of four listeners once");
    for(unsigned s=0;s<4;++s)check(word(cache(s))==human_handles[s],"local multiplayer native engine unchanged");
    // Online human opponents are positional and their original dispatcher is gated.
    for(bool replicated:{false,true})for(unsigned local=0;local<(replicated?14u:4u);++local){
        reset();rules.active=rules.connected=true;rules.phase=rr64::netplay::Phase::Race;rules.local_slot=local;rules.replicated_riders=replicated;
        const unsigned own=replicated?0:local,remote=own?0:1;put(actor(own)+8,0);put(actor(remote)+8,1);
        position(own,0,0);position(remote,8,0);put(0x800A657C+(replicated?0:local)*4,own);
        call(func_8005980C,bike(remote));check(active()==0,"online remote original producer gated");
        call(func_8005980C,bike(own));const auto own_handle=word(cache(own));frames();
        check(active()==2&&word(cache(own))==own_handle,"online local native plus one remote managed engine");
        auto private_memory=memory;auto private_ctx=context();const auto before=memory;
        {rr64::prediction::ReplayScope replay;rr64_rival_engine_frame(private_memory.data(),&private_ctx);rr64_rival_engine_mode(private_memory.data(),&private_ctx,0);rr64_rival_engine_gate(private_memory.data(),&private_ctx);}
        check(private_memory==before&&memory==before,"private replay cannot produce, stop or alter live engine state");
    }
    // Bike source and listening rider must separate after an eject.
    reset();for(unsigned s=2;s<14;++s)position(s,3000+float(s),0);position(1,8,0);frames();auto near=half(row_for(word(cache(1)))+0x9E);
    shortword(body(0)+rr64::engine::rider::bike_attached,0);vec(body(0)+0x8C,1000,0);frames();check(active()==0,"fallen listener follows body away from parked own bike");
    position(1,1008,0);frames();check(row_for(word(cache(1)))&&half(row_for(word(cache(1)))+0x9E)==near,"engine source stays on rival bike near fallen listener");
    vec(0x800B7418,1000,0);vec(0x800B7424,1000,-1);frames();const auto turned=coefficients(effective_pan(row_for(word(cache(1)))));check(turned[0]>turned[1],"camera turn reverses stereo while fallen");
    // Lifecycle cases must release all owned voices and leave unrelated effects.
    for(unsigned mode=0;mode<9;++mode){reset();position(1,8,0);frames();const auto sound=start_native();
        if(mode==0)shortword(actor(1)+0x24,0);
        if(mode==1)shortword(bike(1)+rr64::engine::bike::rider_attached,0);
        if(mode==2)shortword(rr64::engine::globals::gameplay_pause_state,1);
        if(mode==3)highlights=1;
        if(mode==4)put(rr64::engine::globals::main_mode,0);
        if(mode==5){rules.active=true;rules.connected=false;}
        if(mode==6){auto ctx=context();rr64_rival_engine_recovery(m,&ctx,actor(1));position(1,1000,0);}
        if(mode==7){auto ctx=context();rr64_rival_engine_mode(m,&ctx,0);put(rr64::engine::globals::main_mode,0);}
        if(mode==8){auto ctx=context();rr64_rival_engine_mode(m,&ctx,0);put(rr64::engine::globals::pending_mode,0);}
        frames();check(active()==1&&row_for(sound),"disabled/dead/paused/highlight/menu/recovery cleanup preserves unrelated effect");
    }
    // Pitch and sample choice are the original bike-state algorithm.
    for(unsigned type:{0u,12u,15u,16u})for(float rpm:{10.f,150.f,400.f}){
        reset();put(bike(1),type);position(1,8,0);scalar(bike(1)+0x498,rpm);frames();auto managed_row=row_for(word(cache(1)));check(managed_row!=0,"bike profile engine audible");
        const auto effect=half(managed_row+0xA6);const auto pitch=value(managed_row+0x30);
        put(bike(2),type);put(actor(2)+8,2);scalar(bike(2)+0x498,rpm);call(stock_func_800571DC,bike(2));auto native_row=row_for(word(cache(2)));
        check(native_row&&half(native_row+0xA6)==effect&&value(native_row+0x30)==pitch,"managed engine reuses exact native sample and pitch");
    }
    // Bounded producer benchmark: real fourteen-racer/four-view scan and
    // actual native memory operations; no sound device, IO or allocator calls.
    reset(16,4);for(unsigned s=0;s<14;++s)position(s,float(s),5);frames();
    const auto before_alloc=allocations.load();const auto start=std::chrono::steady_clock::now();
    constexpr unsigned iterations=20000;for(unsigned i=0;i<iterations;++i)frame();
    const auto micros=std::chrono::duration<double,std::micro>(std::chrono::steady_clock::now()-start).count()/iterations;
    check(allocations.load()==before_alloc,"hot path has zero host heap allocations");check(active()<=3,"long producer run stays bounded");
    std::printf("{\"passed\":true,\"checks\":%u,\"benchmark_frames\":%u,\"microseconds_per_frame\":%.3f,\"benchmark_heap_allocations\":0,\"native_allocator\":true,\"game_launched\":false}\n",checks,iterations,micros);
}
