#pragma once
#include <array>

namespace rr64::rider_skins {
// Opt-in evidence for the native selector. Fixed-size records contain addresses
// and draw commands, never texture pixels; the event thread writes the report.
struct PreviewTrace {
    unsigned reason{}, root{}, appearance{}, mode{}, handler{}, epoch{}, gfx{};
    unsigned pool{}, head{}, base{}, active_base{}, capacity{}, start{}, end{}, list{};
    unsigned rejected_opcode{}, replacements{}, image_count{}, commands{};
    std::array<unsigned, 4> choices{}, donors{}, headers{};
    // address/type/entity/slot/graph for up to eight original showroom actors.
    std::array<std::array<unsigned, 5>, 8> actors{};
    // Up to sixteen actual texture-image commands: opcode/address pairs.
    std::array<std::array<unsigned, 2>, 16> images{};
};
bool preview_trace_enabled() noexcept;
bool take_preview_trace(PreviewTrace &sample) noexcept;
unsigned preview_trace_dropped() noexcept;
}
