#include "rr64_course_ai.hpp"
#include "rr64_course_guardrail.hpp"
#include "rr64_course_hazard_motion.hpp"
#include "rr64_course_impact.hpp"
#include "rr64_engine_layout.hpp"
#include "rr64_experimental_course.hpp"
#include "rr64_netplay.hpp"
#include "rr64_prediction_replay.hpp"
#include "rr64_race_end_trace.hpp"

#include <algorithm>
#include <bit>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <utility>
#include <vector>

using namespace rr64;
using namespace rr64::engine;
using namespace rr64::course_walls;
namespace {
unsigned checks = 0;
bool enabled = true;
netplay::PhysicsRules rules{};
experimental_course::RouteData route{};
void check(bool ok, const char *message) {
    ++checks;
    if (!ok) { std::fprintf(stderr, "Course AI: %s\n", message); std::exit(1); }
}
constexpr unsigned actor = 0x800D8688, bike_address = 0x80100000,
                   rider_address = 0x80102000, status = 0x80103000,
                   course_projection = 0x80103100, projection = 0x80103200,
                   profile = 0x80103300, ai = 0x80103400,
                   cache = 0x80200000, donor = 0x80300000, stack = 0x807F0000;
unsigned word(unsigned char *m, unsigned p) { unsigned v=0; read_u32(m,p,v); return v; }
recomp_context context() {
    recomp_context c{};
    c.r29=guest_address(stack); c.r31=0x12345678;
    c.r19=guest_address(actor); c.r18=guest_address(projection);
    c.r21=guest_address(rider_address); c.r16=guest_address(projection+0x14);
    return c;
}
std::vector<unsigned char> records(33*16);
std::vector<float> heights(33, 0);
void seed(unsigned char *m) {
    enabled=true; rules={}; prediction::replay_active=false;
    write_u32(m, globals::main_mode,28); write_u32(m,globals::pending_mode,28);
    write_u16(m,actor+0x24,1); write_u16(m,actor+0x26,1);
    write_u32(m,actor+8,~0u); write_u32(m,actor+0x20,1);
    for (auto [offset,value] : {std::pair{0xE0u,bike_address}, {0xE4u,rider_address},
             {0xE8u,status}, {0xECu,course_projection}, {0xF0u,projection},
             {0xF8u,profile}, {0x104u,ai}}) write_u32(m,actor+offset,value);
    write_u32(m,bike_address+0x800,rider_address); write_u32(m,rider_address+0x584,bike_address);
    write_u16(m,bike_address+0x7F8,1); write_u16(m,rider_address+0x57C,1);
    write_float(m,bike_address+0x16C,15); write_float(m,rider_address+0x8C,15);
    write_float(m,rider_address+0x140,1); write_float(m,projection+0x18,1);
    write_float(m,bike_address+0xE0,1); // Native bike lateral normal at +DC.
    write_u32(m,status,12); write_u32(m,course_projection,12);
    write_u32(m,stack+0x4C,12); write_u32(m,0x800A21C8,cache);
    write_u32(m,0x800A21C0,32); write_u32(m,0x800A21C4,33);
    write_u32(m,0x800A6544,donor); write_u32(m,0x800A6540,33);
    write_u32(m,0x800D7628,30); write_u16(m,0x800D767A,1);
    // Native projection's scalar math constants and admission thresholds.
    // The fixture authors straight curves, not extracted game geometry.
    for (unsigned p : {0xD00u,0xD38u,0xE20u,0xE24u,0xE48u}) write_float(m,0x80000000+p,1);
    for (auto [p,value] : {std::pair{0xE34u,4.f},{0xE38u,.5f},{0xE3Cu,-.5f},
             {0xE4Cu,.001f},{0xE54u,.5f},{0x4D50u,-2.f},{0x4D54u,100.f},
             {0x5D34u,80.f},{0x5D38u,-.15f}}) write_float(m,0x80000000+p,value);
    for (unsigned i=0;i<33;++i) {
        const unsigned bits=std::bit_cast<unsigned>(float(i)*5);
        for (unsigned b=0;b<4;++b) records[i*16+8+b]=static_cast<unsigned char>(bits>>(24-b*8));
        for (unsigned b=0;b<16;++b) write_s8(m,donor+i*16+b,records[i*16+b]);
        if (i==32) break;
        for (unsigned point=0;point<3;++point) write_float(m,cache+i*80+point*8,float(i+point)*5);
        write_u32(m,cache+i*80+0x18,i);
        write_float(m,cache+i*80+0x1C,0); write_float(m,cache+i*80+0x20,1);
    }
    route=experimental_course::RouteData{};
    route.records_be=records.data(); route.byte_count=unsigned(records.size());
    route.record_count=33; route.wrap_segment=30;
    route.record_heights=heights.data(); route.height_count=unsigned(heights.size());
}
void planner_checks() {
    const auto floor=build_surface_world(std::array{
        Triangle{1,{{{-100,-100,0},{100,-100,0},{100,100,0}}}},
        Triangle{2,{{{-100,-100,0},{100,100,0},{-100,100,0}}}}});
    const Sphere sphere{{0,0,1},.3f};
    for (float direction : {-1.f,1.f}) {
        std::vector<Triangle> triangles;
        unsigned id=1;
        for (float r : {44.f,56.f}) for (int i=-20;i<120;++i) {
            const float a=i*.01f,b=(i+1)*.01f;
            Vec p{r*std::sin(a),direction*(50-r*std::cos(a)),-5};
            Vec q{r*std::sin(b),direction*(50-r*std::cos(b)),-5};
            Vec ptop=p,qtop=q; ptop[2]=qtop[2]=5;
            triangles.push_back({id++,{p,q,ptop}});
            triangles.push_back({id++,{q,qtop,ptop}});
        }
        const auto walls=build_world(triangles);
        const auto query=[&](float rate) {
            return course_ai::choose(walls.get(),floor.get(),nullptr,{},std::span(&sphere,1),
                {25,0,0},{1,0,0},{0,direction,0},10,-6,6,0,rate);
        };
        const auto old=query(0), turn=query(direction*.5f);
        check(old.brake,"straight forecast falsely brakes on a clear bend");
        check(!turn.brake&&!turn.steer,"bounded curve forecast keeps clear bend free");
        check(query(NAN).brake,"nonfinite turn keeps conservative straight forecast");
        // A real obstruction still wins over route guidance.
        triangles.push_back({id++,{{{8,-100,-5},{8,100,-5},{8,0,100}}}});
        const auto blocked=build_world(triangles);
        const auto hit=course_ai::choose(blocked.get(),floor.get(),nullptr,{},std::span(&sphere,1),
            {25,0,0},{1,0,0},{0,direction,0},10,-6,6,0,direction*.5f);
        check(hit.brake&&!hit.steer,"solid wall cannot be bypassed by curve forecast");
    }
}
}

