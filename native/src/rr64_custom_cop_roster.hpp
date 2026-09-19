#pragma once
#include "rr64_engine_layout.hpp"
#include "rr64_netplay.hpp"

namespace rr64::custom_cop {
// Authority hosts use canonical slots, including human slots above three.
// Clients map those identities into their local roster for movement prediction.
// Native local multiplayer retains its original contiguous four-player roster.
inline unsigned human_mask(unsigned char *m, const netplay::Status &status) {
    if (status.active && status.authoritative) {
        if (!status.connected || !status.authority_humans || (status.authority_humans >> 14))
            return 0;
        if (status.is_host)
            return status.authority_humans;
        if (status.local_slot >= 14 || !(status.authority_humans & (1u << status.local_slot)))
            return 0;
        unsigned mapped = 0;
        for (unsigned slot = 0; slot < 14; ++slot)
            if (status.authority_humans & (1u << slot))
                mapped |= 1u << online_flow::mapped_slot(slot, status.local_slot,
                                                         status.replicated_riders);
        return mapped;
    }
    unsigned count = 0;
    if (!engine::read_u32(m, 0x800a6578, count) || !count || count > 4)
        return 0;
    return (1u << count) - 1;
}
} // namespace rr64::custom_cop
