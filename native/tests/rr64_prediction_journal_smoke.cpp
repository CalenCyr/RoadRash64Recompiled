#include "rr64_prediction_journal.hpp"
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <algorithm>
void check(bool b){if(!b){std::fputs("prediction journal check failed\n",stderr);std::exit(1);}}
int main(){
 using namespace rr64;
 const auto size=engine::kRdramSize;
 std::vector<unsigned char> memory(size),restored(size,0xff);
 prediction::GuestJournal journal;journal.reset(7);
 check(journal.capture(7,0,memory.data(),size));check(journal.data_bytes()==size);
 check(journal.contains(7,0) && !journal.contains(8,0) && !journal.contains(7,1));
 const auto start=std::chrono::steady_clock::now();
 for(unsigned seq=1;seq<=256;++seq){memory[(seq-1)*4096]=static_cast<unsigned char>(seq);check(journal.capture(7,seq,memory.data(),size));}
 const auto elapsed=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count();
 check(journal.frames()==257 && journal.data_bytes()<size+256*4096+1);
 check(!journal.capture(7,257,memory.data(),size)); // no unacknowledged eviction
 check(journal.restore(7,100,restored.data(),size));
 for(unsigned p=0;p<256;++p)check(restored[p*4096]==(p<100?static_cast<unsigned char>(p+1):0));
 auto untouched=restored;check(!journal.restore(8,100,restored.data(),size) && restored==untouched);
 check(!journal.retire_before(7,257));check(journal.retire_before(7,100));
 check(journal.frames()==157 && !journal.restore(7,99,restored.data(),size));
 check(!journal.contains(7,99) && journal.contains(7,100) && journal.contains(7,256));
 check(journal.capture(7,257,memory.data(),size));
 check(journal.retire_before(7,257));check(journal.data_bytes()==size);
 check(journal.restore(7,257,restored.data(),size) && restored==memory);
 prediction::GuestJournal bounded(size);bounded.reset(9);
 check(bounded.capture(9,0,memory.data(),size));memory[1]=99;
 check(!bounded.capture(9,1,memory.data(),size));check(bounded.frames()==1 && bounded.data_bytes()==size);
 check(bounded.restore(9,0,restored.data(),size) && restored[1]==0);
 bounded.reset(10);check(bounded.data_bytes()==0 && !bounded.restore(9,0,restored.data(),size));
 // Replay produces a separate branch rooted at the acknowledged command.
 // The live branch stays intact until its replay ticket has been accepted.
 prediction::GuestJournal replacement;replacement.reset(7,257);
 check(!replacement.capture(7,0,memory.data(),size));
 memory[2]=42;check(replacement.capture(7,257,memory.data(),size));
 memory[3]=43;check(replacement.capture(7,258,memory.data(),size));
 check(!journal.restore(7,258,restored.data(),size));
 journal.swap(replacement);
 check(journal.restore(7,258,restored.data(),size) && restored==memory);
 check(replacement.restore(7,257,restored.data(),size) && restored[2]==0);
 replacement.reset(0);check(replacement.data_bytes()==0);
 // A dense workload must exhaust the explicit budget without corrupting the
 // prior state. Sparse timing above is not a worst-case performance estimate.
 prediction::GuestJournal dense(size*2);dense.reset(11,400);
 std::fill(memory.begin(),memory.end(),1);check(dense.capture(11,400,memory.data(),size));
 std::fill(memory.begin(),memory.end(),2);check(dense.capture(11,401,memory.data(),size));
 std::fill(memory.begin(),memory.end(),3);check(!dense.capture(11,402,memory.data(),size));
 check(dense.frames()==2 && dense.data_bytes()==size*2);
 check(dense.restore(11,401,restored.data(),size));
 check(std::all_of(restored.begin(),restored.end(),[](auto b){return b==2;}));
 check(dense.retire_before(11,401) && dense.capture(11,402,memory.data(),size));
 dense.reset(12,UINT32_MAX);check(dense.capture(12,UINT32_MAX,memory.data(),size));
 check(!dense.capture(12,0,memory.data(),size));
 std::printf("guest journal: 256 sparse steps, %.3f ms total, %.3f ms/capture; bounded retention checks passed (synthetic workload)\n",elapsed,elapsed/256);
}
