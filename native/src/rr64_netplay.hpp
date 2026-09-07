#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <vector>

namespace rr64::netplay {

constexpr std::uint16_t kDefaultPort = 6464;
// Road Rash allocates fourteen native bike and rider records. Network slots
// use that full pool, while N64 controller emulation remains limited to the
// console's four physical ports.
constexpr std::uint8_t kMaximumNetworkPlayers = 14;
constexpr std::uint8_t kMaximumLocalControllers = 4;
constexpr std::uint8_t kMaximumPlayers = kMaximumNetworkPlayers;
constexpr std::uint8_t kInvalidSlot = 0xFF;
constexpr std::size_t kVoicePayloadCapacity = 256;

enum class Mode : std::uint8_t {
    Offline = 0,
    Host,
    Join,
};

enum class Phase : std::uint8_t {
    Offline = 0,
    Connecting,
    Lobby,
    GameSetup,
    CharacterSelect,
    TrackSelect,
    Race,
};

// The game-side bridge owns the meaning and guest addresses of these words.
// Netplay only transports the host's finalized stock multiplayer settings.
constexpr std::size_t kGameSetupWordCount = 17;

struct GameSetupState {
    bool valid = false;
    std::uint32_t revision = 0;
    std::uint16_t transition_buttons = 0;
    std::array<std::uint32_t, kGameSetupWordCount> words{};

    bool operator==(const GameSetupState&) const = default;
};

struct Config {
    std::uint8_t maximum_players = kMaximumPlayers;
    Mode mode = Mode::Offline;
    std::string player_name = "Rider";
    std::string host_address = "127.0.0.1";
    std::uint16_t port = kDefaultPort;

    bool operator==(const Config&) const = default;
};

struct PlayerInfo {
    bool connected = false;
    bool ready = false;
    std::uint8_t slot = kInvalidSlot;
    std::uint8_t character = 0;
    std::uint8_t track = 0;
    std::uint16_t ping_ms = 0;
    std::string name{};
};

struct RiderState {
    bool active = false;
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
    std::uint16_t health = 0;
    std::uint16_t flags = 0;
    std::uint8_t animation = 0;
    std::uint8_t weapon = 0;
    std::uint8_t character = 0;
    std::uint8_t bike = 0;
};

struct ReceivedVoiceFrame {
    std::uint8_t speaker_slot = kInvalidSlot;
    std::uint32_t sequence = 0;
    std::uint16_t payload_size = 0;
    std::array<std::uint8_t, kVoicePayloadCapacity> payload{};
};

struct Status {
    std::uint8_t maximum_players = kMaximumPlayers;
    bool active = false;
    bool connected = false;
    bool is_host = false;
    bool replicated_riders = false;
    std::uint8_t local_slot = kInvalidSlot;
    std::uint8_t connected_players = 0;
    Phase phase = Phase::Offline;
    GameSetupState game_setup{};
    std::string message{};
    std::array<PlayerInfo, kMaximumPlayers> players{};
};

void configure(const Config& config);
void shutdown();
void update();

Status get_status();
bool set_ready(bool ready);
bool set_character(std::uint8_t character);
bool set_track(std::uint8_t track);
bool host_set_phase(Phase phase);
bool host_commit_game_setup(const GameSetupState& setup);
bool all_connected_players_ready();

void set_local_input(std::uint16_t buttons, float stick_x, float stick_y);
bool get_player_input(std::uint8_t slot, std::uint16_t& buttons, float& stick_x, float& stick_y);

// The local machine publishes its controlled rider under its network slot.
// The host turns proposals into the canonical snapshot distributed to every
// peer. Consumers may request a blend between the two newest snapshots.
void set_local_rider_state(const RiderState& state);
bool get_rider_state(std::uint8_t slot, RiderState& state);
bool get_interpolated_rider_state(std::uint8_t slot, float alpha, RiderState& state);

// Encoded voice frames follow the same authenticated direct-connect topology:
// clients send to the host, which relays a canonical speaker slot to the other
// peers. Frames are accepted only during an active race.
bool submit_local_voice(std::span<const std::uint8_t> encoded_frame);
std::vector<ReceivedVoiceFrame> take_received_voice_frames();

} // namespace rr64::netplay
