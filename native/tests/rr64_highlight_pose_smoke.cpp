#include "rr64_highlight_pose.hpp"
#include "rr64_actor_pose.hpp"
#include "rr64_highlight_network.hpp"
#include <cstdlib>
#include <algorithm>
#include <bit>
#include <iostream>
#include <memory>
#include <source_location>
#include <vector>
#include <zstd.h>

using namespace rr64;
using namespace rr64::engine;
using namespace rr64::highlights;
namespace {
unsigned checks=0;
void check(bool value,const char *message,const std::source_location where=std::source_location::current()) {
    ++checks;
    if(!value){std::cerr<<where.line()<<": "<<message<<'\n';std::exit(1);}
}
constexpr unsigned node=0x80100000,entity=0x80200000;
constexpr Vec3 anchor{20,30,40},camera{21,31,41};
constexpr Quaternion rotation{0,0,0,1};
struct Fixture {
    std::vector<unsigned char> memory=std::vector<unsigned char>(kRdramSize);
    std::array<unsigned,5> records{},poses{},sources{};
    Fixture(bool scattered,unsigned kind,unsigned tier,unsigned bank) {
        auto *m=memory.data();
        records=scattered?std::array<unsigned,5>{0x80120200,0x80120000,0x80120400,0x80120100,0x80120300}:
                          std::array<unsigned,5>{0x80110000,0x80110018,0x80110030,0x80110048,0x80110060};
        constexpr std::array<unsigned,5> types{0x13,0x12,0x10,0x12,0x10};
        constexpr std::array<unsigned,5> flags{0x1000,0x2000,0,0x2000,0x4000};
        write_u32(m,node,kind);write_u32(m,node+actor_scene::entity,entity);
        write_u32(m,entity+8,node);write_u32(m,node+actor_scene::lod_models+tier*4,records[0]);
        write_float(m,0x8009dbac+bank*4,bank==1?100.f:10.f);
        for(unsigned i=0;i<records.size();++i) {
            poses[i]=(scattered?0x80520000u:0x80510000u)+i*0x40;
            sources[i]=(scattered?0x80400000u:0x80300000u)+i*0x100;
            write_u16(m,records[i]+4,types[i]);
            const auto delta=i+1<records.size()?static_cast<int>(records[i+1]-records[i])/8:0;
            write_u16(m,records[i]+8,static_cast<std::uint16_t>(delta));
            write_u16(m,records[i]+10,flags[i]);
            write_u32(m,records[i]+12,poses[i]);write_u32(m,records[i]+20,sources[i]);
            write_u32(m,sources[i],types[i]);
            for(unsigned j=0;j<7;++j)write_float(m,poses[i]+j*4,j<3?float(i+j):(j==6?1.f:0.f));
        }
        write_u16(m,sources[0]+0x12,bank);
    }
};
void bind_and_restore(Fixture &guest,unsigned tier,const Pose &pose) {
    const auto before=guest.memory;BindReport report;
    check(begin_actor(guest.memory.data(),node,tier,pose,anchor,rotation,camera,&report),
          "matching resource hierarchy must bind despite independently allocated record spacing");
    for(unsigned i=0;i<pose.count;++i)for(unsigned j=0;j<7;++j) {
        float actual=0;read_float(guest.memory.data(),guest.poses[pose.bones[i].index]+j*4,actual);
        check(actual==pose.bones[i].values[j],"recorded child pose reaches its local ordered bone");
    }
    end_actor();check(guest.memory==before,"every playback write restores exactly");
}
void rejected(Fixture &guest,unsigned tier,const Pose &pose,BindFailure reason) {
    const auto before=guest.memory;BindReport report;
    check(!begin_actor(guest.memory.data(),node,tier,pose,anchor,rotation,camera,&report),"incompatible binding rejected");
    check(report.failure==reason,"precise rejection reason");
    check(guest.memory==before,"rejection writes no guest memory");
}
void relocation() {
    for(unsigned kind:{1u,2u})for(unsigned tier=0;tier<3;++tier)for(unsigned bank:{1u,2u}) {
        Fixture host(false,kind,tier,bank),guest(true,kind,tier,bank);
        Pose recorded,local;
        check(capture_node(host.memory.data(),node,tier,recorded),"host source pose captured");
        check(capture_node(guest.memory.data(),node,tier,local),"guest source pose captured");
        ModelGraphTopologySnapshot a,b;
        check(capture_model_graph_topology(host.memory.data(),host.records[0],a) &&
              capture_model_graph_topology(guest.memory.data(),guest.records[0],b),"valid native linked graphs");
        check(a.topology_hash!=b.topology_hash && !compatible_transform_topology(a,b),
              "fixture reproduces different heap links while retaining strict local graph checks");
        check(a.allocation_hash==b.allocation_hash,"ordered transform allocation is identical");
        bind_and_restore(guest,tier,recorded);
        check(recorded.topology==local.topology,"portable identity ignores graph/pose/resource relocation");
        auto changed=guest;write_u16(changed.memory.data(),changed.records[4]+10,0);
        rejected(changed,tier,recorded,BindFailure::Compatibility); // different hierarchy pop
        changed=guest;write_u32(changed.memory.data(),changed.records[3]+20,changed.sources[1]);
        rejected(changed,tier,recorded,BindFailure::Compatibility); // bone source order changed
        changed=guest;write_u16(changed.memory.data(),changed.records[2]+4,0x11);
        rejected(changed,tier,recorded,BindFailure::Compatibility); // same transform count, different control record
        changed=guest;write_u16(changed.memory.data(),changed.sources[0]+0x12,bank==1?2:1);
        rejected(changed,tier,recorded,BindFailure::Compatibility);
        changed=guest;write_u32(changed.memory.data(),changed.records[1]+12,node);
        rejected(changed,tier,recorded,BindFailure::PoseOverlap);
        changed=guest;write_u32(changed.memory.data(),changed.records[1]+12,0);
        rejected(changed,tier,recorded,BindFailure::PosePointer);
        changed=guest;write_u16(changed.memory.data(),changed.records[1]+8,
            static_cast<std::uint16_t>(static_cast<int>(changed.records[0]-changed.records[1])/8));
        rejected(changed,tier,recorded,BindFailure::Graph); // cyclic runtime links
    }
}
void codec() {
    Fixture host(false,1,0,1),guest(true,1,0,1);
    auto frame=std::make_unique<Frame>();frame->tick=1;frame->time_us=1;
    auto &r=frame->racers[0];r.active=true;r.bike_rotation=rotation;r.rider_rotation=rotation;
    check(capture_node(host.memory.data(),node,0,r.bike_pose),"codec source pose");r.rider_pose=r.bike_pose;
    check(valid_frame(*frame),"codec fixture is a complete valid frame");
    const Clip clip{.slot=0,.score=1,.event_tick=1,.event_time_us=1,.frames=std::span<const Frame>(frame.get(),1)};
    highlight_network::Blob encoded;
    check(highlight_network::encode(std::span(&clip,1),encoded),"real compressed codec accepts portable pose");
    highlight_network::Playlist decoded;
    check(highlight_network::decode(encoded,decoded),"real compressed codec decodes portable pose");
    check(decoded.view().size()==1,"one decoded clip");
    bind_and_restore(guest,0,decoded.view()[0].frames[0].racers[0].bike_pose);
    std::vector<unsigned char> raw(encoded.raw_size);
    check(ZSTD_decompress(raw.data(),raw.size(),encoded.bytes.data(),encoded.bytes.size())==raw.size(),
          "inspect real codec payload for version compatibility test");
    check(raw.size()>4 && raw[3]=='4',"item-aware portable format carries RHL4");
    raw[3]='3'; // Former format without item/effect presentation fields.
    auto obsolete=encoded;obsolete.bytes.resize(ZSTD_compressBound(raw.size()));
    const auto bytes=ZSTD_compress(obsolete.bytes.data(),obsolete.bytes.size(),raw.data(),raw.size(),3);
    check(!ZSTD_isError(bytes),"construct otherwise valid obsolete stream");obsolete.bytes.resize(bytes);
    obsolete.checksum=14695981039346656037ull;
    for(auto byte:obsolete.bytes){obsolete.checksum^=byte;obsolete.checksum*=1099511628211ull;}
    check(!highlight_network::decode(obsolete,decoded),"obsolete pose identity version rejected despite valid compression/checksum");
    check(decoded.view()[0].frames[0].racers[0].bike_pose==r.bike_pose,"obsolete stream leaves accepted playback intact");
}
mk64_items::Snapshot recorded_items() {
    using namespace mk64_items;
    Snapshot s;s.enabled=1;s.clock=120;s.next_generation=64;s.random=0x5a61ff91;
    for(unsigned i=0;i<racer_capacity;++i) {
        auto &r=s.riders[i];r.held=Item::Mushroom;r.charges=1;r.revision=i+1;
        r.star_until=140+i;r.boo_until=180+i;r.shrink_until=220+i;
        r.boost_until=250+i;r.boost_speed=20.f+i;r.hit_until=130+i;r.last_use=100;
        r.event_serial=100+i;r.cue=Cue::Mushroom;r.event_target=i;
    }
    s.riders[0].held=Item::GoldenMushroom;s.riders[0].golden_until=300;
    s.riders[1].held=Item::BananaBunch;s.riders[1].charges=1;s.riders[1].deployed=1;
    s.riders[2].held=Item::TripleRedShell;s.riders[2].charges=1;s.riders[2].deployed=1;
    for(unsigned i=0;i<object_capacity;++i) {
        auto &o=s.objects[i];o.generation=i+1;o.born=110;o.expires=350;
        o.kind=Item::GreenShell;o.mode=ObjectMode::Flying;o.owner=i%14;o.target=no_target;
        o.position={float(i),float(i*2),1};o.velocity={float(i)/2,3,0};o.bounces=i%5;
    }
    s.objects[1].kind=Item::Banana;s.objects[1].mode=ObjectMode::Trailing;s.objects[1].orbit=4;
    s.objects[2].kind=Item::RedShell;s.objects[2].mode=ObjectMode::Orbiting;s.objects[2].orbit=2;
    return s;
}
void item_codec() {
    auto frames=std::make_unique<std::array<Frame,2>>();
    for(unsigned i=0;i<frames->size();++i) {
        auto &f=(*frames)[i];f.tick=i+1;f.time_us=1+i*100000;
        f.items=recorded_items();f.items.clock+=i;
        for(auto &o:f.items.objects){o.position[0]+=i*2;o.velocity[1]+=i*2;}
        check(valid_frame(f),"nonempty item/effect frame validates");
    }
    const Clip clip{.slot=13,.score=3,.event_tick=1,.event_time_us=1,.frames=*frames};
    highlight_network::Blob encoded;highlight_network::Playlist decoded;
    check(highlight_network::encode(std::span(&clip,1),encoded),"RHL4 encodes full item pool and fourteen effect records");
    check(highlight_network::decode(encoded,decoded),"RHL4 decodes full item pool and fourteen effect records");
    for(unsigned i=0;i<2;++i)check(decoded.view()[0].frames[i].items==(*frames)[i].items,
                                 "every field of nonempty recorded item state roundtrips exactly");
    std::vector<unsigned char> raw(encoded.raw_size);
    check(ZSTD_decompress(raw.data(),raw.size(),encoded.bytes.data(),encoded.bytes.size())==raw.size(),
          "decompress actual RHL4 for malformed item tests");
    const std::array<unsigned char,16> marker{1,0,0,0,120,0,0,0,64,0,0,0,0x91,0xff,0x61,0x5a};
    const auto found=std::search(raw.begin(),raw.end(),marker.begin(),marker.end());
    check(found!=raw.end(),"locate item metadata in serialized field stream");
    const auto offset=std::size_t(found-raw.begin());
    for(unsigned fault=0;fault<3;++fault) {
        auto malformed=raw;
        if(fault==0)malformed[offset]=2;
        if(fault==1)malformed[offset+4]=0xff; // Clock beyond recorded effect deadlines.
        if(fault==2)malformed[offset+16]=16; // Out-of-range held item.
        auto bad=encoded;bad.bytes.resize(ZSTD_compressBound(malformed.size()));
        const auto length=ZSTD_compress(bad.bytes.data(),bad.bytes.size(),malformed.data(),malformed.size(),3);
        check(!ZSTD_isError(length),"compress structurally valid malformed item frame");bad.bytes.resize(length);
        bad.checksum=14695981039346656037ull;
        for(auto byte:bad.bytes){bad.checksum^=byte;bad.checksum*=1099511628211ull;}
        check(!highlight_network::decode(bad,decoded),"malformed item state rejected despite valid compression and checksum");
        check(decoded.view()[0].frames[0].items==(*frames)[0].items,
              "malformed replay leaves the previous accepted playlist intact");
    }
    auto output=std::make_unique<Frame>();const auto &a=(*frames)[0];auto &b=(*frames)[1];
    check(interpolate(a,b,50001,*output),"item presentation interpolates valid surrounding samples");
    check(output->items.clock==a.items.clock && output->items.riders==a.items.riders,
          "inventory and effect timers retain preceding authored sample");
    for(unsigned i=0;i<64;++i) {
        check(output->items.objects[i].position[0]==a.items.objects[i].position[0]+1,
              "stable item position blends between authored samples");
        check(output->items.objects[i].velocity[1]==a.items.objects[i].velocity[1]+1,
              "stable item velocity blends between authored samples");
    }
    const auto original=b.items;
    for(unsigned discontinuity=0;discontinuity<7;++discontinuity) {
        b.items=original;auto &o=b.items.objects[0];
        if(discontinuity==0){o.generation=65;b.items.next_generation=65;}
        if(discontinuity==1)o.owner=13;
        if(discontinuity==2)o.kind=mk64_items::Item::RedShell;
        if(discontinuity==3)o.mode=mk64_items::ObjectMode::Resting;
        if(discontinuity==4)o={};
        if(discontinuity==5)o.position[0]=1000;
        if(discontinuity==6)++o.bounces;
        check(interpolate(a,b,50001,*output),"item discontinuity samples remain valid");
        check(output->items.objects[0]==a.items.objects[0],"replacement/release/retirement/teleport retains preceding item sample");
        check(interpolate(a,b,b.time_us,*output) && output->items==b.items,"item discontinuity commits at authored boundary");
    }
    b.items={};check(interpolate(a,b,50001,*output) && output->items==a.items,
                     "disabled item state retires at authored sample, without inventing intermediate effects");
}
}
int main(){relocation();codec();item_codec();std::cout<<"highlight pose smoke passed "<<checks<<" checks\n";}
