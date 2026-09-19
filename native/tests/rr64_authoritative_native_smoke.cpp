#include "rr64_authoritative_native.hpp"
#include "rr64_authoritative_controller.hpp"
#include <vector>
#include <cstdlib>
void check(bool b){if(!b)std::abort();}
unsigned calls=0;
extern "C" void func_80040664(unsigned char *m,recomp_context *c) {
 using namespace rr64::engine;unsigned index=99;
 read_u32(m,static_cast<unsigned>(c->r4)+4,index);check(index==0);
 std::uint16_t held=0;read_u16(m,globals::controller_buttons,held);
 write_u16(m,static_cast<unsigned>(c->r5),held);
 c->r2=123;++calls;
}
int main(){
 using namespace rr64;std::vector<unsigned char> memory(16*1024*1024,0x5a);auto *m=memory.data();
 for(unsigned slot=0;slot<14;++slot){
 unsigned actor=0x800d8570+slot*0x118;engine::write_u32(m,actor+4,slot);
 auto before=memory;recomp_context ctx{};ctx.r4=static_cast<std::int64_t>(static_cast<std::int32_t>(actor));ctx.r5=static_cast<std::int64_t>(static_cast<std::int32_t>(0x807e0000));
 authority::Command command{7,1,0x8000,27,-33};
 check(authority::native_controls(m,ctx,command,0,7));check(ctx.r2==123);
 std::uint16_t result=0;engine::read_u16(m,0x807e0000,result);check(result==0x8000);
 engine::write_u16(m,0x807e0000,0x5a5a);check(memory==before);
 check(!authority::native_controls(m,ctx,command,0,8));check(memory==before);
 }
 check(calls==14);
}
