#pragma once

// Shared camera-independent collision support for online and offline races
// on stock and imported courses. Exported names retain their online prefix.

#ifdef __cplusplus
#include <cstdint>
#include <memory>
#include <span>
#include <stdexcept>
namespace rr64::online_terrain {
// Private prediction translates this exact failure to its transaction error;
// live races report it rather than dereference unavailable collision geometry.
struct CollisionUnavailable : std::runtime_error { using std::runtime_error::runtime_error; };
// Called before the guest heap is reset for a new game image.
void reset() noexcept;
// Bind only the scratch allocation and immutable ROM belonging to this
// historical image. No live heap/cache state or guest allocation is used.
bool bind_replay(unsigned char* memory, std::span<const std::uint8_t> rom);
// Read a prepared live session's immutable cell for rendering. Never prepares
// terrain, writes scratch, or admits physics during results. Consume the view
// immediately inside the session; it must not survive a ROM/session reset.
// Optional identity owns bank metadata for a render cache key, not ROM bytes.
bool immutable_cell(unsigned char* memory, unsigned index,
                    std::span<const std::uint8_t>& bytes,
                    std::shared_ptr<const void>* identity = nullptr) noexcept;
}
extern "C" {
#endif
// Prepare before authority/prediction snapshots at the 6AFFC entry. Returns
// false if an active online race cannot obtain a bounded native scratch block.
// Offline races use the same support independently of graphics;
// preparation failure raises an explicit race error before actor updates.
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
// Native62594 static contacts use authored placement and terrain-wall lists
// independently of graphics residency. Hooks change local registers only;
// separate low-RAM scratch belongs to the same live/private snapshot bank.
void rr64_online_terrain_collision_begin(unsigned char* memory, void* context)
#ifdef __cplusplus
    noexcept(false)
#endif
;
void rr64_online_terrain_collision_objects(unsigned char* memory, void* context);
void rr64_online_terrain_collision_walls(unsigned char* memory, void* context);
#ifdef __cplusplus
}
#endif
