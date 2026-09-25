#include "rr64_course_audio.hpp"
#include "rr64_course_hazards.hpp"
#include "rr64_course_items.hpp"
#include "rr64_course_walls.hpp"
#include "rr64_engine_layout.hpp"
#include "rr64_experimental_course.hpp"
#include "rr64_highlights.hpp"
#include "rr64_online_flow.hpp"
#include "rr64_prediction_replay.hpp"
#include <algorithm>
#include <array>
#include <cmath>

namespace rr64::course_audio {
void reset_mixer() noexcept;
namespace {
using Vec=std::array<float,3>;
struct Listener { Vec position{}; unsigned bike=0, canonical=0; float speed=0; };
std::array<unsigned,4096> wood_surfaces{};
unsigned wood_count=0;
bool bridge_roll=false;
netplay::CourseItemState previous_items;
netplay::CourseHazardState previous_hazards;
bool observed=false;
unsigned previous_clock=0;
unsigned word(unsigned char *m,unsigned address) {
    unsigned v=0;engine::read_u32(m,address,v);return v;
}
unsigned half(unsigned char *m,unsigned address) {
    std::uint16_t v=0;engine::read_u16(m,address,v);return v;
}
bool vector(unsigned char *m,unsigned address,Vec &v) {
    for(unsigned i=0;i<3;++i)
        if(!engine::read_float(m,address+4*i,v[i]) || !std::isfinite(v[i]))return false;
    return true;
}
float spatial_gain(Vec p,std::span<const Listener> listeners,float range=90) {
    float best=0;
    for(const auto &l:listeners) {
        float sum=0;for(unsigned i=0;i<3;++i)sum+=(p[i]-l.position[i])*(p[i]-l.position[i]);
        const float t=std::clamp(1.f-std::sqrt(sum)/range,0.f,1.f);
        best=std::max(best,t*t);
    }
    // One mix for the physical audio device. A source visible to multiple
    // split views is not played multiple times at multiplying volume.
    return best;
}
bool boundary(unsigned clock,unsigned before,unsigned period,unsigned phase) {
    return clock>=before && (clock+phase)/period!=(before+phase)/period;
}
}
void set_wood_surfaces(std::span<const std::uint32_t> ids,bool bridge) noexcept {
    wood_count=ids.size()<=wood_surfaces.size()?unsigned(ids.size()):0;
    bridge_roll=bridge;
    std::copy_n(ids.begin(),wood_count,wood_surfaces.begin());
    std::sort(wood_surfaces.begin(),wood_surfaces.begin()+wood_count);
}
void reset_runtime() noexcept {
    reset_mixer();previous_items={};previous_hazards={};previous_clock=0;observed=false;
}
} // namespace rr64::course_audio

