#pragma once

// Shared camera-independent collision support for online races and offline
// imported courses. The exported names retain their original online prefix.

#ifdef __cplusplus
#include <cstdint>
#include <span>
namespace rr64::online_terrain {
// Called before the guest heap is reset for a new game image.
void reset() noexcept;
// Bind only the scratch allocation and immutable ROM belonging to this
// historical image. No live heap/cache state or guest allocation is used.
bool bind_replay(unsigned char* memory, std::span<const std::uint8_t> rom);
}
extern "C" {
#endif
// Prepare before authority/prediction snapshots at the 6AFFC entry. Returns
// false if an active online race cannot obtain a bounded native scratch block.
// Offline imported races use the same support independently of graphics;
// preparation failure raises an explicit course error before actor updates.
int rr64_online_terrain_prepare(unsigned char* memory, void* context)
#ifdef __cplusplus
    noexcept(false)
#endif
;
// 146C8 entry: discard any cached geometry pointing into reusable scratch.
void rr64_online_terrain_query_begin(unsigned char* memory, void* context);
// 146C8 before14768: supply an immutable source cell when graphics has not
// loaded it. Changes only the descriptor/state argument registers.
void rr64_online_terrain_lookup(unsigned char* memory, void* context);
#ifdef __cplusplus
}
#endif
