// Presentation-only translation consistency. No guest state or origin guesses.
#pragma once

#include <array>
#include <cmath>

namespace RT64::RR64CoordinateConsistency {
    struct TranslationState {
        std::array<float, 3> previous{};
        std::array<float, 3> current{};
        bool mapped = false;
        bool interpolate = false;
    };

    inline bool finite(const TranslationState &state) {
        for (unsigned i = 0; i < 3; i++) {
            if (!std::isfinite(state.previous[i]) || !std::isfinite(state.current[i])) {
                return false;
            }
        }
        return true;
    }

    // A coordinate-origin shift can appear in both the object's translation
    // and its view. Holding one changed translation while blending the other
    // loses that cancellation. Without an authoritative shared-origin record,
    // refuse the transition instead of guessing an offset or forcing movement.
    // Missing mappings are handled by the existing geometry/membership gate.
    inline bool coherentTranslations(bool enabled, const TranslationState &world,
        const TranslationState &view)
    {
        if (!enabled || !world.mapped || !view.mapped) {
            return true;
        }
        if (!finite(world) || !finite(view)) {
            return false;
        }
        return (world.interpolate == view.interpolate) ||
            (world.previous == world.current) || (view.previous == view.current);
    }
}