extern "C" void rr64_course_audio_step(unsigned char *m) {
    using namespace rr64;
    using namespace course_audio;
    if(prediction::active())return; // Never consume events inside private replay.
    const unsigned mode=word(m,engine::globals::main_mode);
    const auto rules=netplay::get_physics_rules();
    if(!m || !experimental_course::active() || !engine::is_live_race_mode(mode) ||
       rr64_highlights_presenting() ||
       (rules.active && (!rules.connected || rules.phase!=netplay::Phase::Race))) {
        reset_runtime();return;
    }
    if(half(m,engine::globals::gameplay_pause_state)) {set_running(false);return;}
    const auto state=course_items::capture_state();
    const auto hazards=course_hazards::capture_state();
    const auto *data=course_hazards::data();
    const unsigned clock=std::max(state.clock,hazards.clock);
    if(observed && clock<previous_clock)reset_runtime();
    std::array<Listener,4> listener_storage{};
    unsigned listener_count=0;
    const unsigned humans=rules.active?1:std::clamp(word(m,engine::local_race::humans),1u,4u);
    for(unsigned i=0;i<humans;++i) {
        const unsigned viewport=rules.active?(rules.replicated_riders?0:rules.local_slot):i;
        if(viewport>=4)continue;
        const unsigned slot=word(m,0x800A657Cu+viewport*4u);
        if(slot>=14)continue;
        const unsigned canonical=rules.active?online_flow::mapped_slot(slot,rules.local_slot,rules.replicated_riders):slot;
        if(canonical>=netplay::kMaximumPlayers)continue;
        const unsigned actor=0x800D8570u+slot*0x118u;
        const unsigned bike=word(m,actor+0xE0),rider=word(m,actor+0xE4);
        if(!half(m,actor+0x24) || !engine::valid_guest_range(bike,engine::bike::stride) ||
           !engine::valid_guest_range(rider,engine::rider::stride))continue;
        Listener l;l.bike=bike;l.canonical=canonical;
        // Rider body follows the listening player during an eject; do not
        // leave the environment anchored to the abandoned bike.
        const bool attached=half(m,rider+engine::rider::bike_attached)!=0;
        if(!vector(m,attached?bike+engine::bike::body_position:rider+0x8C,l.position))continue;
        Vec velocity;if(vector(m,bike+0x178,velocity))
            l.speed=std::sqrt(velocity[0]*velocity[0]+velocity[1]*velocity[1]);
        listener_storage[listener_count++]=l;
    }
    const std::span<const Listener> listeners(listener_storage.data(),listener_count);
    if(listeners.empty()) {set_running(false);return;}
    set_running(true);
    const auto definitions=course_items::definitions();
    for(unsigned i=0;observed && i<state.count && i<definitions.size();++i) {
        if(state.generation[i]!=previous_items.generation[i] && state.cooldown[i]>=60) {
            course_items::ItemBoxDefinition box;
            if(course_items::posed_definition(definitions[i],hazards,box))
                request(Effect::ItemBreak,.7f*spatial_gain(box.position,listeners,45));
        }
    }
    for(const auto &listener:listeners) {
        const auto &roll=state.roulette[listener.canonical];
        const auto &old=previous_items.roulette[listener.canonical];
        if(roll.phase==1)request(Effect::Roulette,.28f,0x100u+listener.canonical);
        if(observed && roll.phase==2 && (old.phase!=2 || old.generation!=roll.generation))
            request(Effect::ItemChosen,.6f);
        // The wood sound follows actual grounded donor WOOD_BRIDGE faces,
        // not the course name or a shared RR surface material number.
        if(wood_count && listener.speed>.7f &&
           half(m,listener.bike+engine::bike::rider_attached) &&
           !half(m,listener.bike+engine::bike::drive_control_lockout)) {
            const auto *surfaces=course_walls::surface_world();
            Vec start=listener.position;start[2]+=.3f;
            if(surfaces) {
                const auto floor=course_walls::sweep_sphere(*surfaces,{start,.06f},{0,0,-1.1f});
                if(floor.hit && floor.normal[2]>.25f &&
                   std::binary_search(wood_surfaces.begin(),wood_surfaces.begin()+wood_count,floor.triangle_id))
                    request(bridge_roll?Effect::BridgeRoll:Effect::WoodRoll,std::clamp(listener.speed/30.f,0.f,.27f),
                            0x200u+listener.canonical,std::clamp(.6f+listener.speed/35.f,.6f,1.5f));
            }
        }
    }
    if(data && observed)for(unsigned i=0;i<hazards.count && i<data->definitions.size();++i) {
        const auto &definition=data->definitions[i];const auto &pose=hazards.poses[i];
        const auto &old=previous_hazards.poses[i];
        if(!pose.active)continue;
        Effect effect{};bool play=false;float range=90,volume=.55f;
        using Kind=course_hazards::Kind;
        switch(definition.kind) {
        case Kind::Train:
            // Source subtype6 is the locomotive; passenger cars and tender
            // must never duplicate the horn. Shared clock gives both peers
            // the same horn cadence without network audio packets.
            if(definition.subtype==6) {
                play=boundary(clock,previous_clock,300,definition.id*23);
                effect=Effect::TrainWhistle;range=180;volume=.7f;
            }break;
        case Kind::Ferry:
            play=boundary(clock,previous_clock,420,120);effect=(clock/420)%2?Effect::FerryWhistle:Effect::FerryDouble;
            range=200;volume=.7f;break;
        case Kind::Crossing:
            play=pose.model!=definition.model &&
                 (old.model==definition.model || boundary(clock,previous_clock,40,0));
            effect=Effect::CrossingBell;range=100;volume=.32f;break;
        case Kind::Thwomp:
            play=pose.model==3 && old.model!=3;effect=Effect::Impact;range=70;break;
        case Kind::Rock:
            play=old.velocity[2]<-2 && pose.velocity[2]>=0;effect=Effect::Impact;range=70;break;
        case Kind::Penguin:
            play=boundary(clock,previous_clock,150,definition.id*37);effect=Effect::Penguin;range=45;volume=.23f;break;
        case Kind::Mole:
            play=pose.position[2]>definition.position[2]+.2f && old.position[2]<=definition.position[2]+.2f;
            effect=Effect::Mole;range=35;volume=.3f;break;
        default:break;
        }
        if(play)request(effect,volume*spatial_gain(pose.position,listeners,range));
    }
    previous_items=state;previous_hazards=hazards;previous_clock=clock;observed=true;
}
