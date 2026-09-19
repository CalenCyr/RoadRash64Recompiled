// Included only in the private generated worker translation unit.
#include "rr64_prediction_resource_worker.hpp"
namespace rr64::prediction {
namespace {
thread_local Resources* resource_services=nullptr;
thread_local const ResourceInventory* resource_target=nullptr;
struct ResourceTargetReached {};
void resource_checkpoint(){
    if(!resource_target)return;
    const auto current=ResourceInventory::inspect(resource_services->memory());
    if(!current.can_start_worker() || (current.free & ~resource_target->free))
        throw Resources::Invalid("resource schedule cannot reach observed inventory");
    if(current.free==resource_target->free && current.queued==resource_target->queued &&
       current.priority==resource_target->priority)throw ResourceTargetReached{};
}
void osSetIntMask_recomp(uint8_t*,recomp_context* context){context->r2=1;}
void osInvalDCache_recomp(uint8_t*,recomp_context*){}
void osSendMesg_recomp(uint8_t*,recomp_context* context){
    context->r2=S32(resource_services->send(context->r4,context->r5,context->r6!=0));
}
void osRecvMesg_recomp(uint8_t*,recomp_context* context){
    context->r2=S32(resource_services->receive(context->r4,context->r5,context->r6!=0));
}
void osPiStartDma_recomp(uint8_t*,recomp_context* context){
    const auto stack=static_cast<unsigned>(context->r29);
    resource_services->dma(context->r7,resource_services->read(stack+16),
        resource_services->read(stack+20),resource_services->read(stack+24),context->r4,context->r6);
    context->r2=0;
}
