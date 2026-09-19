#pragma once
#include "rr64_netplay.hpp"
#include <string>

namespace rr64::online_ready {
inline bool visible(const netplay::Status &s) {
    return s.active && s.connected && s.game_setup.valid &&
        (s.phase == netplay::Phase::CharacterSelect || s.phase == netplay::Phase::TrackSelect);
}
inline bool confirmed(const netplay::Status &s, const netplay::PlayerInfo &p) {
    // Lobby readiness and previous-round selections are not race confirmation.
    return p.connected && p.selection.round == s.game_setup.revision && p.selection.confirmed;
}
inline std::string label(const netplay::Status &s, const netplay::PlayerInfo &p, unsigned slot) {
    std::string name;
    // The original font is ASCII. Keep network names inside their fixed cell.
    for (unsigned char c : p.name) {
        if (name.size() == 10) break;
        name += c >= 32 && c <= 126 ? char(c) : '?';
    }
    if (name.empty()) name = "P" + std::to_string(slot + 1);
    const bool ready = confirmed(s, p);
    name += ready ? " READY" : " WAIT";
    if (ready && (s.game_setup.race_options & 512u) && p.selection.bike == 31 &&
        p.selection.rider >= 40 && p.selection.rider <= 44) name += " COP";
    return name;
}
}