namespace rr64::experimental_course {
bool active() noexcept { return enabled; }
const RouteData *route_data() noexcept { return &route; }
}
namespace rr64::netplay { PhysicsRules get_physics_rules() { return rules; } }
namespace rr64::course_walls {
const World *world() noexcept { return nullptr; }
const World *surface_world() noexcept { return nullptr; }
}
namespace rr64::course_hazards {
const Data *data() noexcept { return nullptr; }
netplay::CourseHazardState capture_state() noexcept { return {}; }
DynamicMotion resolve(std::span<const Sphere>,Vec,Vec,float,bool) noexcept {
    check(false,"unexpected physical hazard publication"); return {};
}
void notify_hit(unsigned,Vec) noexcept { check(false,"AI query published a hazard hit"); }
}
namespace rr64::course_impact {
Result apply(unsigned char*,const recomp_context&,unsigned,Body,const Contact&) {
    check(false,"unexpected physical impact"); return {};
}
}
namespace rr64::course_guardrail {
Result apply(unsigned char*,const recomp_context&,unsigned,const Contact&) {
    check(false,"unexpected physical rail adapter"); return {};
}
Eligibility eligibility(unsigned char*,unsigned,const Contact&) noexcept {
    check(false,"unexpected physical rail eligibility"); return Eligibility::Blocked;
}
}
namespace rr64::race_end_trace {
void observe_course_wall(unsigned char*,unsigned,unsigned,unsigned,unsigned,unsigned,unsigned,
                         const float*,const float*,const float*,float) { check(false,"wall publication"); }
void observe_course_wall_context(unsigned char*,unsigned,unsigned,const CourseWallContext&) {
    check(false,"wall context publication");
}
}
extern "C" {
void test_course_ai_projection(unsigned char*,recomp_context*);
void test_course_ai_projection_before(unsigned char*,recomp_context*);
void func_8005A9DC(unsigned char*,recomp_context*);
void func_8004E754(unsigned char*,recomp_context*) { check(false,"physical wall adapter"); }
int rr64_mk64_items_ghost(unsigned char*,unsigned) { check(false,"physical item wall bypass"); return 0; }
void do_break(unsigned) { check(false,"native break"); }
}

