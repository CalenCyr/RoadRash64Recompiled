#include "rr64_netplay.hpp"
#include "rr64_connection_address.hpp"

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <deque>
#include <mutex>
#include <random>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <WinSock2.h>
#include <WS2tcpip.h>
#else
#include <arpa/inet.h>
#include <cerrno>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

using SOCKET = int;
using socklen_type = socklen_t;
constexpr SOCKET INVALID_SOCKET = -1;
constexpr int SOCKET_ERROR = -1;
constexpr int WSAEWOULDBLOCK = EWOULDBLOCK;

inline int closesocket(SOCKET socket_handle) { return ::close(socket_handle); }

inline int ioctlsocket(SOCKET socket_handle, long command, u_long* argument) {
    return ::ioctl(socket_handle, static_cast<unsigned long>(command), argument);
}

inline int WSAGetLastError() { return errno; }
#endif

#ifdef _WIN32
using socklen_type = int;
#endif

namespace rr64::netplay {
namespace {

using Clock = std::chrono::steady_clock;

constexpr std::uint32_t kProtocolMagic = 0x52523634u; // RR64
constexpr std::uint16_t kProtocolVersion = 6;
constexpr std::size_t kPlayerNameCapacity = 24;
constexpr auto kHelloInterval = std::chrono::milliseconds(500);
constexpr auto kStateInterval = std::chrono::milliseconds(25);
constexpr auto kSnapshotInterval = std::chrono::milliseconds(50);
constexpr auto kRaceStateInterval = std::chrono::milliseconds(25);
constexpr auto kRaceSnapshotInterval = std::chrono::milliseconds(50);
constexpr auto kPingInterval = std::chrono::seconds(1);
constexpr auto kPeerTimeout = std::chrono::seconds(5);
constexpr auto kConnectTimeout = std::chrono::seconds(15);
constexpr std::size_t kMaximumQueuedVoiceFrames = 128;
constexpr std::uint16_t kMaximumVoicePacketsPerSecond = 75;

enum class PacketType : std::uint8_t {
    Hello = 1,
    Welcome,
    ClientState,
    LobbySnapshot,
    Ping,
    Pong,
    Disconnect,
    ClientRiderState,
    RaceSnapshot,
    Voice,
    ConnectReject,
};

#pragma pack(push, 1)
struct PacketHeader {
    std::uint32_t magic = kProtocolMagic;
    std::uint16_t version = kProtocolVersion;
    PacketType type = PacketType::Hello;
    std::uint8_t reserved = 0;
    std::uint16_t size = 0;
    std::uint16_t reserved2 = 0;
    std::uint32_t sequence = 0;
    std::uint64_t session = 0;
};

struct HelloPacket {
    PacketHeader header{};
    char player_name[kPlayerNameCapacity]{};
};
enum class RejectReason : std::uint16_t { Full=1, Busy=2, Version=3 };
struct ConnectRejectPacket {
    PacketHeader header{};
    RejectReason reason{};
    std::uint16_t expected_version=kProtocolVersion;
};

struct WelcomePacket {
    PacketHeader header{};
    std::uint8_t assigned_slot = kInvalidSlot;
    std::uint8_t maximum_players = kMaximumPlayers;
    std::uint8_t phase = static_cast<std::uint8_t>(Phase::Lobby);
    std::uint8_t replicated_riders = 0;
};

struct ClientStatePacket {
    PacketHeader header{};
    std::uint8_t slot = kInvalidSlot;
    std::uint8_t ready = 0;
    std::uint8_t character = 0;
    std::uint8_t track = 0;
    std::uint16_t buttons = 0;
    std::int8_t stick_x = 0;
    std::int8_t stick_y = 0;
    char player_name[kPlayerNameCapacity]{};
};

struct WirePlayer {
    std::uint8_t connected = 0;
    std::uint8_t ready = 0;
    std::uint8_t character = 0;
    std::uint8_t track = 0;
    std::uint16_t buttons = 0;
    std::int8_t stick_x = 0;
    std::int8_t stick_y = 0;
    std::uint16_t ping_ms = 0;
    char name[kPlayerNameCapacity]{};
};

struct WireGameSetup {
    std::uint32_t revision = 0;
    std::uint16_t transition_buttons = 0;
    std::uint8_t valid = 0;
    std::uint8_t reserved = 0;
    std::uint32_t words[kGameSetupWordCount]{};
};

struct LobbySnapshotPacket {
    PacketHeader header{};
    std::uint8_t phase = static_cast<std::uint8_t>(Phase::Lobby);
    std::uint8_t connected_players = 0;
    std::uint8_t replicated_riders = 0;
    std::uint8_t reserved = 0;
    WireGameSetup game_setup{};
    WirePlayer players[kMaximumPlayers]{};
};

struct WireRiderState {
    std::uint8_t active = 0;
    std::uint8_t animation = 0;
    std::uint8_t weapon = 0;
    std::uint8_t character = 0;
    std::uint8_t bike = 0;
    std::uint8_t reserved = 0;
    std::uint16_t health = 0;
    std::uint16_t flags = 0;
    std::uint16_t reserved2 = 0;
    std::uint32_t tick = 0;
    float position_x = 0.0f;
    float position_y = 0.0f;
    float position_z = 0.0f;
    float front_wheel_x = 0.0f;
    float front_wheel_y = 0.0f;
    float front_wheel_z = 0.0f;
    float rear_wheel_x = 0.0f;
    float rear_wheel_y = 0.0f;
    float rear_wheel_z = 0.0f;
    float bike_lean = 0.0f;
    float front_suspension = 0.0f;
    float rear_suspension = 0.0f;
};

struct ClientRiderStatePacket {
    PacketHeader header{};
    std::uint8_t slot = kInvalidSlot;
    std::uint8_t reserved[3]{};
    WireRiderState state{};
};

struct RaceSnapshotPacket {
    PacketHeader header{};
    std::uint32_t host_tick = 0;
    WireRiderState riders[kMaximumPlayers]{};
};

struct PingPacket {
    PacketHeader header{};
    std::uint8_t slot = kInvalidSlot;
    std::uint8_t reserved[3]{};
    std::uint32_t nonce = 0;
};

struct VoicePacket {
    PacketHeader header{};
    std::uint8_t speaker_slot = kInvalidSlot;
    std::uint8_t reserved = 0;
    std::uint16_t payload_size = 0;
    std::uint32_t voice_sequence = 0;
    std::uint8_t payload[kVoicePayloadCapacity]{};
};
#pragma pack(pop)

struct InputState {
    std::uint16_t buttons = 0;
    float stick_x = 0.0f;
    float stick_y = 0.0f;
};

struct Peer {
    bool connected = false;
    sockaddr_in endpoint{};
    Clock::time_point last_seen{};
    Clock::time_point ping_sent{};
    std::uint32_t ping_nonce = 0;
    Clock::time_point voice_window_start{};
    std::uint16_t voice_packets_in_window = 0;
};

struct Session {
    Config config{};
    SOCKET socket = INVALID_SOCKET;
    sockaddr_in host_endpoint{};
    std::uint64_t token = 0;
    std::uint32_t sequence = 1;
    std::uint8_t local_slot = kInvalidSlot;
    Phase phase = Phase::Offline;
    std::array<PlayerInfo, kMaximumPlayers> players{};
    std::array<InputState, kMaximumPlayers> inputs{};
    std::array<RiderState, kMaximumPlayers> riders{};
    std::array<RiderState, kMaximumPlayers> previous_riders{};
    std::array<Peer, kMaximumPlayers> peers{};
    GameSetupState game_setup{};
    Clock::time_point last_hello{};
    Clock::time_point last_state{};
    Clock::time_point last_snapshot{};
    Clock::time_point last_race_state{};
    Clock::time_point last_race_snapshot{};
    Clock::time_point last_ping{};
    std::uint32_t client_ping_nonce = 0;
    Clock::time_point client_ping_sent{};
    std::string message = "Offline";
    std::uint32_t last_lobby_snapshot_sequence = 0;
    std::uint32_t last_race_snapshot_sequence = 0;
    std::uint32_t local_voice_sequence = 0;
    std::array<std::uint32_t, kMaximumPlayers> last_voice_sequences{};
    std::deque<ReceivedVoiceFrame> received_voice_frames{};
    bool replicated_riders = false;
    Clock::time_point connect_started{}, last_host_seen{};
    std::uint64_t hello_attempts=0, received_packets=0;
    int last_socket_error=0;
};

std::mutex g_mutex;
Session g_session{};
bool g_winsock_started = false;

bool is_host_locked() {
    return g_session.config.mode == Mode::Host;
}

bool is_client_locked() {
    return g_session.config.mode == Mode::Join;
}

std::uint64_t make_session_token() {
    const auto now = static_cast<std::uint64_t>(
        std::chrono::duration_cast<std::chrono::nanoseconds>(Clock::now().time_since_epoch()).count());
    std::random_device random;
    return now ^ (static_cast<std::uint64_t>(random()) << 32) ^ random();
}

void copy_string(char* destination, std::size_t capacity, const std::string& source) {
    if (capacity == 0) {
        return;
    }
    const std::size_t length = std::min(capacity - 1, source.size());
    std::memcpy(destination, source.data(), length);
    destination[length] = '\0';
}

std::string read_string(const char* source, std::size_t capacity) {
    const void* end = std::memchr(source, '\0', capacity);
    const std::size_t length = end == nullptr
        ? capacity
        : static_cast<const char*>(end) - source;
    return std::string(source, length);
}

bool finite_rider_state(const RiderState& state) {
    return std::isfinite(state.position_x) && std::isfinite(state.position_y) &&
        std::isfinite(state.position_z) && std::isfinite(state.front_wheel_x) &&
        std::isfinite(state.front_wheel_y) && std::isfinite(state.front_wheel_z) &&
        std::isfinite(state.rear_wheel_x) && std::isfinite(state.rear_wheel_y) &&
        std::isfinite(state.rear_wheel_z) && std::isfinite(state.bike_lean) &&
        std::isfinite(state.front_suspension) && std::isfinite(state.rear_suspension);
}

WireRiderState to_wire(const RiderState& state) {
    WireRiderState wire{};
    wire.active = state.active ? 1 : 0;
    wire.animation = state.animation;
    wire.weapon = state.weapon;
    wire.character = state.character;
    wire.bike = state.bike;
    wire.health = state.health;
    wire.flags = state.flags;
    wire.tick = state.tick;
    wire.position_x = state.position_x;
    wire.position_y = state.position_y;
    wire.position_z = state.position_z;
    wire.front_wheel_x = state.front_wheel_x;
    wire.front_wheel_y = state.front_wheel_y;
    wire.front_wheel_z = state.front_wheel_z;
    wire.rear_wheel_x = state.rear_wheel_x;
    wire.rear_wheel_y = state.rear_wheel_y;
    wire.rear_wheel_z = state.rear_wheel_z;
    wire.bike_lean = state.bike_lean;
    wire.front_suspension = state.front_suspension;
    wire.rear_suspension = state.rear_suspension;
    return wire;
}

RiderState from_wire(const WireRiderState& wire) {
    RiderState state{};
    state.active = wire.active != 0;
    state.animation = wire.animation;
    state.weapon = wire.weapon;
    state.character = wire.character;
    state.bike = wire.bike;
    state.health = wire.health;
    state.flags = wire.flags;
    state.tick = wire.tick;
    state.position_x = wire.position_x;
    state.position_y = wire.position_y;
    state.position_z = wire.position_z;
    state.front_wheel_x = wire.front_wheel_x;
    state.front_wheel_y = wire.front_wheel_y;
    state.front_wheel_z = wire.front_wheel_z;
    state.rear_wheel_x = wire.rear_wheel_x;
    state.rear_wheel_y = wire.rear_wheel_y;
    state.rear_wheel_z = wire.rear_wheel_z;
    state.bike_lean = wire.bike_lean;
    state.front_suspension = wire.front_suspension;
    state.rear_suspension = wire.rear_suspension;
    return state;
}

float blend(float from, float to, float alpha) {
    return from + (to - from) * alpha;
}

RiderState interpolate(const RiderState& previous, const RiderState& current, float alpha) {
    if (!previous.active || previous.tick >= current.tick) {
        return current;
    }
    RiderState result = current;
    result.position_x = blend(previous.position_x, current.position_x, alpha);
    result.position_y = blend(previous.position_y, current.position_y, alpha);
    result.position_z = blend(previous.position_z, current.position_z, alpha);
    result.front_wheel_x = blend(previous.front_wheel_x, current.front_wheel_x, alpha);
    result.front_wheel_y = blend(previous.front_wheel_y, current.front_wheel_y, alpha);
    result.front_wheel_z = blend(previous.front_wheel_z, current.front_wheel_z, alpha);
    result.rear_wheel_x = blend(previous.rear_wheel_x, current.rear_wheel_x, alpha);
    result.rear_wheel_y = blend(previous.rear_wheel_y, current.rear_wheel_y, alpha);
    result.rear_wheel_z = blend(previous.rear_wheel_z, current.rear_wheel_z, alpha);
    result.bike_lean = blend(previous.bike_lean, current.bike_lean, alpha);
    result.front_suspension = blend(previous.front_suspension, current.front_suspension, alpha);
    result.rear_suspension = blend(previous.rear_suspension, current.rear_suspension, alpha);
    return result;
}

static_assert(sizeof(LobbySnapshotPacket) <= 1200, "Lobby snapshots must avoid UDP fragmentation");
static_assert(sizeof(RaceSnapshotPacket) <= 1200, "Race snapshots must avoid UDP fragmentation");
static_assert(sizeof(VoicePacket) <= 1200, "Voice packets must avoid UDP fragmentation");

template <typename Packet>
void initialize_packet(Packet& packet, PacketType type) {
    packet.header.magic = kProtocolMagic;
    packet.header.version = kProtocolVersion;
    packet.header.type = type;
    packet.header.size = static_cast<std::uint16_t>(sizeof(Packet));
    packet.header.sequence = g_session.sequence++;
    packet.header.session = g_session.token;
}

bool endpoint_equal(const sockaddr_in& left, const sockaddr_in& right) {
    return left.sin_family == right.sin_family &&
        left.sin_port == right.sin_port &&
        left.sin_addr.s_addr == right.sin_addr.s_addr;
}

void close_socket_locked() {
    if (g_session.socket != INVALID_SOCKET) {
        closesocket(g_session.socket);
        g_session.socket = INVALID_SOCKET;
    }
}
void connection_failure_locked(const std::string& message) {
    std::fprintf(stderr,"[RR64-NET] failed: %s hello-attempts=%llu received=%llu last-socket-error=%d\n",
        message.c_str(),static_cast<unsigned long long>(g_session.hello_attempts),
        static_cast<unsigned long long>(g_session.received_packets),g_session.last_socket_error);
    std::fflush(stderr);
    close_socket_locked();g_session.phase=Phase::Offline;g_session.config.mode=Mode::Offline;
    g_session.local_slot=kInvalidSlot;g_session.token=0;g_session.players={};g_session.inputs={};
    g_session.riders={};g_session.previous_riders={};g_session.message=message;
}
void record_socket_error_locked(const char* operation,int error) {
    if(error==WSAEWOULDBLOCK)return;
    if(g_session.last_socket_error!=error){
        std::fprintf(stderr,"[RR64-NET] %s error=%d\n",operation,error);std::fflush(stderr);
    }
    g_session.last_socket_error=error;
}

bool start_winsock_locked() {
    if (g_winsock_started) {
        return true;
    }
#ifdef _WIN32
    WSADATA data{};
    if (WSAStartup(MAKEWORD(2, 2), &data) != 0) {
        return false;
    }
#endif
    g_winsock_started = true;
    return true;
}

bool create_socket_locked(bool bind_host, std::uint16_t port) {
    if (!start_winsock_locked()) {
        g_session.message = "Could not initialize Windows networking";
        return false;
    }

    g_session.socket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (g_session.socket == INVALID_SOCKET) {
        g_session.message = "Could not create UDP socket";
        return false;
    }

    u_long nonblocking = 1;
    if (ioctlsocket(g_session.socket, FIONBIO, &nonblocking) == SOCKET_ERROR) {
        close_socket_locked();
        g_session.message = "Could not configure UDP socket";
        return false;
    }

    sockaddr_in local{};
    local.sin_family = AF_INET;
    local.sin_addr.s_addr = htonl(INADDR_ANY);
    local.sin_port = htons(bind_host ? port : 0);
#ifdef _WIN32
    if(bind_host){
        BOOL exclusive=TRUE;
        if(setsockopt(g_session.socket,SOL_SOCKET,SO_EXCLUSIVEADDRUSE,reinterpret_cast<const char*>(&exclusive),sizeof(exclusive))==SOCKET_ERROR){
            record_socket_error_locked("exclusive-bind",WSAGetLastError());close_socket_locked();
            g_session.message="Could not reserve the hosting port.";return false;
        }
    }
#endif
    if (bind(g_session.socket, reinterpret_cast<const sockaddr*>(&local), sizeof(local)) == SOCKET_ERROR) {
        record_socket_error_locked("bind",WSAGetLastError());
        close_socket_locked();
        g_session.message = "Could not bind UDP port " + std::to_string(port);
        return false;
    }
    socklen_type length=sizeof(local);getsockname(g_session.socket,reinterpret_cast<sockaddr*>(&local),&length);
    std::fprintf(stderr,"[RR64-NET] socket role=%s local-udp-port=%u protocol=%u\n",bind_host?"host":"join",ntohs(local.sin_port),kProtocolVersion);std::fflush(stderr);
    return true;
}

bool resolve_host(const std::string& address, std::uint16_t port, sockaddr_in& endpoint) {
    addrinfo hints{};
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_DGRAM;
    hints.ai_protocol = IPPROTO_UDP;
    addrinfo* results = nullptr;
    const std::string port_text = std::to_string(port);
    if (getaddrinfo(address.c_str(), port_text.c_str(), &hints, &results) != 0 || results == nullptr) {
        return false;
    }
    endpoint = *reinterpret_cast<sockaddr_in*>(results->ai_addr);
    freeaddrinfo(results);
    return true;
}

template <typename Packet>
void send_packet_locked(const Packet& packet, const sockaddr_in& endpoint) {
    if (g_session.socket == INVALID_SOCKET) {
        return;
    }
    const int sent=sendto(
        g_session.socket,
        reinterpret_cast<const char*>(&packet),
        static_cast<int>(sizeof(Packet)),
        0,
        reinterpret_cast<const sockaddr*>(&endpoint),
        sizeof(endpoint));
    if(sent==SOCKET_ERROR)record_socket_error_locked("send",WSAGetLastError());
}

std::size_t voice_packet_size(const VoicePacket& packet) {
    return offsetof(VoicePacket, payload) + packet.payload_size;
}

bool valid_voice_packet(const VoicePacket& packet, int size) {
    return packet.payload_size > 0 && packet.payload_size <= kVoicePayloadCapacity &&
        size == static_cast<int>(voice_packet_size(packet));
}

void send_voice_packet_locked(VoicePacket& packet, const sockaddr_in& endpoint) {
    if (g_session.socket == INVALID_SOCKET) {
        return;
    }
    const std::size_t size = voice_packet_size(packet);
    packet.header.size = static_cast<std::uint16_t>(size);
    sendto(
        g_session.socket,
        reinterpret_cast<const char*>(&packet),
        static_cast<int>(size),
        0,
        reinterpret_cast<const sockaddr*>(&endpoint),
        sizeof(endpoint));
}

void enqueue_voice_frame_locked(const VoicePacket& packet) {
    if (g_session.received_voice_frames.size() >= kMaximumQueuedVoiceFrames) {
        g_session.received_voice_frames.pop_front();
    }
    ReceivedVoiceFrame frame{};
    frame.speaker_slot = packet.speaker_slot;
    frame.sequence = packet.voice_sequence;
    frame.payload_size = packet.payload_size;
    std::copy_n(packet.payload, packet.payload_size, frame.payload.begin());
    g_session.received_voice_frames.push_back(std::move(frame));
}

std::uint8_t connected_count_locked() {
    return static_cast<std::uint8_t>(std::count_if(
        g_session.players.begin(), g_session.players.end(),
        [](const PlayerInfo& player) { return player.connected; }));
}

std::uint8_t find_peer_slot_locked(const sockaddr_in& endpoint) {
    for (std::uint8_t slot = 1; slot < kMaximumPlayers; ++slot) {
        if (g_session.peers[slot].connected && endpoint_equal(g_session.peers[slot].endpoint, endpoint)) {
            return slot;
        }
    }
    return kInvalidSlot;
}

std::uint8_t allocate_peer_slot_locked() {
    for (std::uint8_t slot = 1; slot < kMaximumPlayers; ++slot) {
        if (!g_session.peers[slot].connected) {
            return slot;
        }
    }
    return kInvalidSlot;
}

void send_welcome_locked(std::uint8_t slot) {
    WelcomePacket packet{};
    initialize_packet(packet, PacketType::Welcome);
    packet.assigned_slot = slot;
    packet.phase = static_cast<std::uint8_t>(g_session.phase);
    packet.replicated_riders = g_session.replicated_riders ? 1 : 0;
    send_packet_locked(packet, g_session.peers[slot].endpoint);
}

void send_hello_locked(const Clock::time_point now) {
    HelloPacket packet{};
    initialize_packet(packet, PacketType::Hello);
    packet.header.session = 0;
    copy_string(packet.player_name, sizeof(packet.player_name), g_session.config.player_name);
    send_packet_locked(packet, g_session.host_endpoint);
    g_session.last_hello = now;
    ++g_session.hello_attempts;
    if(g_session.hello_attempts==1 || g_session.hello_attempts%10==0){
        std::fprintf(stderr,"[RR64-NET] hello attempt=%llu received=%llu\n",static_cast<unsigned long long>(g_session.hello_attempts),static_cast<unsigned long long>(g_session.received_packets));std::fflush(stderr);
    }
}

void send_client_state_locked(const Clock::time_point now) {
    if (g_session.local_slot == kInvalidSlot) {
        return;
    }
    const std::uint8_t slot = g_session.local_slot;
    ClientStatePacket packet{};
    initialize_packet(packet, PacketType::ClientState);
    packet.slot = slot;
    packet.ready = g_session.players[slot].ready ? 1 : 0;
    packet.character = g_session.players[slot].character;
    packet.track = g_session.players[slot].track;
    packet.buttons = g_session.inputs[slot].buttons;
    packet.stick_x = static_cast<std::int8_t>(std::lround(std::clamp(g_session.inputs[slot].stick_x, -1.0f, 1.0f) * 127.0f));
    packet.stick_y = static_cast<std::int8_t>(std::lround(std::clamp(g_session.inputs[slot].stick_y, -1.0f, 1.0f) * 127.0f));
    copy_string(packet.player_name, sizeof(packet.player_name), g_session.players[slot].name);
    send_packet_locked(packet, g_session.host_endpoint);
    g_session.last_state = now;
}

void send_snapshot_locked(const Clock::time_point now) {
    LobbySnapshotPacket packet{};
    initialize_packet(packet, PacketType::LobbySnapshot);
    packet.phase = static_cast<std::uint8_t>(g_session.phase);
    packet.connected_players = connected_count_locked();
    packet.reserved=static_cast<std::uint8_t>(std::clamp<unsigned>(g_session.config.maximum_players,2u,kMaximumPlayers));
    packet.replicated_riders = g_session.replicated_riders ? 1 : 0;
    packet.game_setup.revision = g_session.game_setup.revision;
    packet.game_setup.transition_buttons = g_session.game_setup.transition_buttons;
    packet.game_setup.valid = g_session.game_setup.valid ? 1 : 0;
    std::copy(
        g_session.game_setup.words.begin(),
        g_session.game_setup.words.end(),
        packet.game_setup.words);
    for (std::uint8_t slot = 0; slot < kMaximumPlayers; ++slot) {
        const PlayerInfo& player = g_session.players[slot];
        WirePlayer& wire = packet.players[slot];
        wire.connected = player.connected ? 1 : 0;
        wire.ready = player.ready ? 1 : 0;
        wire.character = player.character;
        wire.track = player.track;
        wire.buttons = g_session.inputs[slot].buttons;
        wire.stick_x = static_cast<std::int8_t>(std::lround(std::clamp(g_session.inputs[slot].stick_x, -1.0f, 1.0f) * 127.0f));
        wire.stick_y = static_cast<std::int8_t>(std::lround(std::clamp(g_session.inputs[slot].stick_y, -1.0f, 1.0f) * 127.0f));
        wire.ping_ms = player.ping_ms;
        copy_string(wire.name, sizeof(wire.name), player.name);
    }
    for (std::uint8_t slot = 1; slot < kMaximumPlayers; ++slot) {
        if (g_session.peers[slot].connected) {
            send_packet_locked(packet, g_session.peers[slot].endpoint);
        }
    }
    g_session.last_snapshot = now;
}

void send_client_rider_state_locked(const Clock::time_point now) {
    if (g_session.local_slot >= kMaximumPlayers) {
        return;
    }
    const RiderState& state = g_session.riders[g_session.local_slot];
    if (!state.active || !finite_rider_state(state)) {
        return;
    }
    ClientRiderStatePacket packet{};
    initialize_packet(packet, PacketType::ClientRiderState);
    packet.slot = g_session.local_slot;
    packet.state = to_wire(state);
    send_packet_locked(packet, g_session.host_endpoint);
    g_session.last_race_state = now;
}

void send_race_snapshot_locked(const Clock::time_point now) {
    RaceSnapshotPacket packet{};
    initialize_packet(packet, PacketType::RaceSnapshot);
    for (std::uint8_t slot = 0; slot < kMaximumPlayers; ++slot) {
        packet.host_tick = std::max(packet.host_tick, g_session.riders[slot].tick);
        packet.riders[slot] = to_wire(g_session.riders[slot]);
    }
    for (std::uint8_t slot = 1; slot < kMaximumPlayers; ++slot) {
        if (g_session.peers[slot].connected) {
            send_packet_locked(packet, g_session.peers[slot].endpoint);
        }
    }
    g_session.last_race_snapshot = now;
}

void send_ping_locked(std::uint8_t slot, const sockaddr_in& endpoint, const Clock::time_point now) {
    PingPacket packet{};
    initialize_packet(packet, PacketType::Ping);
    packet.slot = slot;
    packet.nonce = g_session.sequence ^ static_cast<std::uint32_t>(
        std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count());
    if (is_host_locked()) {
        g_session.peers[slot].ping_nonce = packet.nonce;
        g_session.peers[slot].ping_sent = now;
    }
    else {
        g_session.client_ping_nonce = packet.nonce;
        g_session.client_ping_sent = now;
    }
    send_packet_locked(packet, endpoint);
}

bool valid_packet(const PacketHeader& header, int received_size) {
    return received_size >= static_cast<int>(sizeof(PacketHeader)) &&
        header.magic == kProtocolMagic &&
        header.version == kProtocolVersion &&
        header.size == received_size;
}
void send_reject_locked(const sockaddr_in& endpoint,RejectReason reason){
    ConnectRejectPacket packet{};initialize_packet(packet,PacketType::ConnectReject);
    packet.header.session=0;packet.reason=reason;send_packet_locked(packet,endpoint);
}

void handle_host_packet_locked(const std::uint8_t* bytes, int size, const sockaddr_in& endpoint, const Clock::time_point now) {
    const PacketHeader& header = *reinterpret_cast<const PacketHeader*>(bytes);
    if (header.type == PacketType::Hello && size == sizeof(HelloPacket)) {
        std::uint8_t slot = find_peer_slot_locked(endpoint);
        if (slot == kInvalidSlot) {
            if(g_session.phase!=Phase::Lobby){send_reject_locked(endpoint,RejectReason::Busy);return;}
            if(connected_count_locked()>=std::clamp<unsigned>(g_session.config.maximum_players,2u,kMaximumPlayers)){
                send_reject_locked(endpoint,RejectReason::Full);return;
            }
            slot = allocate_peer_slot_locked();
            if (slot == kInvalidSlot) {
                return;
            }
            g_session.peers[slot].connected = true;
            g_session.peers[slot].endpoint = endpoint;
            g_session.players[slot].connected = true;
            g_session.players[slot].slot = slot;
            g_session.players[slot].ready = false;
            std::fprintf(stderr,"[RR64-NET] hello accepted slot=%u peer=%s:%u\n",slot,inet_ntoa(endpoint.sin_addr),ntohs(endpoint.sin_port));std::fflush(stderr);
        }
        g_session.peers[slot].last_seen = now;
        const auto& hello = *reinterpret_cast<const HelloPacket*>(bytes);
        g_session.players[slot].name = read_string(hello.player_name, sizeof(hello.player_name));
        if (g_session.players[slot].name.empty()) {
            g_session.players[slot].name = "Rider " + std::to_string(slot + 1);
        }
        send_welcome_locked(slot);
        send_snapshot_locked(now);
        g_session.message = "Lobby open - " + std::to_string(connected_count_locked()) + " / " +
            std::to_string(kMaximumPlayers) + " riders";
        return;
    }

    if (header.session != g_session.token) {
        return;
    }
    const std::uint8_t slot = find_peer_slot_locked(endpoint);
    if (slot == kInvalidSlot) {
        return;
    }
    g_session.peers[slot].last_seen = now;

    if (header.type == PacketType::ClientState && size == sizeof(ClientStatePacket)) {
        const auto& state = *reinterpret_cast<const ClientStatePacket*>(bytes);
        if (state.slot != slot) {
            return;
        }
        PlayerInfo& player = g_session.players[slot];
        player.ready = state.ready != 0;
        player.character = state.character;
        player.track = state.track;
        player.name = read_string(state.player_name, sizeof(state.player_name));
        g_session.inputs[slot].buttons = state.buttons;
        g_session.inputs[slot].stick_x = static_cast<float>(state.stick_x) / 127.0f;
        g_session.inputs[slot].stick_y = static_cast<float>(state.stick_y) / 127.0f;
    }
    else if (header.type == PacketType::ClientRiderState && size == sizeof(ClientRiderStatePacket)) {
        const auto& packet = *reinterpret_cast<const ClientRiderStatePacket*>(bytes);
        if (packet.slot != slot || g_session.phase != Phase::Race) {
            return;
        }
        RiderState proposed = from_wire(packet.state);
        if (!proposed.active || !finite_rider_state(proposed) || proposed.tick <= g_session.riders[slot].tick) {
            return;
        }
        // The host owns the canonical array and accepts only the state carried
        // by the authenticated endpoint assigned to this slot. Game-level
        // sanity checks can be layered here without changing the wire format.
        g_session.previous_riders[slot] = g_session.riders[slot];
        g_session.riders[slot] = proposed;
    }
    else if (header.type == PacketType::Voice &&
             size >= static_cast<int>(offsetof(VoicePacket, payload))) {
        const auto& incoming = *reinterpret_cast<const VoicePacket*>(bytes);
        Peer& peer = g_session.peers[slot];
        if (peer.voice_window_start.time_since_epoch().count() == 0 ||
            now - peer.voice_window_start >= std::chrono::seconds(1)) {
            peer.voice_window_start = now;
            peer.voice_packets_in_window = 0;
        }
        if (g_session.phase != Phase::Race || incoming.speaker_slot != slot ||
            !valid_voice_packet(incoming, size) ||
            incoming.voice_sequence <= g_session.last_voice_sequences[slot] ||
            peer.voice_packets_in_window >= kMaximumVoicePacketsPerSecond) {
            return;
        }

        ++peer.voice_packets_in_window;
        g_session.last_voice_sequences[slot] = incoming.voice_sequence;
        VoicePacket canonical = incoming;
        initialize_packet(canonical, PacketType::Voice);
        canonical.speaker_slot = slot;
        for (std::uint8_t destination = 1; destination < kMaximumPlayers; ++destination) {
            if (destination != slot && g_session.peers[destination].connected) {
                send_voice_packet_locked(canonical, g_session.peers[destination].endpoint);
            }
        }
        enqueue_voice_frame_locked(canonical);
    }
    else if (header.type == PacketType::Ping && size == sizeof(PingPacket)) {
        PingPacket pong = *reinterpret_cast<const PingPacket*>(bytes);
        pong.header.type = PacketType::Pong;
        pong.header.sequence = g_session.sequence++;
        send_packet_locked(pong, endpoint);
    }
    else if (header.type == PacketType::Pong && size == sizeof(PingPacket)) {
        const auto& pong = *reinterpret_cast<const PingPacket*>(bytes);
        Peer& peer = g_session.peers[slot];
        if (pong.nonce == peer.ping_nonce) {
            const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - peer.ping_sent).count();
            g_session.players[slot].ping_ms = static_cast<std::uint16_t>(std::clamp<std::int64_t>(elapsed, 0, 999));
        }
    }
    else if (header.type == PacketType::Disconnect) {
        g_session.peers[slot] = {};
        g_session.players[slot] = {};
        g_session.inputs[slot] = {};
        g_session.riders[slot] = {};
        g_session.previous_riders[slot] = {};
    }
}

void handle_client_packet_locked(const std::uint8_t* bytes, int size, const sockaddr_in& endpoint, const Clock::time_point now) {
    if (!endpoint_equal(endpoint, g_session.host_endpoint)) {
        return;
    }
    const PacketHeader& header = *reinterpret_cast<const PacketHeader*>(bytes);
    if(g_session.phase==Phase::Connecting && header.type==PacketType::ConnectReject && size==sizeof(ConnectRejectPacket)){
        const auto& rejection=*reinterpret_cast<const ConnectRejectPacket*>(bytes);
        switch(rejection.reason){
        case RejectReason::Full:connection_failure_locked("The host's lobby is full.");break;
        case RejectReason::Busy:connection_failure_locked("The host has already started. Return both games to the lobby.");break;
        case RejectReason::Version:connection_failure_locked("Online versions differ. Run the same build on both PCs.");break;
        default:break;
        }
        return;
    }
    if (header.type == PacketType::Welcome && size == sizeof(WelcomePacket)) {
        if(g_session.phase!=Phase::Connecting || g_session.local_slot!=kInvalidSlot)return;
        const auto& welcome = *reinterpret_cast<const WelcomePacket*>(bytes);
        if (welcome.assigned_slot == 0 || welcome.assigned_slot >= kMaximumPlayers || welcome.maximum_players != kMaximumPlayers ||
            header.session==0 || welcome.phase!=static_cast<std::uint8_t>(Phase::Lobby)) {
            return;
        }
        g_session.token = header.session;
        g_session.local_slot = welcome.assigned_slot;
        g_session.phase = static_cast<Phase>(welcome.phase);
        g_session.replicated_riders = welcome.replicated_riders != 0;
        PlayerInfo& local = g_session.players[g_session.local_slot];
        local.connected = true;
        local.slot = g_session.local_slot;
        local.name = g_session.config.player_name;
        g_session.message = "Connected to host";
        g_session.last_host_seen=now;
        std::fprintf(stderr,"[RR64-NET] welcome accepted slot=%u attempts=%llu\n",g_session.local_slot,static_cast<unsigned long long>(g_session.hello_attempts));std::fflush(stderr);
        send_client_state_locked(now);
        return;
    }
    if (g_session.token == 0 || header.session != g_session.token) {
        return;
    }
    g_session.last_host_seen=now;

    if (header.type == PacketType::LobbySnapshot && size == sizeof(LobbySnapshotPacket)) {
        const auto& snapshot = *reinterpret_cast<const LobbySnapshotPacket*>(bytes);
        if (snapshot.phase > static_cast<std::uint8_t>(Phase::Race)) {
            return;
        }
        if (header.sequence <= g_session.last_lobby_snapshot_sequence) {
            return;
        }
        g_session.last_lobby_snapshot_sequence = header.sequence;
        const Phase incoming_phase = static_cast<Phase>(snapshot.phase);
        if (incoming_phase != g_session.phase) {
            g_session.received_voice_frames.clear();
            g_session.last_voice_sequences = {};
            g_session.local_voice_sequence = 0;
        }
        g_session.phase = incoming_phase;
        g_session.replicated_riders = snapshot.replicated_riders != 0;
        g_session.config.maximum_players=static_cast<std::uint8_t>(std::clamp<unsigned>(snapshot.reserved,2u,kMaximumPlayers));
        g_session.game_setup.valid = snapshot.game_setup.valid != 0;
        g_session.game_setup.revision = snapshot.game_setup.revision;
        g_session.game_setup.transition_buttons = snapshot.game_setup.transition_buttons;
        std::copy(
            std::begin(snapshot.game_setup.words),
            std::end(snapshot.game_setup.words),
            g_session.game_setup.words.begin());
        for (std::uint8_t slot = 0; slot < kMaximumPlayers; ++slot) {
            const WirePlayer& wire = snapshot.players[slot];
            PlayerInfo& player = g_session.players[slot];
            player.connected = wire.connected != 0;
            player.ready = wire.ready != 0;
            player.slot = player.connected ? slot : kInvalidSlot;
            player.character = wire.character;
            player.track = wire.track;
            player.ping_ms = wire.ping_ms;
            player.name = read_string(wire.name, sizeof(wire.name));
            g_session.inputs[slot].buttons = wire.buttons;
            g_session.inputs[slot].stick_x = static_cast<float>(wire.stick_x) / 127.0f;
            g_session.inputs[slot].stick_y = static_cast<float>(wire.stick_y) / 127.0f;
        }
        g_session.message = "Connected - " + std::to_string(snapshot.connected_players) + " / " +
            std::to_string(kMaximumPlayers) + " riders";
    }
    else if (header.type == PacketType::RaceSnapshot && size == sizeof(RaceSnapshotPacket)) {
        if (g_session.phase != Phase::Race || header.sequence <= g_session.last_race_snapshot_sequence) {
            return;
        }
        const auto& snapshot = *reinterpret_cast<const RaceSnapshotPacket*>(bytes);
        std::array<RiderState, kMaximumPlayers> incoming{};
        for (std::uint8_t slot = 0; slot < kMaximumPlayers; ++slot) {
            incoming[slot] = from_wire(snapshot.riders[slot]);
            if (incoming[slot].active && !finite_rider_state(incoming[slot])) {
                return;
            }
        }
        g_session.last_race_snapshot_sequence = header.sequence;
        for (std::uint8_t slot = 0; slot < kMaximumPlayers; ++slot) {
            if (incoming[slot].tick >= g_session.riders[slot].tick) {
                g_session.previous_riders[slot] = g_session.riders[slot];
                g_session.riders[slot] = incoming[slot];
            }
        }
    }
    else if (header.type == PacketType::Voice &&
             size >= static_cast<int>(offsetof(VoicePacket, payload))) {
        const auto& packet = *reinterpret_cast<const VoicePacket*>(bytes);
        if (g_session.phase != Phase::Race || !valid_voice_packet(packet, size) ||
            packet.speaker_slot >= kMaximumPlayers ||
            packet.speaker_slot == g_session.local_slot ||
            packet.voice_sequence <= g_session.last_voice_sequences[packet.speaker_slot]) {
            return;
        }
        g_session.last_voice_sequences[packet.speaker_slot] = packet.voice_sequence;
        enqueue_voice_frame_locked(packet);
    }
    else if (header.type == PacketType::Ping && size == sizeof(PingPacket)) {
        PingPacket pong = *reinterpret_cast<const PingPacket*>(bytes);
        pong.header.type = PacketType::Pong;
        pong.header.sequence = g_session.sequence++;
        send_packet_locked(pong, g_session.host_endpoint);
    }
    else if (header.type == PacketType::Pong && size == sizeof(PingPacket)) {
        const auto& pong = *reinterpret_cast<const PingPacket*>(bytes);
        if (pong.nonce == g_session.client_ping_nonce && g_session.local_slot < kMaximumPlayers) {
            const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - g_session.client_ping_sent).count();
            g_session.players[g_session.local_slot].ping_ms = static_cast<std::uint16_t>(std::clamp<std::int64_t>(elapsed, 0, 999));
        }
    }
    else if (header.type == PacketType::Disconnect) {
        g_session.message = "Host ended the session";
        g_session.phase = Phase::Connecting;
        g_session.token = 0;
        g_session.local_slot = kInvalidSlot;
        g_session.riders = {};
        g_session.previous_riders = {};
    }
}

