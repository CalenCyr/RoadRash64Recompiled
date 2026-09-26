#include "rr64_profile_names.hpp"
#include "rr64_engine_layout.hpp"
#include "rr64_local_players.hpp"

#include <algorithm>

namespace {
using namespace rr64;

void write_name(unsigned char* memory, unsigned destination, bool editing) {
    if (!memory) return;
    constexpr unsigned capacity = engine::globals::campaign_name_capacity;
    static_assert(capacity == engine::globals::multiplayer_display_name_stride);
    const auto name = local_players::snapshot().front();
    const unsigned length = std::min<unsigned>(name.size(), capacity - 1);
    for (unsigned i = 0; i < capacity; ++i) {
        // The original editor displays underscores for unused cells and
        // removes them on confirmation. Race names are ordinary C strings.
        const char value = i < length ? name[i] : editing && i + 1 < capacity ? '_' : '\0';
        engine::write_s8(memory, destination + i, value);
    }
    if (editing)
        engine::write_u32(memory, engine::globals::name_entry_character,
                          std::min(length, capacity - 2));
}
} // namespace

extern "C" void rr64_profile_name_new_campaign(unsigned char* memory) {
    // Runs once after 7280C clears a new name-entry field. Never called from
    // the drawing loop, save loading or campaign continuation, so manual
    // edits and previously saved campaign identities retain their ownership.
    write_name(memory, rr64::engine::globals::campaign_name, true);
}

extern "C" void rr64_profile_name_thrash(unsigned char* memory) {
    // 6C414 gives the human actor a fixed name. Replace it after the native
    // copy, before race initialization consumes the actor's identity. The
    // caller requires solo Thrash; local/online use their existing tables.
    constexpr unsigned first_actor_name = 0x800D857Cu;
    write_name(memory, first_actor_name, false);
}
