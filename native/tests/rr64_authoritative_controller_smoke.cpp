#include "rr64_authoritative_controller.hpp"
#include <vector>
#include <cstdlib>
void check(bool b){if(!b)std::abort();}
int main(){
 using namespace rr64;std::vector<unsigned char> memory(16*1024*1024,0x5a);auto *m=memory.data();
 for(unsigned slot=0;slot<14;++slot){
 const unsigned actor=0x800d8570+slot*0x118;engine::write_u32(m,actor+4,slot);
 const auto before=memory;authority::ControllerScope scope;authority::Command c{7,1,0x8001,-127,63};
 check(scope.apply(m,actor,c,1,7));check(!scope.apply(m,actor,c,0,7));
 unsigned index=9;engine::read_u32(m,actor+4,index);check(index==0);
 std::uint16_t buttons=0;engine::read_u16(m,engine::globals::controller_buttons,buttons);check(buttons==0x8001);
 engine::read_u16(m,engine::globals::controller_changed_buttons,buttons);check(buttons==0x8000);
 engine::read_u16(m,engine::globals::controller_pressed_buttons,buttons);check(buttons==0x8000);
 std::uint8_t x=0;engine::read_u8(m,engine::globals::controller_stick_x,x);check(x==129);
 engine::read_u8(m,engine::globals::controller_stick_x+1,x);check(x==0x5a);
 scope.restore(m);check(memory==before);scope.restore(m);check(memory==before);
 check(!scope.apply(m,actor,c,0,8));check(memory==before);
 }
}
