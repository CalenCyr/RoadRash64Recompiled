#pragma once

#include <filesystem>

namespace rr64::character_preferences {
inline constexpr unsigned unknown = 0xFFFFFFFFu;

// Initialize before game-thread use. Reloads all four local preferences from
// directory/character-preferences.cfg; missing or invalid files mean unknown.
void initialize(const std::filesystem::path& directory);

// UI/shutdown thread only. Failed writes remain dirty for a later retry.
void flush();

// Game/UI safe and free of file I/O. Availability in the current game mode is
// checked by the caller; only local slots 0..3 and native rider IDs 0..44 persist.
void remember(unsigned slot, unsigned rider);
unsigned last(unsigned slot);
} // namespace rr64::character_preferences
