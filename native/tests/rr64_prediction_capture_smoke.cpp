#include "../src/rr64_prediction_capture.cpp"
#include "../src/rr64_prediction_verify_inputs.hpp"
#include <cstdio>
#include <cstdlib>
namespace { unsigned captures=0;float clock_value=1; }
namespace rr64::netplay {Status get_status(){Status s;s.active=s.connected=s.authoritative=s.is_host=true;s.local_slot=0;s.authority_humans=0x2001;return s;}}
namespace ultramodern::rr64 {
bool copy_guest_snapshot(unsigned char *m,unsigned char *out,std::size_t size){++captures;std::memcpy(out,m,size);return true;}
}
namespace rr64::prediction {
bool capture_manual_eject(ManualEjectState &out){out[0].durability=clock_value;return true;}
bool capture_cop_rules(CopRulesState &out){out.win_started=clock_value;return true;}
bool capture_cop_posts(CopPostsState &out){out[13].cue=clock_value;return true;}
}
void check(bool value){if(!value)std::abort();}
int main(){
 // COP1 rounding belongs to the executing thread. A moved historical entry
 // must restore it privately and preserve the caller even if execution throws.
 std::fenv_t original;check(std::fegetenv(&original)==0);
 for(int rounding : {FE_TONEAREST,FE_DOWNWARD,FE_UPWARD,FE_TOWARDZERO}){
  check(std::fesetround(rounding)==0);
  recomp_context source{};source.f_odd=&source.f0.u32h;
  rr64::prediction::CpuContext captured;check(captured.capture(source));
  auto relocated=captured;
  check(std::fesetround(FE_TONEAREST)==0);
  try {
   rr64::prediction::FloatingPointScope floating(relocated);
   check(floating.valid() && std::fegetround()==rounding);
   check(std::fesetround(FE_UPWARD)==0);
   throw 1;
  }catch(int){}
  check(std::fegetround()==FE_TONEAREST);
 }
 {rr64::prediction::CpuContext invalid;
  rr64::prediction::FloatingPointScope floating(invalid);check(!floating.valid());}
 check(std::fesetenv(&original)==0);
 for(unsigned mode=0;mode<2;++mode){
  rr64::prediction::CpuContext saved;
  recomp_context source{};source.mips3_float_mode=mode;
  source.f_odd=mode?&source.f1.u32l:&source.f0.u32h;
  *source.f_odd=0x12345678;source.r29=0xffffffff807f0000ull;
  check(saved.capture(source));
  auto relocated=saved;source.f_odd=nullptr;source.f0={};source.f1={};
  recomp_context destination{};check(relocated.restore(destination));
  check(destination.f_odd==(mode?&destination.f1.u32l:&destination.f0.u32h));
  check(*destination.f_odd==0x12345678 && destination.r29==0xffffffff807f0000ull);
  *destination.f_odd=42;check(source.f0.u64==0 && source.f1.u64==0);
  rr64::prediction::CpuContext invalid;check(!invalid.capture(source) && !invalid.restore(destination));
 }

 std::vector<unsigned char> m(rr64::engine::kRdramSize),restored(m.size());
 // Omitted entry state is reported by word offset, including block boundaries.
 rr64::engine::write_u32(restored.data(),0x800100fc,1);
 rr64::engine::write_u32(restored.data(),0x80010100,2);
 rr64::engine::write_u32(restored.data(),0x80010864,3);
 auto offsets=rr64::prediction::compare_entry_region(m.data(),restored.data(),0x80010000,0x868,0,"bike");
 check(offsets.valid && offsets.words==3 && offsets.mask[0]==(std::uint64_t(1)<<63) &&
       offsets.mask[1]==1 && offsets.mask[8]==(std::uint64_t(1)<<25));
 check(!rr64::prediction::compare_entry_region(m.data(),restored.data(),0x807ffffc,8,0,"invalid").valid);
 std::fill(restored.begin(),restored.end(),0);
 rr64::authority::NativeTiming timing{};timing.substeps=2;
 for(unsigned i=0;i<timing.bits.size();++i)timing.bits[i]=std::bit_cast<unsigned>(float(i+1)/61.f);
 check(rr64::prediction::restore_timing(m.data(),timing));
 rr64::prediction::UpdateCounters counters{0xffffffffu,0x80000000u,std::bit_cast<unsigned>(0.025f)},captured{};
 check(rr64::prediction::restore_update_counters(m.data(),counters));
 check(rr64::prediction::capture_update_counters(m.data(),captured) && captured==counters);
 check(!rr64::prediction::capture_update_counters(nullptr,captured) && captured==counters);
 const auto before_invalid_clock=m;
 auto invalid_clock=counters;invalid_clock[2]=0x7fc00000;
 check(!rr64::prediction::restore_update_counters(m.data(),invalid_clock) && m==before_invalid_clock);
 // Represent a quiescent resource pool in the historical fixture.
 rr64::engine::write_u16(m.data(),0x8009ce90,32);
 for(unsigned i=0;i<32;++i)rr64::engine::write_u16(m.data(),0x800b1580+2*i,i);
 rr64::engine::write_u32(m.data(),0x800b1970,1);
 rr64::engine::write_u32(m.data(),0x800b1974,0x800b1980);
 recomp_context ctx{};ctx.f_odd=&ctx.f0.u32h;
 rr64::authority::Command c{1,2,0,0,0};
 check(!rr64_prediction_capture_before(m.data(),&c,&ctx));c.sequence=1;
 m[100]=5;check(rr64_prediction_capture_before(m.data(),&c,&ctx));
 check(history->native.size()==1 && history->native.front().rules.win_started==1);
 check(history->native.front().session.humans==0x2001 && history->native.front().session.is_host);
 m[100]=8;clock_value=2;check(rr64_prediction_capture_after(m.data()));
 check(history->native.back().timing==timing);
 check(history->guest.restore(1,0,restored.data(),restored.size()) && restored[100]==5);
 check(history->guest.restore(1,1,restored.data(),restored.size()) && restored[100]==8);
 check(history->native.back().posts[13].cue==2 && captures==2);
 check(!rr64_prediction_capture_after(m.data()) && !rr64_prediction_capture_before(m.data(),&c,&ctx));
 c.sequence=2;check(rr64_prediction_capture_before(m.data(),&c,&ctx));
 check(rr64::prediction::restore_update_counters(m.data(),{0,1}));
 auto later=timing;later.bits[0]=std::bit_cast<unsigned>(1.f/30.f);later.substeps=1;
 check(rr64::prediction::restore_timing(m.data(),later));
 check(!rr64_prediction_capture_before(m.data(),&c,&ctx));check(rr64_prediction_capture_after(m.data()));
 check(history->native.back().timing==timing); // completion must retain entry timing
 rr64::prediction::HistoricalReplay plan;
 const std::array commands{history->native[1].command,history->native[2].command};
 check(rr64::prediction::prepare_history_replay(1,0,commands,0x800d8570,plan));
 check(plan.memory[100]==5 && plan.steps.size()==2 && plan.baseline.rules.win_started==1);
 check(plan.steps[0].previous_buttons==0 && plan.steps[1].timing==timing);
 check(plan.steps[0].counters==counters && plan.steps[1].counters==counters); // entry, not completion
 plan.memory[100]=77;
 check(history->guest.restore(1,0,restored.data(),restored.size()) && restored[100]==5);
 auto wrong=commands;wrong[1].buttons^=1;
 check(!rr64::prediction::prepare_history_replay(1,0,wrong,0x800d8570,plan));
 check(plan.memory[100]==77 && plan.steps.size()==2); // failed prepare is atomic
 const auto inventory=history->native.front().resources;
 history->native.front().resources.unrepresented=1;
 rr64::prediction::ReplayAdmission admission;
 check(!rr64::prediction::prepare_history_replay(1,0,commands,0x800d8570,plan,&admission));
 check(admission==rr64::prediction::ReplayAdmission::ResourcePending && plan.memory[100]==77);
 check(!rr64::prediction::prepare_history_replay(1,0,wrong,0x800d8570,plan,&admission));
 check(admission==rr64::prediction::ReplayAdmission::Invalid);
 history->native.front().resources=inventory;
 // Fully acknowledged correction performs no native update, so a held worker
 // must not prevent it. The private image retains the task exactly as captured.
 const auto last_inventory=history->native.back().resources;
 history->native.back().resources.unrepresented=1;
 rr64::prediction::HistoricalReplay no_steps;
 check(rr64::prediction::prepare_history_replay(1,2,{},0x800d8570,no_steps,&admission));
 check(admission==rr64::prediction::ReplayAdmission::Ready && no_steps.steps.empty());
 rr64::prediction::ReplayedHistory no_steps_result;
 check(rr64::prediction::evaluate_history_replay(no_steps,{},no_steps_result,
     [](auto&,const auto&){std::abort();return false;},
     [](auto&,const auto&,auto&){std::abort();return false;}));
 check(no_steps_result.final_memory==no_steps.memory && no_steps_result.native.size()==1);
 history->native.back().resources=last_inventory;


 check(rr64::prediction::prepare_history_replay(1,1,std::span(commands).subspan(1),0x800d8570,plan));
 check(plan.memory[100]==8 && plan.steps.size()==1 && plan.baseline.rules.win_started==2);
 rr64::prediction::ReplayedHistory result;
 const auto execute=[](rr64::prediction::Resources &resources,const rr64::prediction::FrameInput &input,
                      rr64::prediction::FrameOutput &out){
     check(input.rules.win_started==2);resources.memory()[100]+=10;
     out={input.rules,input.posts,input.eject};out.rules.win_started=3;return true;
 };
 unsigned prepares=0;
 const auto prepare=[&](auto &resources,const auto &input){
     ++prepares;check(input.command.sequence==2 && resources.memory()[100]==8);
     resources.memory()[101]=42;return true;
 };
 check(rr64::prediction::evaluate_history_replay(plan,{},result,prepare,execute));
 check(prepares==1 && result.final_memory[101]==42 && m[101]==0);
 check(result.final_memory[100]==18 && m[100]==8 && plan.memory[100]==8);
 check(!rr64::prediction::evaluate_history_replay(plan,{},result,prepare,
     [](auto &resources,const auto&,auto&){resources.memory()[100]=99;return false;}));
 check(result.final_memory[100]==18 && history->native.size()==3);
 check(!rr64::prediction::evaluate_history_replay(plan,{},result,
     [](auto &resources,const auto&){resources.memory()[100]=99;return false;},execute));
 check(result.final_memory[100]==18 && result.final_memory[101]==42 && plan.memory[100]==8);
 // Initial baseline is already at update entry; only subsequent updates need
 // the between-update stage. A corrected completion needs it even on step one.
 rr64::prediction::HistoricalReplay initial;
 check(rr64::prediction::prepare_history_replay(1,0,commands,0x800d8570,initial));
 rr64::prediction::ReplayedHistory initial_result;
 unsigned initial_prepares=0,initial_executes=0;
 check(rr64::prediction::evaluate_history_replay(initial,{},initial_result,
     [&](auto &resources,const auto &input){
         ++initial_prepares;check(input.command.sequence==2 && initial_executes==1);
         resources.memory()[102]=7;return true;
     },[&](auto &resources,const auto &input,auto &out){
         ++initial_executes;
         check(resources.memory()[102]==(input.command.sequence==1?0:7));
         out={input.rules,input.posts,input.eject};return true;
     }));
 check(initial_prepares==1 && initial_executes==2 && m[102]==0);
 auto stale=plan;stale.epoch+=1;
 unsigned approvals=0;
 const auto approve=[](void *p)noexcept{++*static_cast<unsigned*>(p);return true;};
 check(!rr64::prediction::commit_history_replay(stale,result,approve,&approvals) && approvals==0);
 check(!rr64::prediction::commit_history_replay(plan,result,[](void*)noexcept{return false;},nullptr));
 check(history->native.size()==3 && result.final_memory[100]==18);
 check(rr64::prediction::commit_history_replay(plan,result,approve,&approvals) && approvals==1);
 check(!rr64::prediction::commit_history_replay(plan,result,approve,&approvals) && approvals==1);
 check(history->native.size()==2 && history->native.back().rules.win_started==3);
 check(!history->guest.restore(1,0,restored.data(),restored.size()));
 check(history->guest.restore(1,2,restored.data(),restored.size()) && restored[100]==18);
 auto invalid=timing;invalid.bits[0]=0;const auto unchanged=m;
 check(!rr64::prediction::restore_timing(m.data(),invalid) && m==unchanged);
 recomp_context replay{};
 check(history->native.back().entry.restore(replay) && replay.f_odd==&replay.f0.u32h);
 rr64_prediction_capture_reset();check(!history);
 // Loading requests cross the rendering/simulation thread boundary in order.
 c.sequence=1;check(rr64_prediction_capture_before(m.data(),&c,&ctx));
 check(rr64_prediction_capture_after(m.data()));
 rr64::engine::write_u32(m.data(),0x800a7720,17);
 rr64_prediction_capture_streaming(m.data(),&ctx);
 rr64::engine::write_u32(m.data(),0x800a7720,23);
 rr64_prediction_capture_streaming(m.data(),&ctx);
 c.sequence=2;check(rr64_prediction_capture_before(m.data(),&c,&ctx));
 check(rr64_prediction_capture_after(m.data()));
 check(rr64::prediction::prepare_history_replay(1,1,std::span(&c,1),0x800d8570,plan));
 check(plan.streaming.size()==1 && plan.streaming[0].size()==2);
 check(!rr64::prediction::evaluate_history_replay(plan,{},result,
     [](auto&,const auto&){return true;},[](auto&,const auto&,auto&){return true;}));
 unsigned streamed=0;
 check(rr64::prediction::evaluate_history_replay(plan,{},result,
     [&](auto&,const auto&){check(streamed==2);return true;},
     [](auto&,const auto& input,auto& out){out={input.rules,input.posts,input.eject};return true;},
     [&](auto&,const auto& input,const auto& schedule){
         check(schedule.valid && input.words[1]==(streamed?23u:17u));++streamed;return true;
     }));
 // Overflow cannot silently produce a partial, apparently valid correction.
 for(unsigned i=0;i<65;++i)rr64_prediction_capture_streaming(m.data(),&ctx);
 c.sequence=3;check(rr64_prediction_capture_before(m.data(),&c,&ctx));
 check(rr64_prediction_capture_after(m.data()));
 check(!rr64::prediction::prepare_history_replay(1,2,std::span(&c,1),0x800d8570,plan));
 rr64_prediction_capture_reset();check(!history && !streaming_mailbox.recording);
 std::puts("prediction baseline and completed input state retain separate guest/native histories; duplicate steps rejected");
}
