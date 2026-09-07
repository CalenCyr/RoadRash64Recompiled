#pragma once
#include <array>
#include <cstdint>
#include <cstring>
#include <memory>
// Producer-owned fixed storage. Exact source validation permits animated data
// and address reuse. Invoked only during nondeferred full-sync replay.
namespace RT64 {
struct RR64TextureLoadCache {
 struct Entry {uint32_t address=~0u,bytes=0,dxt=0;bool palette=false;std::array<uint8_t,4096> input{},output{};};
 std::unique_ptr<Entry[]> entries=std::make_unique<Entry[]>(2048);
 uint64_t hits=0,misses=0,fallbacks=0;
 template<class R,class O> void load(R&r,const O&o,const uint8_t*ram){
  auto original=[&](){switch(o.type){case O::Type::Block:r.loadBlockOperation(o.tile,o.texture,false);break;case O::Type::Tile:r.loadTileOperation(o.tile,o.texture,false);break;case O::Type::TLUT:r.loadTLUTOperation(o.tile,o.texture,false);break;}};
  const auto&t=o.tile;const auto&s=o.texture;bool pal=o.type==O::Type::TLUT,block=o.type==O::Type::Block;
  if((!pal&&!block)||(t.siz==3&&t.fmt==0)||t.line!=0||(!block&&(t.lrt>>2)!=(t.ult>>2))||t.siz>3||t.lrs<t.uls){fallbacks++;original();return;}
  uint32_t n=pal?((t.lrs>>2)-(t.uls>>2)+1):((t.lrs-t.uls)>>(4-t.siz))+1;
  uint32_t start=s.address+(((block?t.uls:t.uls>>2)<<s.siz)>>1)+((s.width<<s.siz)>>1)*(block?t.ult:t.ult>>2);
  uint32_t dest=t.tmem<<3,bytes=n*(pal?2:8),out=n*8,dxt=block?t.lrt:0;
  if(!n||n>512||(start&3)||(bytes&3)||dest>4096||out>4096-dest){fallbacks++;original();return;}
  uint32_t slot=((start>>4)*2654435761u ^ bytes*97u ^ dxt*31u ^ unsigned(pal))&2047;
  auto&e=entries[slot];auto*mem=reinterpret_cast<uint8_t*>(r.TMEM)+dest;
  if(e.address==start&&e.bytes==bytes&&e.dxt==dxt&&e.palette==pal&&std::memcmp(e.input.data(),ram+start,bytes)==0){std::memcpy(mem,e.output.data(),out);hits++;return;}
  original();e.address=start;e.bytes=bytes;e.dxt=dxt;e.palette=pal;std::memcpy(e.input.data(),ram+start,bytes);std::memcpy(e.output.data(),mem,out);misses++;
 }
};

} // namespace RT64

