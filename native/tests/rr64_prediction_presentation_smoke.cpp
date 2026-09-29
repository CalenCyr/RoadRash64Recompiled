#include "rr64_prediction_presentation.hpp"
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <vector>

namespace {
unsigned checks=0;
void check(bool ok){++checks;if(!ok){std::fprintf(stderr,"correction presentation check %u failed\n",checks);std::abort();}}
}
int main(){
    using namespace rr64;
    constexpr unsigned actor=0x800d8570,bike=0x80100000,rider=0x80300000,route=0x80500000;
    std::vector<unsigned char> memory(engine::kRdramSize);
    auto *m=memory.data();
    engine::write_u16(m,actor+0x24,1);
    engine::write_u32(m,actor+0xe0,bike);engine::write_u32(m,actor+0xe4,rider);engine::write_u32(m,actor+0xe8,route);
    engine::write_u32(m,bike+engine::bike::rider_pointer,rider);engine::write_u32(m,rider+engine::rider::bike_pointer,bike);
    engine::write_u16(m,bike+engine::bike::rider_attached,1);engine::write_u16(m,rider+engine::rider::bike_attached,1);
    auto before=prediction::mounted_pose(m,7,0);check(before.valid);
    engine::write_float(m,bike+engine::bike::body_position,1.5f);
    auto after=prediction::mounted_pose(m,7,0);
    const auto committed=memory;
    prediction::CorrectionPresentation presentation;
    presentation.corrected(before,after,1000000);
    check(presentation.sample(after,1000000)[0]==-1.5f);
    const float decayed=presentation.sample(after,1040000)[0];
    check(decayed<0 && decayed>-1.5f);
    // Consecutive authority corrections preserve the already displayed point,
    // not an independently filtered rider root. Native movement stays exact.
    before=after;after.position[0]=2;
    presentation.corrected(before,after,1040000);
    check(std::abs(presentation.sample(after,1040000)[0]-(decayed-.5f))<.00001f);
    check(std::abs(presentation.sample(after,1440000)[0])<.02f);
    check(memory==committed);
    for(unsigned discontinuity=0;discontinuity<7;++discontinuity){
        auto changed=after;
        switch(discontinuity){
        case 0:changed.valid=false;break;
        case 1:++changed.round;break;
        case 2:++changed.recovery;break;
        case 3:++changed.model;break;
        case 4:changed.bike+=4;break;
        case 5:changed.rider+=4;break;
        default:changed.mapping=nullptr;break;
        }
        presentation.corrected(before,after,2000000);
        check(presentation.sample(changed,2000000)==std::array<float,3>{});
        check(presentation.sample(after,2000000)==std::array<float,3>{});
    }
    for(float x:{30.0f,std::numeric_limits<float>::infinity(),std::numeric_limits<float>::quiet_NaN()}){
        auto teleported=after;teleported.position[0]=x;
        presentation.corrected(before,teleported,3000000);
        check(presentation.sample(teleported,3000000)==std::array<float,3>{});
    }
    presentation.corrected(before,after,4000000);
    check(presentation.sample(after,4600000)==std::array<float,3>{});
    // Every native detached/crashed/eject/vault branch disables smoothing.
    for(auto [address,value]:std::array<std::pair<unsigned,unsigned>,5>{{
        {bike+engine::bike::rider_attached,0},{rider+engine::rider::bike_attached,0},
        {rider+engine::rider::ejected,1},{bike+engine::bike::drive_control_lockout,1},{bike+0x818,1}}}){
        std::uint16_t saved=0;engine::read_u16(m,address,saved);engine::write_u16(m,address,value);
        check(!prediction::mounted_pose(m,7,0).valid);engine::write_u16(m,address,saved);
    }
    std::printf("%u correction presentation checks passed; bounded mounted-pair offsets, no guest writes\n",checks);
}
