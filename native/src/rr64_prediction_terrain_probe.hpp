#pragma once
#include "rr64_engine_layout.hpp"
#include <array>
#include <cstdio>
#include <vector>

namespace rr64::prediction {
// Read-only verifier evidence. Snapshot descriptors at the live frame boundary;
// compare only cells actually queried by the disposable native update. Nothing
// here imports live terrain into replay or treats an address/hash as identity.
struct TerrainDescriptor {
    unsigned payload=0,epoch=0,header_hash=0;
    std::uint8_t state=0;
    bool valid=false,header_valid=false;
    bool operator==(const TerrainDescriptor&) const = default;
};
inline TerrainDescriptor terrain_descriptor(unsigned char *m,unsigned address){
    TerrainDescriptor d;
    if(!m || !engine::valid_guest_range(address,16))return d;
    d.valid=engine::read_u32(m,address,d.payload) &&
        engine::read_u32(m,address+8,d.epoch) && engine::read_u8(m,address+12,d.state);
    if(d.valid && d.state==5 && engine::valid_guest_range(d.payload,0x58)){
        d.header_valid=true;d.header_hash=2166136261u;
        for(unsigned off=0;off<0x58;off+=4){
            unsigned word=0;engine::read_u32(m,d.payload+off,word);
            d.header_hash=(d.header_hash^word)*16777619u;
        }
    }
    return d;
}
struct TerrainProbe {
    struct Difference {unsigned index=0,bike=0;TerrainDescriptor live,replay,entry;};
    unsigned width=0,table=0,cell_size=0,queries=0,different=0,epoch_only=0,invalid=0,stored=0;
    bool valid=false;
    std::vector<TerrainDescriptor> cells;
    std::vector<TerrainDescriptor> replay_cells;
    std::array<Difference,64> differences{};
    void capture(unsigned char *m,unsigned char *private_memory=nullptr){
        *this=TerrainProbe{};
        if(!m || !engine::read_u32(m,0x800dea8c,width) || !width || width>128 ||
           !engine::read_u32(m,0x800dacd0,cell_size) || !cell_size ||
           !engine::read_u32(m,0x800ddea4,table) ||
           !engine::valid_guest_range(table,width*width*16))return;
        if(!private_memory)private_memory=m;
        unsigned private_width=0,private_size=0,private_table=0;
        if(!engine::read_u32(private_memory,0x800dea8c,private_width) || private_width!=width ||
           !engine::read_u32(private_memory,0x800dacd0,private_size) || private_size!=cell_size ||
           !engine::read_u32(private_memory,0x800ddea4,private_table) ||
           !engine::valid_guest_range(private_table,width*width*16))return;
        cells.reserve(width*width);replay_cells.reserve(width*width);
        for(unsigned i=0;i<width*width;++i){
            cells.push_back(terrain_descriptor(m,table+i*16));
            replay_cells.push_back(terrain_descriptor(private_memory,private_table+i*16));
        }
        valid=true;
    }
    void query(unsigned char *m,unsigned x,unsigned y,unsigned bike){
        ++queries;
        unsigned rw=0,rt=0,rs=0;
        if(!valid || !m || !engine::read_u32(m,0x800dea8c,rw) || rw!=width ||
           !engine::read_u32(m,0x800dacd0,rs) || rs!=cell_size ||
           !engine::read_u32(m,0x800ddea4,rt) || x>=width || y>=width ||
           !engine::valid_guest_range(rt,width*width*16)){++invalid;return;}
        const unsigned index=x*width+y;
        const auto replay=terrain_descriptor(m,rt+index*16);
        if(replay==cells[index])return;
        auto without_epoch=replay;without_epoch.epoch=cells[index].epoch;
        if(without_epoch==cells[index]){++epoch_only;return;}
        ++different;
        for(unsigned i=0;i<stored;++i)
            if(differences[i].index==index && differences[i].bike==bike && differences[i].replay==replay)return;
        if(stored<differences.size())differences[stored++]={index,bike,cells[index],replay,replay_cells[index]};
    }
    void report() const {
        std::fprintf(stderr,"[RR64-REPLAY-TERRAIN] valid=%u queries=%u different=%u epoch-only=%u invalid=%u stored=%u width=%u\n",
            valid,queries,different,epoch_only,invalid,stored,width);
        for(unsigned i=0;i<stored;++i){const auto &d=differences[i];
            std::fprintf(stderr,"[RR64-REPLAY-TERRAIN-CELL] cell=%u bike=%08x state=%u/%u payload=%08x/%08x epoch=%u/%u header-valid=%u/%u header-hash=%08x/%08x\n",
                d.index,d.bike,d.live.state,d.replay.state,d.live.payload,d.replay.payload,
                d.live.epoch,d.replay.epoch,d.live.header_valid,d.replay.header_valid,d.live.header_hash,d.replay.header_hash);
            // Three phases prevent a load performed during replay being
            // misreported as evidence of already-stale entry residency.
            auto entry=d.entry,query=d.replay;
            entry.epoch=d.live.epoch;query.epoch=d.live.epoch;
            std::fprintf(stderr,"[RR64-REPLAY-TERRAIN-PHASE] cell=%u bike=%08x entry-different=%u changed-during-replay=%u private-entry-state=%u private-entry-payload=%08x private-entry-header=%08x\n",
                d.index,d.bike,entry!=d.live,query!=entry,d.entry.state,d.entry.payload,d.entry.header_hash);
        }
    }
};
inline thread_local TerrainProbe *terrain_probe=nullptr;
inline thread_local unsigned terrain_probe_bike=0;
struct TerrainProbeScope {
    TerrainProbe *previous=terrain_probe;
    explicit TerrainProbeScope(TerrainProbe &p){terrain_probe=&p;}
    ~TerrainProbeScope(){terrain_probe=previous;}
    TerrainProbeScope(const TerrainProbeScope&)=delete;
};
struct TerrainBikeScope {
    unsigned previous=terrain_probe_bike;
    explicit TerrainBikeScope(unsigned bike){terrain_probe_bike=bike;}
    ~TerrainBikeScope(){terrain_probe_bike=previous;}
};
inline void probe_terrain_query(unsigned char *m,unsigned x,unsigned y){
    if(terrain_probe)terrain_probe->query(m,x,y,terrain_probe_bike);
}
}
