#pragma once
#include "rr64_highlight_recording.hpp"
#include <cstdint>

namespace rr64::highlights {
// Call after the actual local renderer tier is selected (11CC0:11D8C).
// Captures validated currently held child poses, not proof that animation
// advanced this tick. The caller owns frame enrollment/freshness policy.
bool capture_node(unsigned char *memory, std::uint32_t node, unsigned actual_tier, Pose &out) noexcept;
// End-of-update capture uses the entity's owned scene node and current native
// tier. Independent of camera culling and human/AI control ownership.
bool capture_entity(unsigned char *memory, std::uint32_t entity, unsigned type, Pose &out) noexcept;
// Only after the private renderer's complete pair passes SnapshotStore::publish.
// Copies tier-zero children from the private mapping; verifies the live graph,
// ownership, pose pointers and source bank still match. Writes neither mapping.
bool capture_prepared_node(unsigned char *live, unsigned char *prepared,
                           std::uint32_t node, Pose &out) noexcept;

// One scope per game/render thread. Requires the exact recorded tier and
// topology; does not force a tier, change allocation/resource pointers, or
// update simulation state. Rebuilds only the root pose using current camera.
// Returns false without writes when validation fails or a scope is active.
bool begin_actor(unsigned char *memory, std::uint32_t node, unsigned actual_tier, const Pose &recorded,
                 const Vec3 &world_anchor, const Quaternion &world_rotation,
                 const Vec3 &camera_world) noexcept;
// Existing native root hooks provide the same source1 -> source2 unit/depth
// convention as live detailed rendering, scoped to this recorded actor only.
unsigned root_source(unsigned char *, unsigned node, unsigned record, unsigned original) noexcept;
bool scale_root_matrix(unsigned char *, unsigned node, unsigned record, unsigned matrix) noexcept;
// Restore BEFORE rr64_lod_end_actor/world_end_actor at11F00/120C8/120D4.
// The caller must end before the guest mapping can be released/reset.
void end_actor() noexcept;
bool actor_bound() noexcept;
// Shared with the held-weapon replay root, using identical validated banks.
bool detailed_projection_ready(unsigned char *) noexcept;
} // namespace rr64::highlights
