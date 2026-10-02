#pragma once

#include <filesystem>
#include <string>
#include <string_view>

namespace rr64::rider_skin_preferences {
// Separate from native rider IDs and campaign saves. Initialize before use;
// absent or malformed files select native appearances for all four players.
void initialize(const std::filesystem::path& directory);

// UI/shutdown only. Writes a complete replacement; failures remain pending.
void flush();

// No file I/O. Empty means native; otherwise use a stable [a-z0-9-] ID of at
// most 32 characters. The caller checks whether the mod/entry is available.
void remember(unsigned slot, std::string_view stable_id);
std::string last(unsigned slot);
} // namespace rr64::rider_skin_preferences
