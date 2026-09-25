#pragma once

#include <cstdint>

namespace rr64::offline_modifiers {

// The caller supplies the specific option after the shared offline-race,
// prediction, demo and highlight gates. These predicates only authenticate a
// living human's current native rider/bike. They never write guest memory.
// Use only at the native damage regions documented in the implementation;
// neither function revives an already depleted resource or cancels a crash.
bool rider_damage_protected(unsigned char *memory, std::uint32_t rider,
                            bool enabled) noexcept;
bool bike_damage_protected(unsigned char *memory, std::uint32_t bike,
                           bool enabled) noexcept;

} // namespace rr64::offline_modifiers
