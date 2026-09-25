#include "rr64_highlight_audio.hpp"
#include "rr64_engine_layout.hpp"
#include "rr64_prediction_replay.hpp"

extern "C" void func_80080768(unsigned char *, recomp_context *);
extern "C" void func_800806B4(unsigned char *, recomp_context *);

namespace rr64::highlights {
bool stop_race_loops(unsigned char *memory, void *context) noexcept {
    using namespace engine;
    if (!memory || !context || prediction::active())
        return false;
    auto call = *static_cast<recomp_context *>(context);
    const auto stack = static_cast<std::uint32_t>(call.r29);
    if ((stack & 7u) || !valid_guest_range(stack - 8u, 8u))
        return false;
    call.f_odd = &call.f0.u32h;
    for (unsigned racer = 0; racer < kMaximumRacers; ++racer) {
        for (unsigned slot = 0; slot < racer_audio::loop_slot_count; ++slot) {
            const auto entry = racer_audio::cache + racer * racer_audio::row_stride +
                               slot * racer_audio::entry_stride;
            std::uint32_t handle = racer_audio::unused_handle;
            if (!read_u32(memory, entry, handle) || handle == racer_audio::unused_handle)
                continue;
            // Match the five-slot domain of native stale-loop expiry. Replay
            // freezes that update, but its existing audio voices keep running.
            // The native audio worker consumes these handle-specific requests.
            if (handle != 0) {
                call.r4 = guest_address(handle);
                func_80080768(memory, &call);
                if (static_cast<std::int32_t>(call.r2) > 0) {
                    call.r4 = guest_address(handle);
                    call.r5 = 0;
                    func_800806B4(memory, &call);
                }
            }
            // Native producers allocate fresh handles after this sentinel.
            // Keep cache metadata and the later impact/voice slots untouched.
            write_u32(memory, entry, racer_audio::unused_handle);
        }
    }
    return true;
}
}
