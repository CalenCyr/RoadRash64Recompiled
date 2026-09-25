#pragma once

namespace rr64::offline_modifiers {
// Game-thread, one-shot grant. The caller owns the offline race/demo lifecycle
// and the per-race/per-enable epoch. This never implements infinite ammunition.
// Returns true when a regular local human has a valid, fully stocked inventory.
bool grant_max_weapons(unsigned char *memory, unsigned actor, bool enabled);
}
