#include "rr64_attack_visual_memory.hpp"
#include <vector>
#include <cstdlib>
#include <limits>
#include <algorithm>
void check(bool ok) { if(!ok) std::abort(); }
int main() {
    using namespace rr64;
    std::vector<unsigned char> memory(16*1024*1024);
    auto *m=memory.data(); constexpr unsigned source=0x80100000,target=0x80200000;
    engine::write_float(m,source+0x558,0.5f);
    engine::write_float(m,source+0x55c,0.25f);
    engine::write_float(m,source+0x54c,-1.f);
    engine::write_u32(m,source+0x568,attack_visual::descriptor_base+7*76);
    engine::write_u32(m,source+0x564,1);
    engine::write_u16(m,source+0x528,0xffff);
    auto v=attack_visual::capture(m,source);
    check(v.valid && v.descriptor==7 && v.clocks[1]==0.25f && !v.weapon_visible);
    std::fill(memory.begin()+0x200000,memory.begin()+0x200000+engine::rider::stride,0x5a);
    const auto original=memory;
    attack_visual::Saved saved;
    check(saved.apply(m,target,v));
    unsigned x=0;engine::read_u32(m,target+0x568,x);check(x==attack_visual::descriptor_base+7*76);
    engine::read_u32(m,target+0x538,x);check(x==0x5a5a5a5a); // Local resource untouched.
    saved.restore(m);check(memory==original); // Includes every combat flag.
    for(unsigned bad:{48u,65535u}) { v.descriptor=bad;check(!saved.apply(m,target,v));check(memory==original); }
    v=attack_visual::capture(m,source);v.clocks[2]=std::numeric_limits<float>::quiet_NaN();
    check(!saved.apply(m,target,v));check(memory==original);
    engine::write_u32(m,source+0x568,0x80300000);check(!attack_visual::capture(m,source).valid);
    engine::write_float(m,source+0x558,0);v=attack_visual::capture(m,source);check(v.valid);
    const auto idle_original=memory;
    check(saved.apply(m,target,v));float phase=1;engine::read_float(m,target+0x558,phase);check(phase==0);
    saved.restore(m);check(memory==idle_original);
}
