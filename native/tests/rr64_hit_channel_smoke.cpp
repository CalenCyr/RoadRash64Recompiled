#include "../src/rr64_netplay.cpp"
#include <cstdlib>
using namespace rr64::netplay;
void check(bool b){if(!b)std::abort();}
HitEvent event(){HitEvent e{};e.round=7;e.id=1;e.attacker=1;e.victim=0;e.strength=5;e.attack.valid=1;return e;}
void receive(PacketType type,HitEvent e,unsigned sender=1){HitPacket p{};initialize_packet(p,type);p.event=e;handle_hit_locked(reinterpret_cast<const std::uint8_t*>(&p),sizeof(p),sender,{},Clock::now());}
int main(){
 g_session={};g_session.config.mode=Mode::Host;g_session.phase=Phase::Race;
 g_session.game_setup.revision=7;g_session.local_slot=0;g_session.race_player_mask=3;
 g_session.riders[0].active=g_session.riders[1].active=true;
 receive(PacketType::HitRequest,event());check(g_session.hits.delivered.size()==1);
 receive(PacketType::HitRequest,event());check(g_session.hits.delivered.size()==1);
 HitEvent out{};check(take_hit(out) && out.strength==5);check(!take_hit(out));
 auto e=event();e.id=2;e.attacker=0;receive(PacketType::HitRequest,e);check(!take_hit(out)); // forged owner
 e=event();e.id=3;e.strength=std::numeric_limits<float>::infinity();receive(PacketType::HitRequest,e);check(!take_hit(out));
 e=event();e.id=4;g_session.riders[1].position_x=1000;receive(PacketType::HitRequest,e);check(!take_hit(out));
 g_session.riders[1].position_x=0;e=event();e.id=5;e.round=6;receive(PacketType::HitRequest,e);check(g_session.hits.received_request[1]==4);
 // Queue pressure must not acknowledge/discard an otherwise valid next hit.
 e=event();e.id=5;g_session.hits.delivered.resize(64);receive(PacketType::HitRequest,e);check(g_session.hits.received_request[1]==4);
 g_session.hits.delivered.clear();receive(PacketType::HitRequest,e);check(g_session.hits.received_request[1]==5 && take_hit(out));
 // Host routes a target-owned outcome with a per-recipient reliable sequence.
 g_session.peers[1].connected=true;e=event();e.attacker=0;e.victim=1;
 check(submit_hit(e));check(g_session.hits.commits[1].size()==1);
 receive(PacketType::HitCommitAck,g_session.hits.commits[1].front().event);check(g_session.hits.commits[1].empty());
 // Client accepts ordered host commits once, re-acknowledges retries, rejects gaps.
 g_session={};g_session.config.mode=Mode::Join;g_session.local_slot=1;g_session.phase=Phase::Race;g_session.game_setup.revision=7;
 e=event();e.attacker=0;e.victim=1;receive(PacketType::HitCommit,e,0);receive(PacketType::HitCommit,e,0);
 check(g_session.hits.delivered.size()==1 && take_hit(out));
 e.id=3;receive(PacketType::HitCommit,e,0);check(!take_hit(out));
 e.id=2;receive(PacketType::HitCommit,e,0);check(take_hit(out));
 g_session.game_setup.revision=8;receive(PacketType::HitCommit,e,0);check(!take_hit(out));
 // Drop the first UDP delivery/ACK: retry must carry the same outcome ID.
 check(start_winsock_locked());SOCKET destination=socket(AF_INET,SOCK_DGRAM,IPPROTO_UDP);
 sockaddr_in address{};address.sin_family=AF_INET;address.sin_addr.s_addr=htonl(INADDR_LOOPBACK);
 check(bind(destination,reinterpret_cast<sockaddr*>(&address),sizeof(address))==0);
 socklen_type len=sizeof(address);check(getsockname(destination,reinterpret_cast<sockaddr*>(&address),&len)==0);
 u_long nonblocking=1;check(ioctlsocket(destination,FIONBIO,&nonblocking)==0);
 g_session={};g_session.config.mode=Mode::Host;g_session.phase=Phase::Race;g_session.game_setup.revision=7;
 g_session.socket=socket(AF_INET,SOCK_DGRAM,IPPROTO_UDP);g_session.peers[1].connected=true;g_session.peers[1].endpoint=address;
 g_session.race_player_mask=3;reset_hits_locked();e=event();e.attacker=0;e.victim=1;check(route_hit_locked(e));
 const auto now=Clock::now();service_hits_locked(now);HitPacket first{},retry{};
 check(recv(destination,reinterpret_cast<char*>(&first),sizeof(first),0)==sizeof(first));
 service_hits_locked(now+std::chrono::milliseconds(61));
 check(recv(destination,reinterpret_cast<char*>(&retry),sizeof(retry),0)==sizeof(retry));
 check(first.event.id==retry.event.id && first.event.strength==retry.event.strength);
 receive(PacketType::HitCommitAck,retry.event);service_hits_locked(now+std::chrono::milliseconds(122));
 check(recv(destination,reinterpret_cast<char*>(&retry),sizeof(retry),0)==SOCKET_ERROR);
 closesocket(destination);close_socket_locked();g_session={};
 std::puts("hit ownership, ordered delivery, duplicates, pressure and round checks passed");
}
