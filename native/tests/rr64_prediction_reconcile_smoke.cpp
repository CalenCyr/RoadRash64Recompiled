#include "../src/rr64_prediction_capture.cpp"
#include "../src/rr64_prediction_reconcile.cpp"
#include <cstdio>
#include <cstdlib>
namespace {
rr64::prediction::FrameOutput live_native;
rr64::netplay::AuthorityReplayPlan network_plan;
bool approve_network=false,fail_execute=false,has_frame=true;
unsigned tickets=0;
constexpr unsigned bike=0x80100000,rider=0x80300000;
void check(bool b){if(!b)std::abort();}
}
namespace rr64::netplay {
Status get_status(){Status s;s.active=s.connected=s.authoritative=true;s.replicated_riders=true;s.local_slot=1;
 s.authority_humans=3;s.phase=Phase::Race;s.game_setup.revision=7;return s;}
bool authority_prepare_replay(AuthorityReplayPlan &p){if(!has_frame)return false;p=network_plan;return true;}
bool authority_commit_replay(std::uint64_t t){++tickets;if(!approve_network || t!=network_plan.ticket)return false;has_frame=false;return true;}
}
namespace ultramodern::rr64 {
bool copy_guest_snapshot(unsigned char *m,unsigned char *out,std::size_t size){std::memcpy(out,m,size);return true;}
}
namespace rr64::prediction {
bool capture_cop_rules(CopRulesState &s){s=live_native.rules;return true;}
bool capture_cop_posts(CopPostsState &s){s=live_native.posts;return true;}
bool capture_manual_eject(ManualEjectState &s){s=live_native.eject;return true;}
void commit_cop_rules(const CopRulesState &s)noexcept{live_native.rules=s;}
void commit_cop_posts(const CopPostsState &s)noexcept{live_native.posts=s;}
void commit_manual_eject(const ManualEjectState &s)noexcept{live_native.eject=s;}
// This fixture isolates commit ownership; native ordering has its own original-
// instruction equivalence test, and history-stage sequencing is tested separately.
bool replay_native_order(Resources&,const FrameInput&){return true;}
bool replay_native_streaming(Resources&,const StreamingInput&,const ResourceInventory&){return true;}
bool replay_native_frame(Resources &resources,const FrameInput &input,FrameOutput &out){
 float x=0;engine::read_float(resources.memory(),bike+engine::bike::body_position,x);
 engine::write_float(resources.memory(),bike+engine::bike::body_position,x+1);
 out={input.rules,input.posts,input.eject};out.posts[0].cue+=1;
 return !fail_execute;
}
bool capture_prediction_movement(unsigned char *m,const authority::Stamp&,unsigned,unsigned,bool,netplay::AuthorityFrame &out,const char **,bool){
 out=network_plan.frame;return engine::read_float(m,bike+engine::bike::body_position,out.riders[1].position_x);
}
}
int main(){
 using namespace rr64;
 std::vector<unsigned char> m(engine::kRdramSize),rom(16),saved(engine::kRdramSize);
 engine::write_u32(m.data(),0x800a656c,2);
 engine::write_u16(m.data(),0x800d8594,1);
 // A live bike owns a render object/descriptor, also consulted by simulation.
 engine::write_u32(m.data(),bike+8,0x80600000);
 engine::write_u32(m.data(),0x80600028,0x80600100);
 engine::write_u32(m.data(),0x800d8570+0xe0,bike);engine::write_u32(m.data(),0x800d8570+0xe4,rider);
 engine::write_u32(m.data(),0x800d8570+0xe8,0x80500000);
 // Canonical remote host maps to guest slot 1; its live results must survive
 // reconciliation even when the historical authority frame contains older ones.
 engine::write_u32(m.data(),0x800d8570+0x118+0xe8,0x80500100);
 engine::write_u32(m.data(),0x80500158,88);
 engine::write_u32(m.data(),bike+engine::bike::rider_pointer,rider);engine::write_u32(m.data(),rider+engine::rider::bike_pointer,bike);
 engine::write_u16(m.data(),0x8009ce90,32);
 for(unsigned i=0;i<32;++i)engine::write_u16(m.data(),0x800b1580+i*2,i);
 engine::write_u32(m.data(),0x800b1970,1);engine::write_u32(m.data(),0x800b1974,0x800b1980);
 authority::NativeTiming timing;timing.substeps=1;for(auto &bits:timing.bits)bits=std::bit_cast<unsigned>(.01f);
 check(prediction::restore_timing(m.data(),timing));
 recomp_context ctx{};ctx.f_odd=&ctx.f0.u32h;
 authority::Command command{7,1,0,0,0};
 engine::write_float(m.data(),bike+engine::bike::body_position,10);
 live_native.posts[0].cue=5;
 check(rr64_prediction_capture_before(m.data(),&command,&ctx));
 engine::write_float(m.data(),bike+engine::bike::body_position,11);live_native.posts[0].cue=6;
 check(rr64_prediction_capture_after(m.data()));
 // Newer host-owned/native state must not rewind to the journal's values.
 live_native.rules.win_started=99;
 live_native.posts[1].cue=88;
 live_native.posts[1].muted=false;
 network_plan.ticket=4;network_plan.frame.stamp={7,1,{}};
 auto &state=network_plan.frame.riders[1];state.active=true;state.root.valid=state.rider_position_valid=1;state.position_x=100;
 network_plan.frame.outcomes[1].busts=4;network_plan.frame.outcomes[1].busted=1;
 network_plan.frame.outcomes[0].valid=1;network_plan.frame.outcomes[0].busts=2;
 network_plan.commands[0]=command;network_plan.count=1;
 const auto before=m;const auto epoch=history->epoch;
 const auto inventory=history->native.front().resources;
 history->native.front().resources.unrepresented=1;
 check(prediction::reconcile_movement(m.data(),rom));
 check(m==before && history->epoch==epoch && tickets==0 && has_frame);
 history->native.front().resources=inventory;

 fail_execute=true;check(!prediction::reconcile_movement(m.data(),rom) && m==before && tickets==0 && history->epoch==epoch);
 fail_execute=false;check(!prediction::reconcile_movement(m.data(),rom) && m==before && tickets==1 && history->epoch==epoch);
 approve_network=true;check(prediction::reconcile_movement(m.data(),rom) && tickets==2 && history->epoch!=epoch);
 float x=0;engine::read_float(m.data(),bike+engine::bike::body_position,x);check(x==101 && live_native.posts[0].cue==6);
 unsigned busts=0;std::uint16_t busted=0;engine::read_u32(m.data(),0x80500058,busts);engine::read_u16(m.data(),0x8050004c,busted);
 check(busts==4 && busted==1);
 check(live_native.rules.win_started==99 && live_native.posts[1].cue==88 && !live_native.posts[1].muted);
 engine::read_u32(m.data(),0x80500158,busts);check(busts==88);
 check(history->guest.restore(7,1,saved.data(),saved.size()));engine::read_float(saved.data(),bike+engine::bike::body_position,x);check(x==101);
 const auto committed=m;check(prediction::reconcile_movement(m.data(),rom) && m==committed && tickets==2);
 std::puts("connected reconciliation: private correction/replay, rejected ticket isolation, live/native/history commit and duplicate retirement pass; executor is synthetic");
}
