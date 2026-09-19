#pragma once
#include "rr64_prediction_replay.hpp"
#include "rr64_authoritative_native.hpp"

namespace rr64::prediction {
// Owns a command for one isolated outer update. The temporary controller mapping
// itself is installed only inside each native control translation, not physics.
class Controls {
    inline static thread_local Controls *current_=nullptr;
    authority::Command command_{};
    unsigned actor_=0,calls_=0;
    std::uint16_t previous_=0;
    bool translating_=false,failed_=false,owned_=false,observe_only_=false;
public:
    Controls(unsigned actor,const authority::Command &command,std::uint16_t previous,bool observe_only=false)
        :command_(command),actor_(actor),previous_(previous),observe_only_(observe_only) {
        if(active() && !current_ && authority::valid(command,command.round) &&
           actor>=0x800d8570u && (actor-0x800d8570u)%0x118u==0 &&
           (actor-0x800d8570u)/0x118u<14){current_=this;owned_=true;}
    }
    Controls(const Controls&)=delete;
    Controls& operator=(const Controls&)=delete;
    ~Controls(){if(owned_)current_=nullptr;}
    bool valid()const{return owned_;}
    bool completed(unsigned expected_passes)const {
        return owned_ && !failed_ && calls_==expected_passes;
    }
    static int translate(unsigned char *memory,void *context) {
        auto *self=current_;
        if(!active() || !self || self->translating_ || !context)return 0;
        auto &ctx=*static_cast<recomp_context*>(context);
        if(static_cast<unsigned>(ctx.r4)!=self->actor_)return 0;
        // AI verification must execute the original AI path, never impersonate
        // a human controller. Any human translation makes that sample invalid.
        if(self->observe_only_){self->failed_=true;return 0;}
        self->translating_=true;
        const bool ok=authority::native_controls(memory,ctx,self->command_,self->previous_,self->command_.round);
        self->translating_=false;
        if(ok){++self->calls_;self->previous_=self->command_.buttons;}
        else self->failed_=true;
        return 1;
    }
};
}
