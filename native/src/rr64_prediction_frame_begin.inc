#include "rr64_prediction_frame.hpp"
#include "rr64_prediction_controls.hpp"
#include "rr64_prediction_terrain_probe.hpp"
#include "rr64_prediction_dependency_probe.hpp"
#include "rr64_authoritative_eject.hpp"
#include "rr64_native.hpp"
#include <cstdio>
namespace rr64::prediction {
namespace {
thread_local Resources* frame_resources=nullptr;
thread_local CpuContext worker_entry;
thread_local unsigned frame_blocks=0;
thread_local char frame_error[192]{};
bool fail_frame(const char *reason){std::snprintf(frame_error,sizeof(frame_error),"%s",reason);return false;}
bool prepare_worker_entry(){
    // Stock0CB94 supplies800B12D8; osCreateThread reserves the first16 bytes.
    // Order may run before the first frame, so neither entry may rely on a
    // preceding transaction having initialized this thread-local context.
    recomp_context worker{};worker.r29=engine::guest_address(0x800B12C8);
    worker.f_odd=&worker.f0.u32h;
    return worker_entry.capture(worker);
}
void private_step(){
    if(!frame_blocks)throw Resources::Invalid("native replay work budget exhausted");
    --frame_blocks;
}
[[noreturn]] void private_break(std::uint32_t){throw Resources::Invalid("native replay break");}
[[noreturn]] void private_switch_error(const char*,std::uint32_t,std::uint32_t){throw Resources::Invalid("native replay switch");}
void osSetIntMask_recomp(uint8_t*,recomp_context* context){context->r2=1;}
void osSendMesg_recomp(uint8_t*,recomp_context* context){
    context->r2=S32(frame_resources->send(context->r4,context->r5,context->r6!=0));
}
void osRecvMesg_recomp(uint8_t*,recomp_context* context){
    const auto queue=static_cast<unsigned>(context->r4);
    // Try the historical queue first. Only a real blocking wait can advance
    // the private resource worker; nonblocking polling retains its semantics.
    int result=frame_resources->receive(queue,context->r5,false);
    if(result && context->r6){
        if(queue!=Resources::sync_done && queue!=Resources::async_done)
            throw Resources::Invalid("unregistered frame wait");
        if(drain_resource_worker(*frame_resources,worker_entry)!=ResourceDrain::Idle)
            throw Resources::Invalid("private resource worker could not finish");
        result=frame_resources->receive(queue,context->r5,false);
        if(result)throw Resources::Invalid("missing private resource completion");
    }
    context->r2=S32(result);
}
