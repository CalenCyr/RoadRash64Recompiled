#include "rr64_prediction_terrain_probe.hpp"
#include "rr64_prediction_dependency_probe.hpp"
#include "rr64_prediction_streaming.hpp"
#include <cstdlib>
static void check(bool v){if(!v)std::abort();}
int main(){
    using namespace rr64::prediction;
    using namespace rr64::engine;
    std::vector<unsigned char> m(kRdramSize);
    recomp_context stream_context{};stream_context.f_odd=&stream_context.f0.u32h;
    stream_context.r4=3;stream_context.r29=guest_address(0x807f0000);
    StreamingInput stream;
    write_float(m.data(),0x800a771c,0.75f);
    write_u32(m.data(),0x800a7720,17);write_u32(m.data(),0x800a7724,21);
    write_float(m.data(),0x8009dbac,2.f);write_float(m.data(),0x8009dbcc,3.f);
    write_u32(m.data(),0x800a1830,99);
    check(stream.capture(m.data(),stream_context));
    const auto stream_words=stream.words;
    auto private_image=m;write_u32(private_image.data(),0x800a1830,0);
    check(stream.restore(private_image.data()) && private_image==m);
    stream_context.r4=4;check(!stream.capture(m.data(),stream_context) && stream.words==stream_words);
    stream_context.r4=0;write_u32(m.data(),0x8009dbac,0x7fc00000);
    check(!stream.capture(m.data(),stream_context) && stream.words==stream_words);
    auto invalid_stream=stream;invalid_stream.words[0]=0x7fc00000;
    const auto untouched=private_image;
    check(!invalid_stream.restore(private_image.data()) && private_image==untouched);
    constexpr unsigned table=0x80300000,payload=0x80400000;
    write_u32(m.data(),0x800dea8c,2);write_u32(m.data(),0x800dacd0,100);
    write_u32(m.data(),0x800ddea4,table);
    for(unsigned i=0;i<4;++i){
        write_u32(m.data(),table+i*16,payload+i*0x100);
        // State byte is the high byte of this big-endian guest word.
        write_u32(m.data(),table+i*16+12,0x05000000);
    }
    TerrainProbe p;p.capture(m.data());check(p.valid && p.cells.size()==4);
    const auto before=m;
    p.query(m.data(),1,0,42);check(p.queries==1 && !p.different && before==m);
    // Live snapshot remains immutable while replay mutates or reuses a payload.
    write_u32(m.data(),table+32+12,0x01000000);
    p.query(m.data(),1,0,42);p.query(m.data(),1,0,42);
    check(p.different==2 && p.stored==1 && p.differences[0].live.state==5 && p.differences[0].replay.state==1);
    m=before;write_u32(m.data(),payload,123);
    p.query(m.data(),0,0,43);check(p.stored==2); // Same address, changed header.
    m=before;write_u32(m.data(),table+8,99);
    p.query(m.data(),0,0,43);check(p.epoch_only==1 && p.stored==2);
    p.query(m.data(),2,0,43);check(p.invalid==1);
    write_u32(m.data(),0x800dea8c,3);p.query(m.data(),0,0,43);check(p.invalid==2);
    m=before;
    {TerrainProbeScope scope(p);TerrainBikeScope bike(77);
        check(terrain_probe==&p && terrain_probe_bike==77);
        try{TerrainProbe q;TerrainProbeScope nested(q);TerrainBikeScope b(88);throw 1;}catch(int){}
        check(terrain_probe==&p && terrain_probe_bike==77);
    }
    check(!terrain_probe && !terrain_probe_bike);
    for(unsigned i=0;i<100;++i){write_u32(m.data(),payload,i+1);p.query(m.data(),0,0,i);}
    check(p.stored==64 && p.different>64);
    write_u32(m.data(),0x800ddea4,0x807ffff8);p.capture(m.data());check(!p.valid && p.cells.empty());
    write_u32(m.data(),0x800dea8c,129);p.capture(m.data());check(!p.valid);
    p.capture(nullptr);check(!p.valid);
    m=before;auto live=m;
    // Distinguish a preexisting private dependency from an in-update load.
    write_u32(m.data(),table+12,0x01000000);
    TerrainProbe phases;phases.capture(live.data(),m.data());
    check(phases.valid && phases.cells[0].state==5 && phases.replay_cells[0].state==1);
    phases.query(m.data(),0,0,1);
    check(phases.stored==1 && phases.differences[0].entry==phases.differences[0].replay);
    write_u32(m.data(),table+12,0x02000000);
    phases.query(m.data(),0,0,1);
    check(phases.stored==2 && phases.differences[1].entry.state==1 && phases.differences[1].replay.state==2);
    write_u32(m.data(),0x800dacd0,101);
    phases.capture(live.data(),m.data());check(!phases.valid && phases.cells.empty());
    m=before;
    write_u32(live.data(),payload,456);
    write_u32(live.data(),payload+4,789);
    constexpr unsigned stack=0x807f0000;
    write_u32(live.data(),stack-16,1);
    DependencyProbe d;d.capture(live.data(),m.data(),stack);
    check(d.valid && d.words.size()==2);
    d.read(m.data(),payload+3,2,0x80012340);check(d.hits==1 && d.stored==1);
    d.read(m.data(),payload+3,2,0x80012340);check(d.hits==2 && d.stored==1);
    // A changed private word is no longer an unchanged entry dependency.
    write_u32(m.data(),payload,456);write_u32(m.data(),payload+4,789);
    d.read(m.data(),payload,8,0x80012344);check(d.hits==2);
    d.read(m.data(),0x807fffff,8,0x80012348);check(d.hits==2);
    d.read(m.data(),stack-16,4,0x8001234c);check(d.hits==2);
    {DependencyProbeScope scope(d);
        try{DependencyProbe nested;DependencyProbeScope s(nested);throw 1;}catch(int){}
        check(dependency_probe==&d);
    }
    check(!dependency_probe);
    m=before;
    for(unsigned i=0;i<150;++i)d.read(m.data(),payload,4,0x80010000+i*4);
    check(d.stored==128 && d.hits==152);
    d.capture(nullptr,m.data(),stack);check(!d.valid && d.words.empty());
    std::puts("Terrain probe snapshot, reuse, bounds, retention and scope checks passed");
}
