#include "rr64_online_ready.hpp"
#include "rr64_engine_layout.hpp"
#include "recomp.h"
#include <cstring>
#include <algorithm>
#include <cmath>

extern "C" void func_800796F8(unsigned char *, recomp_context *);
extern "C" void rr64_online_ready_draw(unsigned char *, void *);
extern "C" void rr64_online_disconnect_draw(unsigned char *rdram, void *context) {
    if (rr64::netplay::get_status().host_disconnected)
        rr64_online_ready_draw(rdram, context);
}

// Non-interactive original-font strip in the menu's top margin. Never changes
// guest selection, input, camera, or viewport state; hidden outside pre-race.
extern "C" void rr64_online_ready_draw(unsigned char *rdram, void *context) {
    const auto s = rr64::netplay::get_status();
    if (!context || (!s.host_disconnected && !rr64::online_ready::visible(s))) return;
    const auto original = *static_cast<recomp_context *>(context);
    auto draw_text = [&](const std::string &text, float x, float y, float scale, bool ready) {
        auto c = original;
        c.r29 -= 128;
        if (!rr64::engine::valid_guest_range(unsigned(c.r29), 128)) return;
        const auto length = (std::min)(text.size(), std::size_t(63));
        for (std::size_t i = 0; i < length; ++i) MEM_B(48 + i, c.r29) = text[i];
        MEM_B(48 + length, c.r29) = 0;
        MEM_B(32, c.r29) = ready ? -1 : -66; // RGBA bytes 255 / 190
        MEM_B(33, c.r29) = ready ? -1 : -66;
        MEM_B(34, c.r29) = ready ? 0 : -66;
        MEM_B(35, c.r29) = -1;
        c.r4 = c.r29 + 48;
        std::memcpy(&c.r5, &x, 4);
        std::memcpy(&c.r6, &y, 4);
        c.r7 = c.r29 + 32;
        MEM_W(16, c.r29) = 3;
        unsigned bits; std::memcpy(&bits, &scale, 4);
        MEM_W(20, c.r29) = bits;
        func_800796F8(rdram, &c);
    };
    if (s.host_disconnected) {
        const float scale=.9f + .08f*std::sin(s.host_disconnect_age_ms*.009f);
        draw_text(s.message=="Online sync stopped" ? s.message : "Host Disconnected",160.f-68.f*scale,105.f,scale,true);
        return;
    }
    unsigned count = 0, ready = 0;
    for (const auto &p : s.players) if (p.connected) {
        ++count; ready += rr64::online_ready::confirmed(s, p);
    }
    draw_text("READY " + std::to_string(ready) + "/" + std::to_string(count), 4, 0, .28f, true);
    // The supported 2-4 peer path uses one row. Larger experimental sessions
    // stay in the same 16-pixel margin using two rows and smaller cells.
    const unsigned columns = count > 4 ? 7 : 4;
    unsigned index = 0;
    for (unsigned slot = 0; slot < s.players.size(); ++slot) {
        const auto &p = s.players[slot];
        if (!p.connected) continue;
        draw_text(rr64::online_ready::label(s, p, slot),
            4.f + (index % columns) * (312.f / columns), 6.f + (index / columns) * 5.f,
            count > 4 ? .14f : .24f, rr64::online_ready::confirmed(s, p));
        ++index;
    }
}
