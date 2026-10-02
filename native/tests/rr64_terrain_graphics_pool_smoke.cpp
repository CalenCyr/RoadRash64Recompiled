#include "rr64_terrain_graphics_pool.hpp"
#include "rr64_engine_layout.hpp"
#include "recomp.h"

#include <algorithm>
#include <array>
#include <bit>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iterator>
#include <stdexcept>
#include <vector>

extern "C" void func_8001BD50(unsigned char*, recomp_context*);
extern "C" void func_8001BDF8(unsigned char*, recomp_context*);
extern "C" void func_8001C084(unsigned char*, recomp_context*);
extern "C" void func_8007B8D4(unsigned char*, recomp_context*) noexcept(false);

namespace {
using namespace rr64::engine;
using namespace rr64::terrain_graphics;
int failures = 0;
bool compiled = true;
unsigned graphics_attempts = 0, deferred_loads = 0;
void check(bool condition, const char* name) {
    if (!condition) { ++failures; std::fprintf(stderr, "FAIL: %s\n", name); }
}
unsigned word(unsigned char* m, unsigned p) { unsigned v = 0; read_u32(m,p,v); return v; }
recomp_context context() {
    recomp_context ctx{}; ctx.r29 = guest_address(0x800b1000u);
    ctx.f_odd = &ctx.f0.u32h; return ctx;
}
unsigned allocate(unsigned char* m, unsigned size) {
    rr64_terrain_pool_require(m, 2, size);
    auto ctx = context(); ctx.r4 = 2; ctx.r5 = size;
    func_8001BDF8(m, &ctx); return static_cast<unsigned>(ctx.r2);
}
void release(unsigned char* m, unsigned pointer) {
    auto ctx = context(); ctx.r4 = 2; ctx.r5 = guest_address(pointer);
    func_8001C084(m, &ctx);
}
void initialize(unsigned char* m) {
    auto ctx = context(); ctx.r4=2;ctx.r5=guest_address(0x80710000u);ctx.r6=0x20000;ctx.r7=8;
    func_8001BD50(m, &ctx);
}
bool rejected(unsigned char* m, unsigned bytes) {
    try { rr64_terrain_pool_require(m, 2, bytes); return false; }
    catch (const std::runtime_error&) { return true; }
}
}

// Headless boundaries: there is no renderer, resource worker, texture unload,
// or object creation in this fixture. The original streamer still computes its
// candidate sets and performs original pool1/pool2 frees. Pending ROM requests
// remain pending; a display-list build is represented only by its allocation.
extern "C" void rr64_prediction_verify_streaming(unsigned char*, void*) {}
extern "C" unsigned rr64_terrain_streaming_range(unsigned char*, unsigned bits) {
    return rr64::terrain_graphics::streaming_range(bits, compiled);
}
extern "C" void func_8000CE14(unsigned char*, recomp_context* ctx) { ++deferred_loads; ctx->r2=0; }
extern "C" void func_8007AB1C(unsigned char*, recomp_context*) {}
extern "C" void func_8007B6BC(unsigned char*, recomp_context*) {}
extern "C" void func_8007B7A0(unsigned char*, recomp_context*) {}
extern "C" void osRecvMesg_recomp(unsigned char*, recomp_context* ctx) { ctx->r2=0; }
extern "C" void func_8007D750(unsigned char* m, recomp_context* ctx) noexcept(false) {
    ++graphics_attempts;
    const unsigned cell=static_cast<unsigned>(ctx->r4);
    const unsigned allocation=allocate(m, 9568u); // actual captured refused request
    for (unsigned slot=0;slot<96;++slot) {
        if (word(m,0x800ddd00u+slot*4u)) continue;
        write_s8(m,cell+13,static_cast<std::int8_t>(slot));
        write_u32(m,0x800ddd00u+slot*4u,allocation);
        write_u32(m,0x800df088u+slot*4u,allocation+9568u-8u);
        return;
    }
    throw std::runtime_error("test exhausted native display slots");
}

