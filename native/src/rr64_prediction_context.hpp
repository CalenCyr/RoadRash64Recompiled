#pragma once
#include "recomp.h"
#include <cfenv>

namespace rr64::prediction {
// recomp_context contains a pointer into its own floating-point register bank.
// History stores register values only; each replay gets a freshly rebased view.
class CpuContext {
    recomp_context values_{};
    std::fenv_t floating_{};
    bool valid_=false;
    friend class FloatingPointScope;
public:
    bool capture(const recomp_context &source) {
        const auto *expected=source.mips3_float_mode ? &source.f1.u32l : &source.f0.u32h;
        if(source.mips3_float_mode>1 || source.f_odd!=expected)return false;
        std::fenv_t floating;
        if(std::fegetenv(&floating)!=0)return false;
        values_=source;
        floating_=floating;
        values_.f_odd=nullptr;
        valid_=true;
        return true;
    }
    bool restore(recomp_context &destination)const {
        if(!valid_)return false;
        destination=values_;
        destination.f_odd=destination.mips3_float_mode ? &destination.f1.u32l : &destination.f0.u32h;
        return true;
    }
};
// COP1 control uses the host thread's floating-point environment, not fields
// in recomp_context. Historical execution must restore it and leave the live
// caller unchanged, including on rejected replay and exception unwinding.
class FloatingPointScope {
    std::fenv_t caller_{};
    bool valid_=false;
public:
    explicit FloatingPointScope(const CpuContext &entry){
        if(!entry.valid_ || std::fegetenv(&caller_)!=0)return;
        if(std::fesetenv(&entry.floating_)==0)valid_=true;
        else std::fesetenv(&caller_);
    }
    FloatingPointScope(const FloatingPointScope&)=delete;
    FloatingPointScope& operator=(const FloatingPointScope&)=delete;
    ~FloatingPointScope(){if(valid_)std::fesetenv(&caller_);}
    bool valid()const{return valid_;}
};
}