int main() {
    planner_checks();
    std::vector<unsigned char> memory(kRdramSize);
    auto *m=memory.data(); seed(m);
    // Native search can accept a distant curve, but its following projection
    // rejects the bike behind it. A failed projection must also be recoverable.
    auto search=context(); search.f_odd=&search.f0.u32h; search.r4=guest_address(actor);
    func_8005A9DC(m,&search);
    check(unsigned(search.r2)!=~0u,"forward-facing native search accepts remote curve");
    write_u32(m,stack+0x4C,unsigned(search.r2));
    auto before=memory; auto original=context(); original.f_odd=&original.f0.u32h;
    test_course_ai_projection_before(before.data(),&original);
    check(original.r2==0,"negative control reproduces native projection failure");
    auto corrected=context(); corrected.f_odd=&corrected.f0.u32h;
    test_course_ai_projection(m,&corrected);
    check(corrected.r2==1,"native retry projects nearest physical curve");
    check(word(m,course_projection)==2,"retry chooses nearby authored curve");
    check(word(m,status)==12,"recovery does not award or rewind race progress");
    check(std::memcmp(m+bike_address-kRdramBegin,before.data()+bike_address-kRdramBegin,bike::stride)==0,
          "recovery preserves bike physics");
    check(std::memcmp(m+rider_address-kRdramBegin,before.data()+rider_address-kRdramBegin,rider::stride)==0,
          "recovery preserves rider physics");
    for (unsigned guard=0;guard<17;++guard) {
        memory.assign(kRdramSize,0); m=memory.data(); seed(m);
        auto c=context(); c.r2=0;
        switch (guard) {
        case 0: enabled=false; break;
        case 1: write_u32(m,actor+8,0); break;
        case 2: write_u16(m,actor+0x26,0); break;
        case 3: write_u16(m,bike_address+0x7F8,0); break;
        case 4: write_u16(m,rider_address+0x57E,1); break;
        case 5: write_u16(m,status+0x4E,1); break;
        case 6: write_u16(m,globals::gameplay_pause_state,1); break;
        case 7: prediction::replay_active=true; break;
        case 8: rules.active=true; break;
        case 9: rules.active=rules.connected=rules.authoritative=rules.is_host=true;
                rules.phase=netplay::Phase::Race; rules.local_slot=0; rules.authority_humans=2; break;
        case 10: c.r2=1; break;
        case 11: route.records_be=nullptr; break;
        case 12: c.r18=0; break;
        case 13: write_u32(m,cache+2*80+0x18,0); break;
        case 14: write_u32(m,0x800A21C8,0); break;
        case 15: write_u32(m,globals::main_mode,32); break;
        case 16: write_float(m,rider_address+0x90,1000); break;
        }
        const auto prior=memory; const auto registers=c;
        rr64_course_ai_retry_projection(m,&c);
        check(memory==prior,"excluded retry preserves guest memory");
        check(std::memcmp(&c,&registers,sizeof c)==0,"excluded retry preserves registers");
    }
    std::printf("Course AI: %u checks passed\n",checks);
}
