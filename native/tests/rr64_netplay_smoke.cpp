#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <thread>

#include "rr64_netplay.hpp"

int main(int argc, char** argv) {
    if (argc < 3 || argc > 4) {
        std::fprintf(stderr, "usage: RR64NetplaySmoke host|join port [expected-players]\n");
        return 2;
    }

    const std::string mode = argv[1];
    const bool host = mode == "host";
    const bool join = mode == "join";
    const long parsed_port = std::strtol(argv[2], nullptr, 10);
    const long expected_players = argc == 4 ? std::strtol(argv[3], nullptr, 10) : 2;
    if ((!host && !join) || parsed_port < 1024 || parsed_port > 65535) {
        return 2;
    }
    if (expected_players < 2 || expected_players > rr64::netplay::kMaximumPlayers) {
        return 2;
    }

    rr64::netplay::Config config{};
    config.mode = host ? rr64::netplay::Mode::Host : rr64::netplay::Mode::Join;
    config.player_name = host ? "SmokeHost" : "SmokeClient";
    config.host_address = "127.0.0.1";
    config.port = static_cast<std::uint16_t>(parsed_port);
    rr64::netplay::configure(config);

    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(8);
    bool ready_sent = false;
    bool passed = false;
    bool remote_voice_received = false;
    auto last_voice_send = std::chrono::steady_clock::time_point{};
    while (std::chrono::steady_clock::now() < deadline) {
        rr64::netplay::update();
        const rr64::netplay::Status status = rr64::netplay::get_status();
        if (status.local_slot < rr64::netplay::kMaximumPlayers) {
            rr64::netplay::set_local_input(
                static_cast<std::uint16_t>(0x4000u | status.local_slot),
                static_cast<float>(status.local_slot) / 4.0f,
                -static_cast<float>(status.local_slot) / 4.0f);
        }
        if (status.connected && !ready_sent) {
            ready_sent = rr64::netplay::set_ready(true);
        }
        if (host && status.connected_players >= expected_players && rr64::netplay::all_connected_players_ready() &&
            status.phase == rr64::netplay::Phase::Lobby) {
            rr64::netplay::host_set_phase(rr64::netplay::Phase::GameSetup);
        }
        if (host && status.phase == rr64::netplay::Phase::GameSetup) {
            rr64::netplay::GameSetupState setup{};
            setup.valid = true;
            setup.transition_buttons = 0x8000u;
            for (std::size_t i = 0; i < setup.words.size(); ++i) {
                setup.words[i] = 0x64000000u + static_cast<std::uint32_t>(i);
            }
            rr64::netplay::host_commit_game_setup(setup);
        }
        if (host && status.phase == rr64::netplay::Phase::CharacterSelect) {
            rr64::netplay::host_set_phase(rr64::netplay::Phase::Race);
        }
        const rr64::netplay::Status updated = rr64::netplay::get_status();
        const auto now = std::chrono::steady_clock::now();
        if (updated.phase == rr64::netplay::Phase::Race &&
            updated.local_slot < rr64::netplay::kMaximumPlayers &&
            now - last_voice_send >= std::chrono::milliseconds(50)) {
            const std::uint8_t payload[] = {
                0x56u,
                updated.local_slot,
                static_cast<std::uint8_t>(0xA0u + updated.local_slot),
            };
            rr64::netplay::submit_local_voice(payload);
            last_voice_send = now;
        }
        for (const rr64::netplay::ReceivedVoiceFrame& voice :
             rr64::netplay::take_received_voice_frames()) {
            remote_voice_received = remote_voice_received ||
                (voice.speaker_slot != updated.local_slot && voice.payload_size == 3 &&
                 voice.payload[0] == 0x56u && voice.payload[1] == voice.speaker_slot);
        }
        bool inputs_synchronized = true;
        for (std::uint8_t slot = 0; slot < rr64::netplay::kMaximumPlayers; ++slot) {
            if (!updated.players[slot].connected) {
                continue;
            }
            std::uint16_t buttons = 0;
            float stick_x = 0.0f;
            float stick_y = 0.0f;
            inputs_synchronized = inputs_synchronized &&
                rr64::netplay::get_player_input(slot, buttons, stick_x, stick_y) &&
                buttons == static_cast<std::uint16_t>(0x4000u | slot);
        }
        passed = updated.connected && updated.connected_players >= expected_players &&
            updated.local_slot < rr64::netplay::kMaximumPlayers &&
            updated.phase == rr64::netplay::Phase::Race &&
            updated.game_setup.valid && updated.game_setup.revision > 0 &&
            updated.game_setup.transition_buttons == 0x8000u &&
            updated.game_setup.words.front() == 0x64000000u &&
            updated.game_setup.words.back() ==
                0x64000000u + static_cast<std::uint32_t>(updated.game_setup.words.size() - 1) &&
            inputs_synchronized && remote_voice_received;
        if (passed) {
            break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    if (passed) {
        const auto grace_deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(500);
        while (std::chrono::steady_clock::now() < grace_deadline) {
            rr64::netplay::update();
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    }
    rr64::netplay::shutdown();
    std::fprintf(stderr, "[RR64-NET-TEST] %s %s.\n", mode.c_str(), passed ? "passed" : "failed");
    return passed ? 0 : 1;
}
