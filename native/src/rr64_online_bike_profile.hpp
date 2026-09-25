#pragma once
#include "rr64_engine_layout.hpp"
#include "rr64_netplay.hpp"
#include "rr64_online_flow.hpp"
#include "librecomp/addresses.hpp"

namespace rr64::online_flow {
inline unsigned char *bike_profile_owner = nullptr;
inline unsigned bike_profile_copies = 0;
inline void reset_bike_profiles() {
    // The ROM-session reset also resets the guest heap; do not free through
    // a mapping or heap that may already have been replaced.
    bike_profile_owner = nullptr;
    bike_profile_copies = 0;
}
// The >4-player representation has one native human and remote humans in AI
// slots. Native516B8 subsequently writes both identities from its chosen AI
// profile. Give those confirmed humans an owned profile before that resolver;
// never modify shared ROM profiles, role/control fields, or rider statistics.
inline unsigned bike_profile(unsigned char *rdram, unsigned actor, unsigned profile,
                             const netplay::Status &status) {
    using namespace engine;
    if (!rdram || !status.active || !status.connected || !status.replicated_riders ||
        !status.game_setup.valid || status.local_slot >= netplay::kMaximumPlayers ||
        actor < 0x800D8570u || !valid_guest_range(profile, 16))
        return profile;
    const unsigned offset = actor - 0x800D8570u;
    if (offset % 0x118u || offset / 0x118u >= netplay::kMaximumPlayers)
        return profile;
    const unsigned guest = offset / 0x118u;
    const unsigned slot = mapped_slot(guest, status.local_slot, true);
    const auto &player = status.players[slot];
    const auto &choice = player.selection;
    if (!player.connected || !choice.confirmed || !valid(choice) ||
        choice.round != status.game_setup.revision)
        return profile;
    unsigned donor = 0;
    for (unsigned i = 4; i < 160; ++i) {
        const unsigned entry = 0x800A3460u + i * 16u;
        const unsigned tier = MEM_BU(8, guest_address(entry));
        if (tier == 12)
            break;
        if (MEM_BU(10, guest_address(entry)) == choice.bike) {
            donor = entry;
            break;
        }
    }
    if (!donor)
        return profile;
    if (bike_profile_owner != rdram)
        reset_bike_profiles();
    if (!bike_profile_copies) {
        auto *host = static_cast<unsigned char *>(recomp::alloc(rdram, netplay::kMaximumPlayers * 16u));
        if (!host)
            return profile;
        bike_profile_owner = rdram;
        bike_profile_copies = 0x80000000u + static_cast<unsigned>(host - rdram);
    }
    const unsigned copy = bike_profile_copies + guest * 16u;
    for (unsigned i = 0; i < 16; ++i)
        MEM_B(i, guest_address(copy)) = MEM_B(i, guest_address(profile));
    MEM_B(10, guest_address(copy)) = choice.bike;
    MEM_B(11, guest_address(copy)) = choice.rider;
    MEM_B(13, guest_address(copy)) = MEM_B(13, guest_address(donor));
    return copy;
}
} // namespace rr64::online_flow
