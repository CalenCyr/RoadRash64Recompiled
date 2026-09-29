#include "../src/rr64_netplay.cpp"
#include <cstdlib>
#include <numeric>
#include <source_location>

using namespace rr64::netplay;
namespace item = rr64::mk64_items;
unsigned checks=0;
void check(bool value,std::source_location at=std::source_location::current()) {
    ++checks;
    if(!value){std::fprintf(stderr,"MK64 channel failed at line %u\n",at.line());std::exit(1);}
}
sockaddr_in host_address() {
    sockaddr_in address{};address.sin_family=AF_INET;
    address.sin_addr.s_addr=htonl(INADDR_LOOPBACK);address.sin_port=htons(18991);
    return address;
}
void client() {
    g_session={};g_session.config.mode=Mode::Join;g_session.phase=Phase::Race;
    g_session.local_slot=13;g_session.token=123;g_session.authoritative=true;
    g_session.authority_round=7;g_session.authority_humans=1u|1u<<13;
    g_session.game_setup.valid=true;g_session.game_setup.revision=7;
    g_session.game_setup.course.version=1;g_session.host_endpoint=host_address();
}
AuthorityFrame sample(unsigned tick=1) {
    AuthorityFrame frame{};frame.stamp.round=7;frame.stamp.tick=tick;
    for(auto &bits:frame.timing.bits)bits=std::bit_cast<unsigned>(1.f);
    frame.timing.substeps=2;
    auto &state=frame.mk64_items;
    state.enabled=1;state.clock=120;state.random=12345;state.next_generation=64;
    for(unsigned i=0;i<14;++i) {
        auto &r=state.riders[i];r.held=item::Item::Mushroom;r.charges=1;
        r.revision=i+1;r.star_until=130+i;r.boost_until=150;r.boost_speed=10.f+i;
        r.hit_until=125;r.last_use=110;r.event_serial=i+1;r.cue=item::Cue::Mushroom;r.event_target=i;
    }
    for(unsigned i=0;i<64;++i) {
        auto &o=state.objects[i];o.generation=i+1;o.born=115;o.expires=300;
        o.kind=item::Item::GreenShell;o.mode=item::ObjectMode::Flying;
        o.owner=i%14;o.target=item::no_target;o.position={float(i),float(i*3),1};o.velocity={2,3,4};
    }
    return frame;
}
struct Parts {
    AuthorityStatePacket native{};
    AuthorityWorldPacket traffic{};
    AuthorityMk64RidersPacket riders{};
    std::array<AuthorityMk64ObjectsPacket,kMk64ObjectParts> objects{};
};
Parts parts(const AuthorityFrame &frame) {
    Parts p;
    auto setup=[&](auto &packet,PacketType type) {
        initialize_packet(packet,type);packet.stamp=frame.stamp;
        packet.payload_mask=authority_payload_mask(frame);packet.world_mask=authority_world_mask(frame);
    };
    setup(p.native,PacketType::AuthorityState);p.native.timing=frame.timing;
    p.native.cop_win_age=frame.cop_win_age;
    setup(p.traffic,PacketType::AuthorityWorld);
    setup(p.riders,PacketType::AuthorityMk64Riders);p.riders.meta=mk64_metadata(frame.mk64_items);
    p.riders.riders=frame.mk64_items.riders;
    for(unsigned part=0;part<kMk64ObjectParts;++part) {
        auto &object=p.objects[part];setup(object,PacketType::AuthorityMk64Objects);
        object.part=part;object.meta=p.riders.meta;
        for(unsigned i=0;i<16;++i)object.objects[i]=frame.mk64_items.objects[part*16+i];
    }
    return p;
}
template<class Packet> void deliver(const Packet &packet,const sockaddr_in &address=host_address(),int length=sizeof(Packet)) {
    if(valid_packet(packet.header,length))handle_client_packet_locked(
        reinterpret_cast<const std::uint8_t*>(&packet),length,address,Clock::now());
}
void deliver_part(const Parts &parts,unsigned part) {
    if(part==0)deliver(parts.native);
    else if(part==1)deliver(parts.traffic);
    else if(part==2)deliver(parts.riders);
    else deliver(parts.objects[part-3]);
}
void all(const Parts &parts) {for(unsigned i=0;i<7;++i)deliver_part(parts,i);}
int main() {
    static_assert(kProtocolVersion==65 && kAuthorityWorldParts==24);
    // The host-owned item switch travels in the existing setup word. Exercise
    // real validation and dispatch, including unknown bits and older clients.
    constexpr unsigned disabled_bit=rr64::local_race_options::mk64_items_disabled_bit;
    constexpr unsigned unrelated=7u|(2u<<4)|(5u<<6)|512u|1024u|0x07FFF800u;
    for(unsigned bits : {unrelated,unrelated|disabled_bit}) {
        client();
        LobbySnapshotPacket setup{};
        initialize_packet(setup,PacketType::LobbySnapshot);
        setup.header.sequence=1;
        setup.phase=static_cast<std::uint8_t>(Phase::GameSetup);
        setup.reserved=14;
        setup.game_setup.valid=1;
        setup.game_setup.revision=8;
        setup.game_setup.race_options=bits;
        deliver(setup);
        check(g_session.game_setup.revision==8);
        check(g_session.game_setup.race_options==bits);
        setup.header.sequence=2;setup.game_setup.revision=9;
        setup.game_setup.race_options=bits|(1u<<28);
        deliver(setup);
        check(g_session.game_setup.revision==8);
        check(g_session.game_setup.race_options==bits);
        setup.game_setup.race_options=bits^disabled_bit;
        setup.header.version=62;
        check(!valid_packet(setup.header,sizeof(setup)));
        deliver(setup);
        check(g_session.game_setup.race_options==bits);
        setup.header.version=kProtocolVersion;
        deliver(setup);
        check(g_session.game_setup.revision==9);
        check(g_session.game_setup.race_options==(bits^disabled_bit));
    }
    check(sizeof(AuthorityMk64RidersPacket)==904);
    check(sizeof(AuthorityMk64ObjectsPacket)==940);
    client();const auto frame=sample();check(item::valid(frame.mk64_items));const auto packet=parts(frame);
    // Every ordering uses the real endpoint/session dispatcher and completion
    // path; intermediate effects, inventory, and projectiles remain invisible.
    std::array<unsigned,7> order{};std::iota(order.begin(),order.end(),0u);
    unsigned permutations=0;
    do {
        client();
        for(unsigned i=0;i<7;++i) {
            deliver_part(packet,order[i]);
            check(g_session.authority_frame.stamp.tick==(i==6?1u:0u));
        }
        check(g_session.authority_frame.mk64_items==frame.mk64_items);++permutations;
    }while(std::next_permutation(order.begin(),order.end()));
    check(permutations==5040);
    for(unsigned missing=0;missing<7;++missing) {
        client();for(unsigned i=0;i<7;++i)if(i!=missing)deliver_part(packet,i);
        check(g_session.authority_frame.stamp.tick==0);
        deliver_part(packet,missing);check(g_session.authority_frame.mk64_items==frame.mk64_items);
        all(packet);check(g_session.authority_frame.mk64_items==frame.mk64_items);
    }
    // Invalid scalar rows must not claim an assembly part. Its good retransmit
    // can complete the same tick without replacing any accepted immutable part.
    for(unsigned fault=0;fault<13;++fault) {
        client();auto bad=packet.riders;
        switch(fault) {
        case 0:bad.riders[13].boost_speed=std::numeric_limits<float>::infinity();break;
        case 1:bad.riders[13].star_until=120;break;
        case 2:bad.riders[13].held=item::Item(16);break;
        case 3:bad.riders[13].charges=2;break;
        case 4:bad.riders[13].reserved=1;break;
        case 5:bad.riders[13].event_target=14;break;
        case 6:bad.meta.enabled=0;break;
        case 7:bad.meta.random=0;break;
        case 8:bad.meta.clock=30000001;break;
        case 9:bad.world_mask|=1u<<31;break;
        case 10:bad.payload_mask=1u<<14;break;
        case 11:bad.riders[13].last_use=121;break;
        case 12:bad.riders[13].hit_until=1921;break;
        }
        deliver(bad);for(unsigned i=0;i<7;++i)if(i!=2)deliver_part(packet,i);
        check(!g_session.authority_frame.stamp.tick);deliver(packet.riders);
        check(g_session.authority_frame.mk64_items==frame.mk64_items);
    }
    for(unsigned fault=0;fault<12;++fault) {
        client();auto bad=packet.objects[3];
        switch(fault) {
        case 0:bad.objects[15].velocity[0]=std::numeric_limits<float>::quiet_NaN();break;
        case 1:bad.objects[15].owner=14;break;
        case 2:bad.objects[15].target=14;break;
        case 3:bad.objects[15].expires=120;break;
        case 4:bad.objects[15].born=121;break;
        case 5:bad.objects[15].generation=65;break;
        case 6:bad.objects[15].kind=item::Item::Star;break;
        case 7:bad.objects[15].mode=item::ObjectMode::None;break;
        case 8:bad.objects[15].reserved=1;break;
        case 9:bad.part=4;break;
        case 10:bad.world_mask&=~kAuthorityMk64RidersBit;break;
        case 11:bad.objects={};break;
        }
        deliver(bad);for(unsigned i=0;i<6;++i)deliver_part(packet,i);
        check(!g_session.authority_frame.stamp.tick);deliver(packet.objects[3]);
        check(g_session.authority_frame.mk64_items==frame.mk64_items);
    }
    for(unsigned fault=0;fault<7;++fault) {
        client();auto bad=packet.objects[3];
        switch(fault) {
        case 0:++bad.meta.random;break;
        case 1:++bad.meta.clock;break;
        case 2:++bad.meta.next_generation;break;
        case 3:++bad.stamp.acknowledged[13];break;
        case 4:++bad.stamp.simulated_us[13];break;
        case 5:++bad.stamp.round;break;
        case 6:++bad.stamp.tick;break;
        }
        for(unsigned i=0;i<6;++i)deliver_part(packet,i);deliver(bad);
        check(!g_session.authority_frame.stamp.tick);deliver(packet.objects[3]);
        check(g_session.authority_frame.mk64_items==frame.mk64_items);
    }
    // Cross-fragment object identity and owner/shield consistency are validated
    // only after assembly; malformed whole snapshots never become visible.
    for(unsigned fault=0;fault<3;++fault) {
        client();auto invalid=frame;
        if(fault==0)invalid.mk64_items.objects[63].generation=1;
        if(fault==1)invalid.mk64_items.objects[63].mode=item::ObjectMode::Orbiting;
        if(fault==2) {
            invalid.mk64_items.riders[0].held=item::Item::TripleGreenShell;
            invalid.mk64_items.riders[0].deployed=1;
        }
        check(!item::valid(invalid.mk64_items));all(parts(invalid));check(!g_session.authority_frame.stamp.tick);
        auto later=sample(9);all(parts(later));check(g_session.authority_frame.mk64_items==later.mk64_items);
    }
    // A duplicate cannot replace an accepted piece, including before completion.
    client();deliver(packet.riders);auto duplicate=packet.riders;duplicate.riders[0].revision=999;
    deliver(duplicate);all(packet);check(g_session.authority_frame.mk64_items==frame.mk64_items);
    // Missing slots are cleared when the new frame commits, independent of the
    // old slot generation. Enabled-empty and disabled each retire prior objects.
    auto sparse=sample(2);sparse.mk64_items.objects={};
    sparse.mk64_items.objects[63]=frame.mk64_items.objects[63];
    auto sparse_packets=parts(sparse);deliver(sparse_packets.riders);deliver(sparse_packets.native);
    deliver(sparse_packets.traffic);check(g_session.authority_frame.stamp.tick==1);
    deliver(sparse_packets.objects[3]);check(g_session.authority_frame.mk64_items==sparse.mk64_items);
    auto empty=sparse;empty.stamp.tick=3;empty.mk64_items.objects={};const auto empty_packets=parts(empty);
    deliver(empty_packets.native);deliver(empty_packets.traffic);check(g_session.authority_frame.stamp.tick==2);
    deliver(empty_packets.riders);check(g_session.authority_frame.mk64_items==empty.mk64_items);
    auto disabled=empty;disabled.stamp.tick=4;disabled.mk64_items={};auto disabled_packets=parts(disabled);
    deliver(disabled_packets.native);deliver(disabled_packets.traffic);
    check(g_session.authority_frame.stamp.tick==4 && g_session.authority_frame.mk64_items==item::Snapshot{});
    all(packet);check(g_session.authority_frame.stamp.tick==4);
    // Imported-course gate, authenticated source/token, and protocol mismatch.
    client();g_session.game_setup.course.version=0;all(packet);check(!g_session.authority_frame.stamp.tick);
    for(unsigned fault=0;fault<4;++fault) {
        client();auto bad=packet.riders;auto address=host_address();
        if(fault==0)bad.header.session=999;
        if(fault==1)address.sin_port=htons(18992);
        if(fault==2)bad.header.version=60;
        if(fault==3)bad.header.size--;
        deliver(bad,address);for(unsigned i=0;i<7;++i)if(i!=2)deliver_part(packet,i);
        check(!g_session.authority_frame.stamp.tick);deliver(packet.riders);
        check(g_session.authority_frame.mk64_items==frame.mk64_items);
    }
    // Host admission validates whole item state and rejects non-imported races.
    client();g_session.config.mode=Mode::Host;g_session.local_slot=0;
    check(g_session.authority_host.reset(7,1));rr64::authority::Step native_step;
    check(g_session.authority_host.begin(native_step));check(g_session.authority_host.finish(native_step.tick));
    auto published=sample();published.stamp=g_session.authority_host.stamp();
    g_session.game_setup.course.version=0;check(!authority_publish_frame(published));
    g_session.game_setup.course.version=1;auto invalid=published;
    invalid.mk64_items.objects[63].generation=1;check(!authority_publish_frame(invalid));
    check(authority_publish_frame(published));
    // Exercise the production sender too: capture its real UDP datagrams on a
    // local socket, then feed those exact bytes through the client dispatcher.
    check(start_winsock_locked());
    SOCKET receiver=socket(AF_INET,SOCK_DGRAM,IPPROTO_UDP);
    sockaddr_in receive_address{};receive_address.sin_family=AF_INET;
    receive_address.sin_addr.s_addr=htonl(INADDR_LOOPBACK);
    check(bind(receiver,reinterpret_cast<sockaddr*>(&receive_address),sizeof(receive_address))==0);
    socklen_type address_size=sizeof(receive_address);
    check(getsockname(receiver,reinterpret_cast<sockaddr*>(&receive_address),&address_size)==0);
    u_long nonblocking=1;check(ioctlsocket(receiver,FIONBIO,&nonblocking)==0);
    g_session.socket=socket(AF_INET,SOCK_DGRAM,IPPROTO_UDP);
    g_session.peers[13].connected=true;g_session.peers[13].endpoint=receive_address;
    service_authority_locked(Clock::now());
    std::vector<std::vector<std::uint8_t>> datagrams;
    for(unsigned i=0;i<8;++i) {
        std::array<std::uint8_t,1200> bytes{};
        const int count=recv(receiver,reinterpret_cast<char*>(bytes.data()),int(bytes.size()),0);
        check(count>0 && count<1200);
        datagrams.emplace_back(bytes.begin(),bytes.begin()+count);
    }
    close_socket_locked();closesocket(receiver);client();
    for(const auto &bytes:datagrams) {
        const auto &header=*reinterpret_cast<const PacketHeader*>(bytes.data());
        check(valid_packet(header,int(bytes.size())));
        // Race begin is a separate persistent loading command, not a world part.
        if(header.type==PacketType::AuthorityBegin)continue;
        handle_client_packet_locked(bytes.data(),int(bytes.size()),host_address(),Clock::now());
    }
    check(g_session.authority_frame.mk64_items==published.mk64_items);
    // The authenticated slot determines ownership; duplicate/partial/held native
    // updates consume one edge while retaining ordinary buttons and direction.
    for(unsigned slot=0;slot<14;++slot) {
        rr64::authority::HostRound round;check(round.reset(7,1u<<slot));
        rr64::authority::InputBatch input{};input.round=7;input.count=1;
        input.commands[0]={7,1,0x2000,12,-90,rr64::authority::allowed_actions,40000};
        check(round.receive(slot,input));check(round.receive(slot,input));
        rr64::authority::Step step;check(round.begin(step,16667));
        check(step.inputs[slot].actions==3 && step.inputs[slot].y==-90 && step.inputs[slot].buttons==0x2000);
        check(round.finish(step.tick));check(round.stamp().acknowledged[slot]==0);
        for(unsigned i=0;i<8;++i){check(round.begin(step,16667));check(step.inputs[slot].actions==0);check(round.finish(step.tick));}
        check(round.stamp().acknowledged[slot]==1);
        check(round.receive(slot,input));check(round.begin(step));check(step.inputs[slot].actions==0);check(round.finish(step.tick));
        input.commands[0].sequence=2;input.commands[0].actions=4;check(!round.receive(slot,input));
    }
    g_session={};
    std::printf("MK64 item channel: %u checks, %u packet permutations, 14 canonical input slots; packets %zu/%zu bytes\n",
        checks,permutations,sizeof(AuthorityMk64RidersPacket),sizeof(AuthorityMk64ObjectsPacket));
}
