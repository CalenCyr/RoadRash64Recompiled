#include "rr64_prediction_replay.hpp"
#include "rr64_prediction_rules.hpp"
#include "rr64_prediction_cop_state.hpp"
#include "rr64_custom_cop.hpp"
#include "rr64_engine_layout.hpp"
#include "rr64_local_players.hpp"
#include "rr64_netplay.hpp"
#include <vector>
#include <cstdio>
#include <cstdlib>
rr64::netplay::Status test_status;
namespace rr64::netplay {Status get_status(){return test_status;}}
extern "C" int rr64_custom_cop_enabled(){return 1;}
void check(bool b){if(!b){std::fputs("cop rule replay check failed\n",stderr);std::exit(1);}}
int main(){
 using namespace rr64;
 local_players::active.store(true);
 std::vector<unsigned char> memory(16*1024*1024);auto *m=memory.data();
 engine::write_u32(m,0x800a6578,2);engine::write_u32(m,0x800a656c,2);
 engine::write_u32(m,0x800d8570+0x18,31);engine::write_u32(m,0x800d8570+0x1c,40);
 engine::write_u32(m,0x800d8570+0x118+0x18,1);engine::write_u32(m,0x800d8570+0x118+0x1c,1);
 engine::write_u32(m,0x800d8570+0x118+0xe8,0x80400000);engine::write_u16(m,0x80400000+0x4c,1);
 rr64_custom_cop_begin(m,1);rr64_custom_cop_roles(m);check(rr64_custom_cop_active());
 prediction::CopRulesState historical;check(prediction::capture_cop_rules(historical));
 prediction::CopRulesState continued;
 check(!prediction::replay_cop_rules(continued));
 check(!prediction::seed_cop_rules(historical));
 {
  prediction::ReplayScope replay;check(replay.valid());
  engine::write_float(m,0x800d7670,10.f);check(rr64_custom_cop_win_age(m)==0.f);
  engine::write_float(m,0x800d7670,12.f);check(rr64_custom_cop_win_age(m)==2.f);
  rr64_custom_cop_reset();check(!rr64_custom_cop_active());
  check(prediction::replay_cop_rules(continued) && !continued.race_enabled);
 }
 check(rr64_custom_cop_active());
 {prediction::ReplayScope replay;check(!prediction::replay_cop_rules(continued));
  check(prediction::seed_cop_rules(continued));check(!rr64_custom_cop_active());}
 check(rr64_custom_cop_active());
 engine::write_float(m,0x800d7670,20.f);check(rr64_custom_cop_win_age(m)==0.f);
 engine::write_float(m,0x800d7670,23.f);check(rr64_custom_cop_finished(m));
 rr64_custom_cop_reset();check(!rr64_custom_cop_active());
 {
  prediction::ReplayScope replay;
  check(prediction::seed_cop_rules(historical));check(rr64_custom_cop_active());
  check(!prediction::capture_cop_rules(historical));
 }
 check(!rr64_custom_cop_active());
 // Race ordering dispatches notifications too. Historical cop mode must
 // control the message even after the live game has left that mode.
 const unsigned cop=0x800d8570,racer=cop+0x118;
 engine::write_u16(m,cop+0x88,1);engine::write_u16(m,racer+0x88,1);
 {
  prediction::ReplayScope replay;
  check(prediction::seed_cop_rules(historical));
  rr64_custom_cop_notification(m,cop+0x2c,3,racer+0xc);
  std::uint8_t letter=0;engine::read_u8(m,cop+0x92,letter);check(letter=='B');
 }
 check(!rr64_custom_cop_active());
 const auto notification_memory=memory;
 {
  prediction::ReplayScope replay;
  check(prediction::seed_cop_rules({}));
  rr64_custom_cop_notification(m,cop+0x2c,3,racer+0xc);
  check(memory==notification_memory);
 }
 // Authority human identities need not be a contiguous prefix of the roster.
 test_status.active=test_status.connected=test_status.authoritative=test_status.is_host=true;
 test_status.replicated_riders=true;test_status.authority_humans=0x2001;
 engine::write_u32(m,0x800a656c,14);engine::write_u32(m,0x800a6578,1);
 const unsigned remote=0x800d8570+13*0x118;
 engine::write_u32(m,remote+0x18,31);engine::write_u32(m,remote+0x1c,44);
 engine::write_u32(m,0x800d8570+0x118+0x20,123); // AI role must remain native
 rr64_custom_cop_begin(m,1);rr64_custom_cop_roles(m);check(rr64_custom_cop_active());
 unsigned role=0;engine::read_u32(m,remote+0x20,role);check(role==7);
 engine::read_u32(m,0x800d8570+0x118+0x20,role);check(role==123);
 check(rr64_custom_cop_arrest(m,remote,0x800d8570+0x118));
 prediction::CopRulesState online_rules;check(prediction::capture_cop_rules(online_rules));
 {
  auto saved=prediction::capture_session_rules(test_status);
  prediction::ReplayScope replay(saved);
  check(prediction::seed_cop_rules(online_rules));
  test_status.is_host=false; // A later live session change cannot alter replay rules.
  check(rr64_custom_cop_active());
  check(prediction::status_for_rules().authority_humans==0x2001);
 }
 test_status.is_host=false;test_status.local_slot=13;
 check(rr64_custom_cop_active()); // movement/recovery prediction remains available
 const auto before_outcomes=memory;
 check(!rr64_custom_cop_arrest(m,remote,0x800d8570+0x118));
 check(rr64_custom_cop_win_age(m)==-1 && !rr64_custom_cop_finished(m));
 check(memory==before_outcomes); // clients cannot award busts or finish a race
 test_status={};rr64_custom_cop_reset();
 std::puts("native cop win/reset isolation and unchanged live outcome behavior passed");
}