void pump_receive_locked(const Clock::time_point now) {
    if (g_session.socket == INVALID_SOCKET) {
        return;
    }
    std::array<std::uint8_t, 2048> buffer{};
    for(unsigned packets=0;packets<256;++packets) {
        sockaddr_in endpoint{};
        socklen_type endpoint_size = sizeof(endpoint);
        const int received = recvfrom(
            g_session.socket,
            reinterpret_cast<char*>(buffer.data()),
            static_cast<int>(buffer.size()),
            0,
            reinterpret_cast<sockaddr*>(&endpoint),
            &endpoint_size);
        if (received == SOCKET_ERROR) {
            const int error = WSAGetLastError();
            if (error != WSAEWOULDBLOCK) {
                record_socket_error_locked("receive",error);
                g_session.message = "UDP receive error " + std::to_string(error);
            }
            break;
        }
        if (received < static_cast<int>(sizeof(PacketHeader))) {
            continue;
        }
        const auto& header = *reinterpret_cast<const PacketHeader*>(buffer.data());
        ++g_session.received_packets;
        if(header.magic==kProtocolMagic && header.size==received && header.version!=kProtocolVersion){
            if(is_host_locked() && header.type==PacketType::Hello && received==sizeof(HelloPacket))send_reject_locked(endpoint,RejectReason::Version);
            else if(is_client_locked() && g_session.phase==Phase::Connecting && endpoint_equal(endpoint,g_session.host_endpoint)){
                connection_failure_locked("Online versions differ. Run the same build on both PCs.");return;
            }
            continue;
        }
        if (!valid_packet(header, received)) {
            continue;
        }
        if (is_host_locked()) {
            handle_host_packet_locked(buffer.data(), received, endpoint, now);
        }
        else if (is_client_locked()) {
            handle_client_packet_locked(buffer.data(), received, endpoint, now);
        }
        if(g_session.socket==INVALID_SOCKET)return;
    }
}