int main(int argc, char** argv) {
    std::setvbuf(stdout,nullptr,_IONBF,0);
    std::vector<unsigned char> memory(kRdramSize);
    auto* m=memory.data();
    initialize(m);
    auto state=inspect_pool(m,9568);
    check(state.valid && state.free==0x20000u-16 && state.selected==0x80710008u,
          "original pool initializes and admission identifies exact first fit");
    for (unsigned session=0;session<8;++session) {
        initialize(m);
        std::array<unsigned,24> slots{};
        for (unsigned step=0;step<3000;++step) {
            auto& p=slots[step%slots.size()];
            if(p) release(m,p);
            const unsigned bytes=64u+((step*137u)%4000u);
            p=allocate(m,bytes);
            check(valid_guest_range(p,bytes) && !(p&7u),"original allocate/free stays inside guest pool");
        }
        for (auto p:slots) if(p) release(m,p);
        // Native free coalesces lazily; allocation must nevertheless progress.
        check(inspect_pool(m,4096).valid,"repeated native allocation/free has valid chain");
    }
    initialize(m);
    const auto whole=allocate(m,0x20000u-16u);
    check(rejected(m,8),"full pool refuses without entering native sentinel loop");
    release(m,whole);
    check(rejected(m,0) && rejected(m,0xffffffffu),"zero and overflowing requests are bounded");
    write_u32(m,0x80710000u,0);
    check(!inspect_pool(m,8).valid && rejected(m,8),"premature sentinel rejected");
    initialize(m);write_u32(m,0x80710000u,0xffffffffu);
    check(!inspect_pool(m,8).valid,"oversized block rejected");
    initialize(m);write_u32(m,0x800bbd08u,0x90000000u);
    check(rejected(m,8),"invalid pool pointer rejected before read");
    rr64_terrain_pool_require(nullptr,0,8);
    for(float f:{500.0f,999.0f,1000.0f,1999.0f,3000.0f,3600.0f}) {
        auto bits=std::bit_cast<unsigned>(f);
        check(streaming_range(bits,false)==bits,"off/fallback scope preserves original range");
        check(std::bit_cast<float>(streaming_range(bits,true))==std::min(f,999.0f),
              "compiled terrain caps only redundant native streaming range");
    }
    if(argc==2) {
        std::ifstream input(argv[1],std::ios::binary);
        input.read(reinterpret_cast<char*>(memory.data()),memory.size());
        if(!input) { std::fputs("Cannot read captured RDRAM\n",stderr); return 2; }
        const auto original=memory;
        state=inspect_pool(m,9568);
        std::printf("Captured pool: valid=%u used=%u free=%u largest=%u blocks=%u fit=%u\n",
            state.valid,state.used,state.free,state.largest,state.blocks,state.selected);
        check(state.valid && state.used==106984u && state.free==23760u &&
            state.largest==8304u && !state.selected && rejected(m,9568),
            "captured failure is valid fragmentation, not corruption or total exhaustion");
        const auto fits=allocate(m,8000);release(m,fits);
        check(inspect_pool(m,8).valid,"captured heap executes original allocator and free for fitting request");
        memory=original;m=memory.data();
        // Let outstanding old-range work age beyond the original two-epoch
        // retention period, then rotate native lists at the captured camera.
        // The cap prevents this pressure from boot; it is not a way to resume
        // a process already trapped inside the allocator.
        // No resource worker is synthesized and no live process is accessed.
        try {
            for(unsigned frame=0;frame<4;++frame) {
                write_u32(m,globals::terrain_request_epoch,
                    word(m,globals::terrain_request_epoch)+(frame==0?2u:1u));
                auto ctx=context();ctx.r4=0;
                func_8007B8D4(m,&ctx);
                state=inspect_pool(m,9568);
                std::printf("Capped native streamer %u: valid=%u used=%u free=%u largest=%u attempts=%u deferred=%u\n",
                    frame,state.valid,state.used,state.free,state.largest,graphics_attempts,deferred_loads);
            }
            state=inspect_pool(m,9568);
            check(state.valid && state.selected,"native near-tier retirement makes captured request admissible");
            if(state.valid && state.selected) {
                const auto p=allocate(m,9568);release(m,p);
                check(inspect_pool(m,8).valid,"captured refused size now executes original allocate/free");
            }
        } catch(const std::runtime_error& e) { check(false,e.what()); }
    }
    std::printf("Terrain graphics pool smoke: %s (%d failures)\n",failures?"FAIL":"PASS",failures);
    return failures?1:0;
}
