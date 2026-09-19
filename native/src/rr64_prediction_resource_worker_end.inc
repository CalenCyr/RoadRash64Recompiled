} // private generated functions and OS services

ResourceDrain drain_resource_worker(Resources& resources,const CpuContext& entry,const ResourceInventory* target){
    if(resource_services)return ResourceDrain::Invalid;
    const auto inventory=ResourceInventory::inspect(resources.memory());
    if(!inventory.valid)return ResourceDrain::Invalid;
    if(!inventory.can_start_worker())return ResourceDrain::UnrepresentedTask;
    if(target && !target->can_start_worker())return ResourceDrain::UnrepresentedTask;
    FloatingPointScope floating(entry);
    if(!floating.valid())return ResourceDrain::Invalid;
    recomp_context context{};
    if(!entry.restore(context) ||
       context.r29!=engine::guest_address(static_cast<unsigned>(context.r29)) ||
       !engine::valid_guest_range(static_cast<unsigned>(context.r29)-256,256))
        return ResourceDrain::Invalid;
    struct Binding {
        Binding(Resources& value,const ResourceInventory* target){resource_services=&value;resource_target=target;}
        ~Binding(){resource_services=nullptr;resource_target=nullptr;}
    } binding(resources,target);
    try {
        resources.validate_worker(static_cast<unsigned>(context.r29));
        func_8000C7D0(resources.memory(),&context);
    }catch(const ResourceTargetReached&){return ResourceDrain::Reached;}
    catch(const Resources::Blocked& blocked){
        if(!blocked.send && blocked.queue==Resources::requests &&
           ResourceInventory::inspect(resources.memory()).free==0xFFFFFFFFu)
            return ResourceDrain::Idle;
        // Partial images are disposable; this result is not a continuation PC.
        // The caller must not restart the worker after an interrupted send.
        return ResourceDrain::Blocked;
    }catch(const Resources::Invalid&){return ResourceDrain::Invalid;}
    return ResourceDrain::Invalid;
}
} // rr64::prediction