void expire_peers_locked(const Clock::time_point now) {
    if (!is_host_locked()) {
        return;
    }
    for (std::uint8_t slot = 1; slot < kMaximumPlayers; ++slot) {
        if (g_session.peers[slot].connected && now - g_session.peers[slot].last_seen > kPeerTimeout) {
            g_session.peers[slot] = {};
            g_session.players[slot] = {};
            g_session.inputs[slot] = {};
            g_session.riders[slot] = {};
            g_session.previous_riders[slot] = {};
        }
    }
}

void service_locked(const Clock::time_point now) {
    if (g_session.config.mode == Mode::Offline || g_session.socket == INVALID_SOCKET) {
        return;
    }
    pump_receive_locked(now);
    if(g_session.socket==INVALID_SOCKET)return;
    expire_peers_locked(now);

    if (is_client_locked()) {
        if(g_session.local_slot==kInvalidSlot && now-g_session.connect_started>=kConnectTimeout){
            connection_failure_locked("No host handshake after 15 seconds. Check the host address, UDP port and Windows firewall for this build.");return;
        }
        if(g_session.local_slot!=kInvalidSlot && now-g_session.last_host_seen>=kPeerTimeout){
            connection_failure_locked("Connection to host lost. Return to Host / Join to reconnect.");return;
        }
        if (g_session.local_slot == kInvalidSlot && now - g_session.last_hello >= kHelloInterval) {
            send_hello_locked(now);
        }
        else if (g_session.local_slot != kInvalidSlot && now - g_session.last_state >= kStateInterval) {
            send_client_state_locked(now);
        }
        if (g_session.local_slot != kInvalidSlot && g_session.phase == Phase::Race &&
            now - g_session.last_race_state >= kRaceStateInterval) {
            send_client_rider_state_locked(now);
        }
        if (g_session.local_slot != kInvalidSlot && now - g_session.last_ping >= kPingInterval) {
            send_ping_locked(g_session.local_slot, g_session.host_endpoint, now);
            g_session.last_ping = now;
        }
    }
    else {
        if (now - g_session.last_snapshot >= kSnapshotInterval) {
            send_snapshot_locked(now);
        }
        if (g_session.phase == Phase::Race && now - g_session.last_race_snapshot >= kRaceSnapshotInterval) {
            send_race_snapshot_locked(now);
        }
        if (now - g_session.last_ping >= kPingInterval) {
            for (std::uint8_t slot = 1; slot < kMaximumPlayers; ++slot) {
                if (g_session.peers[slot].connected) {
                    send_ping_locked(slot, g_session.peers[slot].endpoint, now);
                }
            }
            g_session.last_ping = now;
        }
    }
}

} // namespace

