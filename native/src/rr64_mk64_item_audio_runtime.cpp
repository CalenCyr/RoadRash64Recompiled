#include "rr64_mk64_item_audio.hpp"
#include "rr64_mk64_items.hpp"
#include "rr64_engine_layout.hpp"
#include "rr64_experimental_course.hpp"
#include "rr64_netplay.hpp"
#include "rr64_online_flow.hpp"
#include "rr64_prediction_replay.hpp"
#include "rr64_highlights.hpp"
#include <algorithm>
#include <cmath>

namespace {
using namespace rr64;
unsigned word(unsigned char *m, unsigned a) {
    unsigned value = 0;
    engine::read_u32(m, a, value);
    return value;
}
unsigned half(unsigned char *m, unsigned a) {
    std::uint16_t value = 0;
    engine::read_u16(m, a, value);
    return value;
}
bool vector(unsigned char *m, unsigned address, mk64_items::Vec &v) {
    for (unsigned i = 0; i < 3; ++i)
        if (!engine::read_float(m, address + i * 4, v[i]) || !std::isfinite(v[i]) ||
            std::abs(v[i]) > 100000)
            return false;
    return true;
}
}
extern "C" void rr64_mk64_item_audio_step(unsigned char *m) {
    using namespace rr64;
    using namespace mk64_items;
    // Replay executes against a private RDRAM image and must not inspect or
    // reset the live mixer, issue events or mutate its observation baseline.
    if (prediction::active())
        return;
    static unsigned char *mapping = nullptr;
    if (mapping != m) {
        reset_audio();
        mapping = m;
    }
    const bool highlights = rr64_highlights_presenting() != 0;
    if (!m || !experimental_course::active() || highlights ||
        !engine::is_live_race_transition(word(m, engine::globals::main_mode),
                                         word(m, engine::globals::pending_mode))) {
        present_audio({}, {}, {}, false, highlights);
        return;
    }
    const auto rules = netplay::get_physics_rules();
    if (rules.active && (!rules.connected || rules.phase != netplay::Phase::Race)) {
        present_audio({}, {}, {}, false, false);
        return;
    }
    std::array<AudioRider, racer_capacity> riders{};
    for (unsigned canonical = 0; canonical < racer_capacity; ++canonical) {
        const unsigned slot = rules.active ? online_flow::mapped_slot(canonical, rules.local_slot,
                                                                      rules.replicated_riders)
                                           : canonical;
        const unsigned actor = 0x800D8570 + slot * 0x118;
        const unsigned bike = word(m, actor + 0xE0), body = word(m, actor + 0xE4);
        if (!half(m, actor + 0x24) || word(m, actor) != slot || (bike & 3) || (body & 3) ||
            !engine::valid_guest_range(bike, engine::bike::stride) ||
            !engine::valid_guest_range(body, engine::rider::stride) || word(m, bike + 4) != actor ||
            word(m, body + 4) != actor || word(m, bike + engine::bike::rider_pointer) != body ||
            word(m, body + engine::rider::bike_pointer) != bike)
            continue;
        auto &rider = riders[canonical];
        if (!vector(m,
                    half(m, body + engine::rider::bike_attached)
                        ? bike + engine::bike::body_position
                        : body + 0x8C,
                    rider.position))
            continue;
        unsigned identity = 2166136261u;
        for (unsigned value : {bike, body, word(m, actor + 0x18), word(m, actor + 0x1C)})
            identity = (identity ^ value) * 16777619u;
        rider.identity = identity ? identity : 1;
        const unsigned controller = word(m, actor + 8);
        rider.local = rules.active ? canonical == rules.local_slot : controller < 4;
    }
    std::array<AudioListener, 4> listeners{};
    unsigned listener_count = 0;
    const unsigned views =
        rules.active ? 1 : std::clamp(word(m, engine::local_race::humans), 1u, 4u);
    for (unsigned i = 0; i < views; ++i) {
        const unsigned view = rules.active ? (rules.replicated_riders ? 0 : rules.local_slot) : i;
        if (view >= 4)
            continue;
        const unsigned native = word(m, 0x800A657C + view * 4);
        const unsigned slot = rules.active ? online_flow::mapped_slot(native, rules.local_slot,
                                                                      rules.replicated_riders)
                                           : native;
        if (slot >= racer_capacity || !riders[slot].identity || !riders[slot].local)
            continue;
        AudioListener listener{riders[slot].position, {1, 0, 0}, slot};
        Vec eye{}, target{};
        if (vector(m, 0x800B7418 + view * 0x24, eye) &&
            vector(m, 0x800B7424 + view * 0x24, target)) {
            const float dx = target[0] - eye[0], dy = target[1] - eye[1],
                        length = std::hypot(dx, dy);
            if (length > .001f)
                listener.right = {dy / length, -dx / length, 0};
        }
        listeners[listener_count++] = listener;
    }
    const bool running = !half(m, engine::globals::gameplay_pause_state);
    present_audio(capture_state(), riders, std::span(listeners).first(listener_count), running,
                  false);
}
