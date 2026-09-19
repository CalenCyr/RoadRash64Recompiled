#include "rr64_authoritative_native.hpp"
#include "rr64_authoritative_controller.hpp"

extern "C" void func_80040664(unsigned char*,recomp_context*);

namespace rr64::authority {
// Native40664 only translates a rider's controls into the caller-provided
// outputs. It is not the full physics step and must not acknowledge a command.
// The caller supplies this frame's controls after resolving slot ownership.
bool native_controls(unsigned char *m,recomp_context &caller,const Command &command,
                     std::uint16_t previous,std::uint32_t round,std::uint16_t presses) {
    const unsigned actor=static_cast<unsigned>(caller.r4);
    ControllerScope scope;
    if(!scope.apply(m,actor,command,previous,round,presses))return false;
    // Retain the real native ABI and output pointers. This scope is deliberately
    // short: other actors and UI must never observe the scratch controller.
    func_80040664(m,&caller);
    scope.restore(m);
    return true;
}
}
