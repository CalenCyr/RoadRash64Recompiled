#include "rr64_world_sync.hpp"
#include <cstdlib>
#include <limits>
using namespace rr64::world_sync;
void check(bool b){if(!b)std::abort();}
Batch packet(unsigned index,unsigned tick=1,unsigned round=1) {
 Batch b{};b.round=round;b.tick=tick;b.index=index;
 for(unsigned i=0;i<batch_size && index*batch_size+i<capacity;++i){auto &v=b.traffic[i];v.active=1;v.id=index*batch_size+i;v.model=0xD8;v.kind=1;v.position[0]=v.id*10.f;v.motion_valid=1;v.motion[0]=v.position[0];}
 return b;
}
int main(){
 Receiver r;
 for(unsigned i=batches-1;i>0;--i)check(!r.accept(packet(i),1));
 check(!r.snapshot().tick);check(r.accept(packet(0),1));
 check(r.snapshot().traffic[19].position[0]==190 && r.snapshot().traffic[19].motion[0]==190);
 check(!r.accept(packet(0),1));
 for(unsigned i=0;i<batches-1;++i)check(!r.accept(packet(i,2),1));
 auto bad=packet(batches-1,2);bad.traffic[1].motion[5]=std::numeric_limits<float>::quiet_NaN();
 check(!r.accept(bad,1));check(r.snapshot().tick==1);
 for(unsigned i=0;i<batches;++i){auto empty=packet(i,3);empty.traffic={};check(r.accept(empty,1)==(i==batches-1));}
 check(r.snapshot().tick==3 && !r.snapshot().traffic[0].active);
 check(!r.accept(packet(0,2),1));check(!r.accept(packet(0,4,2),1));check(!r.accept(packet(batches,4),1));
 r.reset();
 for(unsigned i=0;i<batches;++i){auto dup=packet(i);if(i==batches-1)dup.traffic[1].id=0;check(!r.accept(dup,1));}
 check(!r.snapshot().tick);
 for(unsigned i=0;i<batches;++i)check(r.accept(packet(i,1,2),2)==(i==batches-1));
 check(r.snapshot().round==2);
 // Newer partial frames do not starve an earlier complete one under jitter.
 r.reset();
 for(unsigned i=0;i<batches-1;++i){check(!r.accept(packet(i,10),1));check(!r.accept(packet(i,11),1));}
 check(r.accept(packet(batches-1,10),1));check(r.snapshot().tick==10);
 check(r.accept(packet(batches-1,11),1));check(r.snapshot().tick==11);
 check(!r.accept(packet(0,10),1));
 // Bounded history evicts incomplete old frames but newer full frames recover.
 r.reset();for(unsigned tick=1;tick<=20;++tick)check(!r.accept(packet(0,tick),1));
 check(!r.accept(packet(1,1),1));
 for(unsigned i=1;i<batches;++i)check(r.accept(packet(i,20),1)==(i==batches-1));
 check(r.snapshot().tick==20);
 auto special=packet(0).traffic[0];special.model=0x125;check(valid(special));
 special.model=0x126;check(valid(special));special.model=0x124;check(!valid(special));
 auto padding=packet(batches-1,21);padding.traffic[2]=packet(0).traffic[0];
 check(!r.accept(padding,1));check(r.snapshot().tick==20);
}
