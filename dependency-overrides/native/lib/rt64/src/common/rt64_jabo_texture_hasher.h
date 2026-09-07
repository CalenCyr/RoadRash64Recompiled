//
// RT64 - legacy Jabo texture-pack compatibility
//

#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "rt64_load_types.h"

namespace RT64 {
    struct JaboTextureHasher {
        // Returns the possible Jabo Direct3D texture identifiers for the
        // decoded TMEM image. Jabo selected one of several D3D8 surface
        // formats; trying the small, fixed set avoids depending on plug-in
        // state that the N64 renderer does not otherwise need to emulate.
        static std::vector<std::string> hashes(const uint8_t *tmem, size_t tmemSize, const LoadTile &loadTile, uint32_t width, uint32_t height, uint32_t tlut);
    };
};
