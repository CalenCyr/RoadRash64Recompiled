#pragma once

#include <array>
#include <cstdint>

#include "recomp.h"

namespace rr64::course_guardrail {
using Vec3 = std::array<float, 3>;

struct Contact {
    Vec3 normal{}; // Outward rail-side normal in native X/Y/Z-up coordinates.
    float top = 0; // Authored rail top at the contact, before native sphere allowance.
    unsigned sphere_index = 0;
    // Only verified finite source panels may use this path. The native top and
    // alignment gates still reject tall contacts; dynamic hazards are excluded.
    bool tagged_low_rail = false;
};

enum class Eligibility { Blocked, FirstVault, ContinuingVault };

struct Result {
    bool consumed = false; // Do not apply the ordinary crash impulse as well.
    bool launched = false; // This contact authored the native one-shot impulse.
    Vec3 velocity_delta{};
};

// Read-only equivalent of the static native rail gates in 8004CBFC/80048A1C.
// Prediction can inspect this decision; only apply() may author a new vault.
Eligibility eligibility(unsigned char *memory, std::uint32_t actor_index,
                        const Contact &contact) noexcept;

// Immediately after native34594, like course_impact::apply. Uses native vector
// helpers and34594 on preserved scratch, committing velocity and the original
// bike+818 vault latch without integrating the live position a second time.
// Clients and isolated replay cannot author an impulse or latch.
Result apply(unsigned char *memory, const recomp_context &context,
             std::uint32_t actor_index, const Contact &contact);
}
