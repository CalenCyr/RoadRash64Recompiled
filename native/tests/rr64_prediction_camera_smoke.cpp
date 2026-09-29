#include "rr64_prediction_camera_diagnostics.hpp"
#include <cstdlib>
#include <iostream>
#include <limits>
#include <vector>

namespace {
unsigned checks=0;
void check(bool value,const char *why) {
    ++checks;
    if(!value){std::cerr<<"camera diagnostic check failed: "<<why<<'\n';std::exit(1);}
}
struct Authority {
    unsigned active=1;
    struct {
        unsigned valid=1,bike_attached=1,rider_attached=1,ejected=0,drive_lockout=0;
        std::array<float,4> bike_rotation{0,0,0,1};
    } root;
};
}
int main() {
    using namespace rr64;
    std::vector<unsigned char> memory(8*1024*1024);
    auto *m=memory.data();
    for(unsigned view=0;view<4;++view) {
        const unsigned mapped=3-view,actor=0x800d8570+mapped*0x118;
        const unsigned bike=0x80200000+mapped*0x2000,rider=bike+0x1000;
        engine::write_u32(m,0x800a4fa0+view*4,6+view);
        engine::write_u32(m,0x800a657c+view*4,mapped);
        engine::write_u16(m,actor+0x24,1);
        engine::write_u32(m,actor+0xe0,bike);engine::write_u32(m,actor+0xe4,rider);
        engine::write_u32(m,bike+engine::bike::rider_pointer,rider);
        engine::write_u32(m,rider+engine::rider::bike_pointer,bike);
        engine::write_u32(m,bike+4,actor);engine::write_u32(m,rider+4,actor);
        engine::write_u16(m,bike+engine::bike::rider_attached,view&1);
        engine::write_u16(m,rider+engine::rider::bike_attached,view&2);
        engine::write_u16(m,rider+engine::rider::ejected,view>=2);
        engine::write_u16(m,bike+engine::bike::drive_control_lockout,view==3);
        for(unsigned i=0;i<4;++i)engine::write_float(m,bike+0x244+i*4,float(view*10+i));
        for(unsigned i=0;i<3;++i)engine::write_float(m,bike+0x7dc+i*4,float(view*100+i));
    }
    const auto before=memory;
    for(unsigned view=0;view<4;++view) {
        const auto result=prediction::capture_camera(m,view);
        check(result.valid && result.mode==6+view && result.mapped_actor==3-view,"native view owns its mapped actor");
        const auto flags=(view&1)|((view&2)?2:0)|(view>=2?4:0)|(view==3?8:0);
        check(result.state_flags==flags,"attachment/crash flags retain independent state");
        for(unsigned i=0;i<4;++i)check(result.rotation[i]==float(view*10+i),"mapped quaternion captured unchanged");
        for(unsigned i=0;i<3;++i)check(result.anchor[i]==float(view*100+i),"mapped anchor captured unchanged");
    }
    check(memory==before,"capturing camera never writes guest memory");
    check(!prediction::capture_camera(nullptr,0).valid && !prediction::capture_camera(m,4).valid,"invalid memory/view rejected");
    const unsigned bike=0x80206000;
    engine::write_u32(m,bike+4,0);
    const auto wrong_owner=prediction::capture_camera(m,0);
    check(!wrong_owner.valid && wrong_owner.mode==6 && wrong_owner.mapped_actor==3,"mismatched owner invalidates actor but retains native controls");
    engine::write_u32(m,bike+4,0x800d8570+3*0x118);
    engine::write_float(m,bike+0x244,std::numeric_limits<float>::quiet_NaN());
    const auto nonfinite=prediction::capture_camera(m,0);
    check(!nonfinite.valid && nonfinite.rotation[0]==0,"nonfinite values cannot enter CSV");
    engine::write_u32(m,0x800a657c,14);
    check(!prediction::capture_camera(m,0).valid,"out-of-range mapped actor rejected");
    Authority authority;
    check(prediction::capture_camera_authority(authority).state_flags==3,"authority attachment flags use identical encoding");
    authority.root.bike_attached=0;authority.root.rider_attached=0;authority.root.ejected=1;authority.root.drive_lockout=1;
    check(prediction::capture_camera_authority(authority).state_flags==12,"authority crash flags use identical encoding");
    authority.active=0;check(!prediction::capture_camera_authority(authority).valid,"inactive authority invalid");
    authority.active=1;authority.root.valid=0;check(!prediction::capture_camera_authority(authority).valid,"invalid authority root invalid");
    std::cout<<"camera diagnostic smoke passed "<<checks<<" checks\n";
}
