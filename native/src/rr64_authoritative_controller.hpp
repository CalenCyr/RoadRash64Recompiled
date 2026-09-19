#pragma once
#include "rr64_authoritative_input.hpp"
#include "rr64_engine_layout.hpp"

namespace rr64::authority {
// For a single game-thread call to40664 only. The actor's canonical roster
// index never changes; its temporary controller index selects scratch slot0.
// Do not use this scope across an entire frame or around menu/UI processing.
class ControllerScope {
    unsigned actor_=0;
    std::uint32_t controller_=0;
    static constexpr std::array<unsigned,5> addresses{
        engine::globals::controller_buttons,engine::globals::controller_changed_buttons,
        engine::globals::controller_pressed_buttons,engine::globals::controller_stick_x,
        engine::globals::controller_stick_y};
    std::array<std::uint32_t,5> saved_{};
public:
    bool apply(unsigned char *m,unsigned actor,const Command &c,std::uint16_t previous,std::uint32_t round,std::uint16_t presses=0) {
        if(actor_ || !valid(c,round) || !engine::valid_guest_range(actor,0x118)) return false;
        if(!engine::read_u32(m,actor+4,controller_)) return false;
        for(unsigned i=0;i<addresses.size();++i)if(!engine::read_u32(m,addresses[i],saved_[i]))return false;
        actor_=actor;engine::write_u32(m,actor+4,0);
        engine::write_u16(m,addresses[0],c.buttons);
        engine::write_u16(m,addresses[1],(c.buttons^previous)|presses);
        engine::write_u16(m,addresses[2],(c.buttons&~previous)|presses);
        engine::write_u32(m,addresses[3],(saved_[3]&0x00ffffffu)|(std::uint32_t(std::uint8_t(c.x))<<24));
        engine::write_u32(m,addresses[4],(saved_[4]&0x00ffffffu)|(std::uint32_t(std::uint8_t(c.y))<<24));
        return true;
    }
    void restore(unsigned char *m) {
        if(!actor_)return;
        for(unsigned i=0;i<addresses.size();++i)engine::write_u32(m,addresses[i],saved_[i]);
        engine::write_u32(m,actor_+4,controller_);actor_=0;
    }
};
}