void configure(const Config& requested) {
    std::lock_guard lock(g_mutex);
    close_socket_locked();
    g_session = {};
    g_session.config = requested;
    if (g_session.config.player_name.empty()) {
        g_session.config.player_name = "Rider";
    }
    if (g_session.config.player_name.size() >= kPlayerNameCapacity) {
        g_session.config.player_name.resize(kPlayerNameCapacity - 1);
    }

    if (requested.mode == Mode::Offline) {
        g_session.phase = Phase::Offline;
        g_session.message = "Offline";
        return;
    }
    if(requested.mode==Mode::Join){
        std::string host,error;
        if(!parse_connection_address(requested.host_address,g_session.config.port,host,error)){connection_failure_locked(error);return;}
        g_session.config.host_address=host;
    }
    if(!g_session.config.port){connection_failure_locked("Enter a UDP port from 1 to 65535.");return;}

    const bool host = requested.mode == Mode::Host;
    if (!create_socket_locked(host, g_session.config.port)) {
        g_session.config.mode = Mode::Offline;
        g_session.phase = Phase::Offline;
        return;
    }

    const Clock::time_point now = Clock::now();
    g_session.connect_started=now;
    g_session.last_hello = now - kHelloInterval;
    g_session.last_state = now - kStateInterval;
    g_session.last_snapshot = now - kSnapshotInterval;
    g_session.last_race_state = now - kRaceStateInterval;
    g_session.last_race_snapshot = now - kRaceSnapshotInterval;
    g_session.last_ping = now - kPingInterval;

    if (host) {
        g_session.token = make_session_token();
        g_session.local_slot = 0;
        g_session.phase = Phase::Lobby;
        g_session.players[0].connected = true;
        g_session.players[0].slot = 0;
        g_session.players[0].name = g_session.config.player_name;
        g_session.message = "Lobby open on UDP port " + std::to_string(g_session.config.port);
    }
    else {
        if (!resolve_host(g_session.config.host_address, g_session.config.port, g_session.host_endpoint)) {
            close_socket_locked();
            g_session.config.mode = Mode::Offline;
            g_session.phase = Phase::Offline;
            g_session.message = "Could not resolve host address";
            return;
        }
        g_session.phase = Phase::Connecting;
        g_session.message = "Connecting to " + g_session.config.host_address + ":" + std::to_string(g_session.config.port);
        std::fprintf(stderr,"[RR64-NET] destination=%s:%u protocol=%u\n",inet_ntoa(g_session.host_endpoint.sin_addr),ntohs(g_session.host_endpoint.sin_port),kProtocolVersion);std::fflush(stderr);
    }
}

