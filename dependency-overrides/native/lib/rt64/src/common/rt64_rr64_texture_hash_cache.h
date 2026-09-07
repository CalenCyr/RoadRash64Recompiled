#pragma once
#include <array>
#include <memory>
#include <cstring>
#include <algorithm>
#include "xxHash/xxh3.h"
namespace RT64 {
struct RR64TextureHashCache {
 struct Entry {std::array<uint32_t,9> key{};std::array<uint8_t,4096> bytes{};uint64_t hash=0;bool valid=false;};
 std::unique_ptr<Entry[]> entries=std::make_unique<Entry[]>(2048);uint64_t hits=0,misses=0,fallbacks=0;
 template<class Original> uint64_t hash(const uint8_t*mem,const RT64::LoadTile&t,uint16_t w,uint16_t h,uint32_t tlut,uint32_t version,Original&& original){
  if(version!=5){fallbacks++;return original();}
  // Certified contiguous hashing only. Compare all sampled texture bytes and
  // the entire applicable palette; unused TMEM cannot change the result.
  const uint32_t size=tlut?2048:4096,start=(t.tmem*8)&(size-1),row=std::max(uint32_t(w)<<t.siz>>1,1u),stride=t.line*8;
  const uint64_t n=uint64_t(stride)*(h?h-1:0)+row;
  if(!w||!h||(t.siz==3&&t.fmt==0)||stride>row||n>size-start){fallbacks++;return original();}
  const uint32_t pal=tlut?(t.siz==0?2048+t.palette*128:2048):0,palBytes=tlut?(t.siz==0?128:2048):0;
  if(palBytes&&pal+palBytes>4096){fallbacks++;return original();}
  std::array<uint32_t,9> key={t.line,t.tmem,t.siz,t.fmt,t.palette,w,h,tlut,version};
  auto slot=(XXH3_64bits(mem+start,std::min(uint64_t(64),n))^XXH3_64bits(mem+pal,std::min(64u,palBytes))^XXH3_64bits(key.data(),sizeof(key)))&2047;
  auto&e=entries[slot];if(e.valid&&e.key==key&&std::memcmp(mem+start,e.bytes.data(),size_t(n))==0&&(!palBytes||std::memcmp(mem+pal,e.bytes.data()+2048,palBytes)==0)){hits++;return e.hash;}
  misses++;e.valid=true;e.key=key;std::memcpy(e.bytes.data(),mem+start,size_t(n));if(palBytes)std::memcpy(e.bytes.data()+2048,mem+pal,palBytes);e.hash=original();return e.hash;
 }
};

} // namespace RT64

