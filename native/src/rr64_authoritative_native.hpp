#pragma once
#include "recomp.h"
#include "rr64_authoritative_input.hpp"
namespace rr64::authority {
// Translate one owned input through the original game routine; does not step
// physics, change authority, or acknowledge the command.
bool native_controls(unsigned char*,recomp_context&,const Command&,std::uint16_t previous,std::uint32_t round,std::uint16_t presses=0);
}