void shutdown() {
    std::lock_guard lock(g_mutex);
    if (g_session.socket != INVALID_SOCKET && g_session.token != 0) {
        PacketHeader packet{};
        packet.type = PacketType::Disconnect;
        packet.size = sizeof(packet);
        packet.sequence = g_session.sequence++;
        packet.session = g_session.token;
        if (is_host_locked()) {
            for (std::uint8_t slot = 1; slot < kMaximumPlayers; ++slot) {
                if (g_session.peers[slot].connected) {
                    send_packet_locked(packet, g_session.peers[slot].endpoint);
                }
            }
        }
        else if (is_client_locked()) {
            send_packet_locked(packet, g_session.host_endpoint);
        }
    }
    close_socket_locked();
    g_session = {};
}

void update() {
    std::lock_guard lock(g_mutex);
    service_locked(Clock::now());
}

Status get_status() {
    std::lock_guard lock(g_mutex);
    Status status{};
    status.maximum_players=g_session.config.maximum_players;
    status.active = g_session.config.mode != Mode::Offline && g_session.socket != INVALID_SOCKET;
    status.connected = is_host_locked() || g_session.local_slot != kInvalidSlot;
    status.is_host = is_host_locked();
    status.replicated_riders = g_session.replicated_riders;
    status.local_slot = g_session.local_slot;
    status.connected_players = connected_count_locked();
    status.phase = g_session.phase;
    status.game_setup = g_session.game_setup;
    status.message = g_session.message;
    status.players = g_session.players;
    return status;
}

