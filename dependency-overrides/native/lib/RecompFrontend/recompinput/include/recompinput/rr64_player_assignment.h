#pragma once
#include <algorithm>
#include <cstddef>

namespace recompinput {
// Preserve player identity on hotplug: surviving devices never shift slots.
// The stock game has four controller ports, irrespective of frontend capacity.
template<class Slots, class Controllers>
void reconcile_local_players(Slots& slots, const Controllers& connected, bool keyboard) {
    const auto end = slots.begin() + std::min<std::size_t>(4, slots.size());
    for (auto it = slots.begin(); it != slots.end(); ++it) {
        if (it >= end || (it->controller && std::find(connected.begin(), connected.end(), it->controller) == connected.end()) ||
            (it->keyboard_enabled && !keyboard)) *it = typename Slots::value_type{};
    }
    for (auto controller : connected) {
        if (!controller || std::any_of(slots.begin(), end, [controller](const auto& p) { return p.controller == controller; })) continue;
        auto free = std::find_if(slots.begin(), end, [](const auto& p) { return !p.controller && !p.keyboard_enabled; });
        if (free == end) break;
        free->controller = controller;
    }
    if (keyboard && std::none_of(slots.begin(), end, [](const auto& p) { return p.keyboard_enabled; })) {
        auto free = std::find_if(slots.begin(), end, [](const auto& p) { return !p.controller && !p.keyboard_enabled; });
        if (free != end) free->keyboard_enabled = true;
    }
}
}
