#include "rr64_mk64_item_kernel.hpp"
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <source_location>
using namespace rr64::mk64_items;
namespace {
unsigned checks=0;
void check(bool condition,const char *description,std::source_location at=std::source_location::current()) {
    ++checks;if(!condition){std::fprintf(stderr,"MK64 review line %u: %s\n",at.line(),description);std::exit(1);}
}
std::array<Racer,3> racers() {
    std::array<Racer,3> r{};
    for(unsigned i=0;i<3;++i) {
        r[i]={true,true,false,true,{float(i)*20,20,0},{},{0,1,0},.45f,float(i)*10,25};
        r[i].contacts[0]={{0,0,.4f},.45f};
        r[i].contact_count=1;
    }
    r[0].position={0,0,0};return r;
}
SurfaceHit floor(void*,Vec start,Vec motion,float radius) {
    if(motion[2]<0 && start[2]+motion[2]<radius) {
        const float fraction=std::clamp((radius-start[2])/motion[2],0.f,1.f);
        return {true,fraction,{0,0,1},{start[0]+motion[0]*fraction,start[1]+motion[1]*fraction,radius}};
    }
    return {};
}
Object *find(Snapshot &s,Item item,ObjectMode mode) {
    for(auto &o:s.objects)if(o.generation && o.kind==item && o.mode==mode)return &o;
    return nullptr;
}
}
int main(int argc,char **argv) {
    const char *part=argc==2?argv[1]:"all";
    const auto selected=[&](const char *name){return !std::strcmp(part,"all") || !std::strcmp(part,name);};
    if(selected("blue")) {
        auto r=racers();Snapshot s;initialize(s,1,10);check(grant(s,0,Item::BlueShell),"blue grants");
        std::array<Use,3> use{};use[0]={true,0};step(s,r,use,10,.01f,{nullptr,floor,nullptr});
        auto *blue=find(s,Item::BlueShell,ObjectMode::Flying);check(blue && blue->target==2,"blue selects leader");
        r[1].progress=30;use={};step(s,r,use,11,.01f,{nullptr,floor,nullptr});
        blue=find(s,Item::BlueShell,ObjectMode::Flying);
        check(blue && blue->target==1,"blue follows a lead change while approaching");
        s.riders[1].star_until=100;step(s,r,use,12,.01f,{nullptr,floor,nullptr});
        blue=find(s,Item::BlueShell,ObjectMode::Flying);
        check(blue && blue->target==1,"star immunity cannot redirect blue to second place");
        check(valid(s),"leader-retarget state remains valid");
    }
    if(selected("bunch"))for(int direction:{0,80}) {
        auto r=racers();Snapshot s;initialize(s,1,10);check(grant(s,0,Item::BananaBunch),"bunch grants");
        std::array<Use,3> use{};use[0]={true,0};step(s,r,use,10,.01f,{nullptr,floor,nullptr});
        check(s.riders[0].charges==5 && s.riders[0].deployed,"bunch deploys five");
        use[0]={true,std::int8_t(direction)};step(s,r,use,15,.01f,{nullptr,floor,nullptr});
        auto *banana=find(s,Item::Banana,ObjectMode::Flying);check(banana,"one bunch banana released");
        check(s.riders[0].charges==4,"one banana charge used");
        check(direction?banana->velocity[1]>0:banana->velocity[1]<=0,"bunch release respects forward throw");
        check(banana->orbit==4,"bunch releases last banana in the trailing chain");
        check(valid(s),"bunch release state remains valid");
    }
    if(selected("green")) {
        auto r=racers();Snapshot s;initialize(s,1,10);check(grant(s,0,Item::GreenShell),"green grants");
        std::array<Use,3> use{};use[0]={true,-80};step(s,r,use,10,.01f,{nullptr,floor,nullptr});
        auto *green=find(s,Item::GreenShell,ObjectMode::Flying);
        check(green && green->velocity[1]<0 && green->target==no_target,"green fires backward without homing");
    }
    if(selected("star")) {
        auto r=racers();r[0].position={2,0,0};r[0].velocity={40,0,0};
        r[1].position={0,0,0};r[2].active=false;
        Snapshot s;initialize(s,1,10);s.riders[0].star_until=100;
        std::array<Use,3> use{};const auto result=step(s,r,use,11,.1f,{});
        check(result.hits[1].active && result.hits[1].item==Item::Star,
              "star contact covers a rider crossed between updates");
        check(result.hits[1].owner==0 && !result.hits[0].active,"star contact retains owner and immunity");
    }
    if(selected("effects")) {
        auto r=racers();Snapshot s;initialize(s,1,10);
        auto &effect=s.riders[0];effect.star_until=20;effect.boo_until=20;effect.shrink_until=20;
        effect.boost_until=20;effect.boost_speed=30;effect.hit_until=20;
        effect.held=Item::GoldenMushroom;effect.charges=1;effect.golden_until=20;
        std::array<Use,3> use{};step(s,r,use,20,.1f,{nullptr,floor,nullptr});
        check(!effect.star_until && !effect.boo_until && !effect.shrink_until && !effect.boost_until &&
              effect.boost_speed==0 && !effect.hit_until && !effect.golden_until && effect.held==Item::None,
              "all transient effects retire exactly at their deadline");
        check(valid(s),"expired effects yield a publishable snapshot");
        for(Item item:{Item::TripleGreenShell,Item::TripleRedShell,Item::BananaBunch}) {
            initialize(s,1,30);check(grant(s,0,item),"defense grants");use[0]={true,0};
            step(s,r,use,30,.01f,{nullptr,floor,nullptr});check(s.riders[0].deployed,"defense deployed");
            r[0].riding=false;use={};step(s,r,use,31,.01f,{nullptr,floor,nullptr});
            check(s.riders[0].held==Item::None && !s.riders[0].deployed,"fall clears attached inventory");
            check(std::none_of(s.objects.begin(),s.objects.end(),[](const auto &o){return o.generation!=0;}),
                  "fall clears all attached objects");r[0].riding=true;
        }
    }
    std::printf("MK64 independent kernel review %s: %u checks passed\n",part,checks);
}