bool set_ready(bool ready) {
    std::lock_guard lock(g_mutex);
    if (g_session.local_slot >= kMaximumPlayers || !g_session.players[g_session.local_slot].connected) {
        return false;
    }
    g_session.players[g_session.local_slot].ready = ready;
    return true;
}

bool set_character(std::uint8_t character) {
    std::lock_guard lock(g_mutex);
    if (g_session.local_slot >= kMaximumPlayers || !g_session.players[g_session.local_slot].connected) {
        return false;
    }
    g_session.players[g_session.local_slot].character = character;
    return true;
}

bool set_track(std::uint8_t track) {
    std::lock_guard lock(g_mutex);
    if (g_session.local_slot >= kMaximumPlayers || !g_session.players[g_session.local_slot].connected) {
        return false;
    }
    g_session.players[g_session.local_slot].track = track;
    return true;
}

bool all_connected_players_ready() {
    std::lock_guard lock(g_mutex);
    const std::uint8_t count = connected_count_locked();
    return count > 0 && std::all_of(
        g_session.players.begin(), g_session.players.end(),
        [](const PlayerInfo& player) { return !player.connected || player.ready; });
}

bool host_set_phase(Phase phase) {
    std::lock_guard lock(g_mutex);
    if (!is_host_locked() || phase <= Phase::Connecting || phase > Phase::Race) {
        return false;
    }
    if (phase == Phase::Lobby) {
        g_session.replicated_riders = false;
        g_session.game_setup = {};
    }
    else if (phase == Phase::GameSetup) {
        g_session.replicated_riders = connected_count_locked() > kMaximumLocalControllers;
        g_session.game_setup = {};
    }
    if (phase != g_session.phase) {
        g_session.received_voice_frames.clear();
        g_session.last_voice_sequences = {};
        g_session.local_voice_sequence = 0;
    }
    g_session.phase = phase;
    for (PlayerInfo& player : g_session.players) {
        if (player.connected) {
            player.ready = false;
        }
    }
    if (g_session.local_slot < kMaximumPlayers) {
        g_session.players[g_session.local_slot].ready = true;
    }
    send_snapshot_locked(Clock::now());
    return true;
}

