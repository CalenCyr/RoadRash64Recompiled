#pragma once
#include "rr64_mk64_item_replay.hpp"

namespace rr64::prediction {
// Item physics is a historical input, separate from live inventory, object
// simulation, cues, and use edges. Builds without imported courses stay inert.
inline bool capture_item_state(mk64_items::ReplayState &out) noexcept {
#ifdef RR64_EXPERIMENTAL_COURSE
    return mk64_items::capture_replay(out);
#else
    out={};return true;
#endif
}
inline bool bind_item_state(unsigned char *memory,const mk64_items::ReplayState &state) noexcept {
#ifdef RR64_EXPERIMENTAL_COURSE
    return mk64_items::bind_replay(memory,state);
#else
    (void)memory;return state==mk64_items::ReplayState{};
#endif
}
inline bool complete_item_state(unsigned duration_us,mk64_items::ReplayState &out) noexcept {
#ifdef RR64_EXPERIMENTAL_COURSE
    mk64_items::finish_replay(duration_us);
    return mk64_items::replay_state(out);
#else
    (void)duration_us;out={};return true;
#endif
}
inline bool correct_item_state(unsigned char *memory,mk64_items::ReplayState &state,
                               const mk64_items::Snapshot &authority) noexcept {
#ifdef RR64_EXPERIMENTAL_COURSE
    return mk64_items::correct_replay(memory,state,authority);
#else
    (void)memory;return authority==mk64_items::Snapshot{} && state==mk64_items::ReplayState{};
#endif
}
}
