#pragma once

namespace rr64::highlights {
// Game-thread transition only. Releases the native sustained-race sound slots;
// leaves independent effects, music and voice alone. False means no safe native
// call context was supplied.
bool stop_race_loops(unsigned char *memory, void *context) noexcept;
}