bool host_commit_game_setup(const GameSetupState& setup) {
    std::lock_guard lock(g_mutex);
    if (!is_host_locked() || g_session.phase != Phase::GameSetup) {
        return false;
    }

    const std::uint32_t previous_revision = g_session.game_setup.revision;
    g_session.game_setup = setup;
    g_session.game_setup.valid = true;
    g_session.game_setup.revision = std::max(
        setup.revision,
        previous_revision + 1);
    g_session.phase = Phase::CharacterSelect;
    g_session.message = "Host settings locked - choose your rider and bike";
    send_snapshot_locked(Clock::now());
    return true;
}

void set_local_input(std::uint16_t buttons, float stick_x, float stick_y) {
    std::lock_guard lock(g_mutex);
    if (g_session.local_slot >= kMaximumPlayers) {
        return;
    }
    InputState& input = g_session.inputs[g_session.local_slot];
    input.buttons = buttons;
    input.stick_x = std::clamp(stick_x, -1.0f, 1.0f);
    input.stick_y = std::clamp(stick_y, -1.0f, 1.0f);
}

bool get_player_input(std::uint8_t slot, std::uint16_t& buttons, float& stick_x, float& stick_y) {
    std::lock_guard lock(g_mutex);
    if (slot >= kMaximumPlayers || !g_session.players[slot].connected) {
        buttons = 0;
        stick_x = 0.0f;
        stick_y = 0.0f;
        return false;
    }
    const InputState& input = g_session.inputs[slot];
    buttons = input.buttons;
    stick_x = input.stick_x;
    stick_y = input.stick_y;
    return true;
}

void set_local_rider_state(const RiderState& state) {
    std::lock_guard lock(g_mutex);
    if (g_session.local_slot >= kMaximumPlayers || !finite_rider_state(state)) {
        return;
    }
    RiderState canonical = state;
    canonical.active = true;
    canonical.character = g_session.players[g_session.local_slot].character;
    if (canonical.tick <= g_session.riders[g_session.local_slot].tick) {
        canonical.tick = g_session.riders[g_session.local_slot].tick + 1;
    }
    g_session.previous_riders[g_session.local_slot] = g_session.riders[g_session.local_slot];
    g_session.riders[g_session.local_slot] = canonical;
}

bool get_rider_state(std::uint8_t slot, RiderState& state) {
    std::lock_guard lock(g_mutex);
    if (slot >= kMaximumPlayers || !g_session.players[slot].connected || !g_session.riders[slot].active) {
        state = {};
        return false;
    }
    state = g_session.riders[slot];
    return true;
}

bool get_interpolated_rider_state(std::uint8_t slot, float alpha, RiderState& state) {
    std::lock_guard lock(g_mutex);
    if (slot >= kMaximumPlayers || !g_session.players[slot].connected || !g_session.riders[slot].active) {
        state = {};
        return false;
    }
    state = interpolate(
        g_session.previous_riders[slot],
        g_session.riders[slot],
        std::clamp(alpha, 0.0f, 1.0f));
    return true;
}

bool submit_local_voice(std::span<const std::uint8_t> encoded_frame) {
    std::lock_guard lock(g_mutex);
    if (g_session.socket == INVALID_SOCKET || g_session.phase != Phase::Race ||
        g_session.local_slot >= kMaximumPlayers || encoded_frame.empty() ||
        encoded_frame.size() > kVoicePayloadCapacity) {
        return false;
    }

    VoicePacket packet{};
    initialize_packet(packet, PacketType::Voice);
    packet.speaker_slot = g_session.local_slot;
    packet.payload_size = static_cast<std::uint16_t>(encoded_frame.size());
    packet.voice_sequence = ++g_session.local_voice_sequence;
    std::copy(encoded_frame.begin(), encoded_frame.end(), packet.payload);

    if (is_host_locked()) {
        for (std::uint8_t slot = 1; slot < kMaximumPlayers; ++slot) {
            if (g_session.peers[slot].connected) {
                send_voice_packet_locked(packet, g_session.peers[slot].endpoint);
            }
        }
    }
    else if (is_client_locked() && g_session.token != 0) {
        send_voice_packet_locked(packet, g_session.host_endpoint);
    }
    else {
        return false;
    }
    return true;
}

std::vector<ReceivedVoiceFrame> take_received_voice_frames() {
    std::lock_guard lock(g_mutex);
    std::vector<ReceivedVoiceFrame> frames;
    frames.reserve(g_session.received_voice_frames.size());
    while (!g_session.received_voice_frames.empty()) {
        frames.push_back(std::move(g_session.received_voice_frames.front()));
        g_session.received_voice_frames.pop_front();
    }
    return frames;
}

} // namespace rr64::netplay
