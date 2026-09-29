#include "rr64_campaign_completion.hpp"
#include "rr64_campaign_bonus_save.hpp"
#include "rr64_engine_layout.hpp"

#include <initializer_list>

namespace {
using namespace rr64::engine;
constexpr unsigned profile = 0x800D6A40u;
unsigned char *ending_return = nullptr;
unsigned word(unsigned char *m, unsigned address) {
    unsigned value = 0;
    read_u32(m, address, value);
    return value;
}
bool completed(unsigned char *m, unsigned record) {
    if (!m || (word(m, record + 0x3C) != 4 && word(m, record + 0x3C) != 5) ||
        word(m, record + 0x20) > 4)
        return false;
    // Match 73054: all eight Level 5 qualification nibbles must be nonzero.
    const unsigned results = word(m, record + 0x50);
    for (unsigned track = 0; track < 8; ++track)
        if (((results >> (track * 4)) & 15u) == 0)
            return false;
    return true;
}
void tier(unsigned char *m, unsigned level) {
    // Same normal/reversed-course entries as native 5F420.
    write_u16(m, 0x800A5334u + level * 2, 1);
    if (level + 8 < 15)
        write_u16(m, 0x800A5334u + (level + 8) * 2, 1);
}
void unlock(unsigned char *m, unsigned record) {
    for (unsigned level = 0; level < 5; ++level)
        tier(m, level);
    // Preserve the native ending reward (23340..23400). Profile +20 records
    // the original gang category, assigned by shop Join, not difficulty.
    const unsigned gang = word(m, record + 0x20);
    tier(m, gang == 1 || gang == 2 ? 6 : gang >= 3 ? 5 : 7);
    if (word(m, record + 0x3C) == 5)
        tier(m, 6);
}
}

extern "C" unsigned rr64_campaign_ending_exit(unsigned char *m, unsigned original) {
    ending_return = nullptr;
    if (original != 1 || !completed(m, profile) || word(m, profile + 0x3C) != 4)
        return original;
    // Called only when the player leaves the original ending/credits. Keep
    // the completed native profile intact so the normal Save Game can write it.
    ending_return = m;
    return 0x2F;
}

extern "C" int rr64_campaign_finish_menu(unsigned char *m) {
    const bool returning = m && ending_return == m;
    ending_return = nullptr;
    if (!returning || !completed(m, profile))
        return 0;
    // Native mode 2F / 73658 has already chosen a valid replay track. Keep its
    // normal menu initialization and highlight Save only on this credits return.
    write_u32(m, 0x8009EF68, 0); // Normal campaign menu; stock Save owns Pak setup.
    write_u32(m, 0x8009E128, 3); // Highlight Save Game.
    return 1;
}

extern "C" unsigned rr64_campaign_shop_promotion(unsigned char *m, unsigned native_ready) {
    if (!m)
        return native_ready;
    const unsigned level = word(m, profile + 0x3C);
    if (level < 4)
        return native_ready;
    // Join has already committed the purchased bike and gang. The optional
    // bonus uses its own bounded transition instead of the stock next-gang
    // promotion, whose tables contain only five campaign chapters.
    const unsigned bike = word(m, profile + 0x14);
    if (level == 4 && native_ready && completed(m, profile) && (bike == 25 || bike == 26)) {
        rr64_campaign_begin_bonus(m);
        write_u32(m, profile + 0x3C, 5);
        write_u32(m, 0x800A6680, 0);
        tier(m, 6);
    }
    return 0; // Stock mode 2F return; bonus never advances to another chapter.
}

extern "C" int rr64_campaign_bonus_active(unsigned char *m) {
    return m && word(m, profile + 0x3C) == 5;
}

extern "C" unsigned rr64_campaign_table_address(unsigned char *m, unsigned address) {
    const unsigned level = m ? word(m, profile + 0x3C) : ~0u;
    // The next-bike shop already maps Level 5's next tier to Insanity in its
    // update loop. Apply that same mapping during its native initialization.
    if ((level == 4 || level == 5) && address == 0x800A6854u + 5 * 4)
        return 0x800A6854u + 6 * 4;
    if (level != 5)
        return address;
    // Only call at the documented campaign table loads. Do not alter shared
    // ROM tables: the bonus borrows Level 5 economics and eight descriptors.
    for (unsigned base : {0x800A73E4u, 0x800A73F8u, 0x800A68A0u, 0x800A68B4u})
        if (address == base + 5 * 4)
            return base + 4 * 4;
    if (address == 0x800A66B8u + 5 * 4)
        return 0x800A66B8u + 6 * 4;
    return address;
}

extern "C" unsigned rr64_campaign_bonus_route(unsigned char *m, unsigned native_route) {
    if (!rr64_campaign_bonus_active(m))
        return native_route;
    const unsigned track = word(m, 0x800A6680);
    return track < 8 ? word(m, 0x800A739Cu + track * 4) : native_route;
}

extern "C" void rr64_campaign_bonus_menu(unsigned char *m) {
    if (rr64_campaign_bonus_active(m))
        write_u32(m, 0x800A6690, 6);
}

extern "C" void rr64_campaign_bonus_descriptor(unsigned char *m) {
    if (!rr64_campaign_bonus_active(m))
        return;
    constexpr unsigned descriptor = 0x800D8520;
    // The original menu has copied a complete 0x48-byte Level 5 descriptor.
    // Match the native Insanity selector's route, race type and rider pool;
    // retain the donor's campaign payouts, population and other race rules.
    write_u32(m, descriptor, 6);
    write_u32(m, descriptor + 4, 1);
    write_u16(m, descriptor + 8, rr64_campaign_bonus_route(m, 0));
    write_u32(m, descriptor + 0x28, 7);
}

extern "C" void rr64_campaign_bonus_label(unsigned char *m, unsigned buffer) {
    // Hook 2A134 receives the original title buffer at sp+B8. This 15-byte
    // label ends at sp+C6, below the later locals at sp+E8 and saved registers.
    constexpr char label[] = "Insanity Bonus";
    if (!rr64_campaign_bonus_active(m) || !valid_guest_range(buffer, sizeof(label)))
        return;
    for (unsigned i = 0; i < sizeof(label); ++i)
        write_s8(m, buffer + i, label[i]);
}

extern "C" void rr64_campaign_restore_unlocks(unsigned char *m, unsigned record) {
    // The hook runs only at 20AB4 after native save acceptance.
    // Status 2 alone is insufficient: it is set before Pak I/O can fail.
    if (!m || record < 0x800C07C8u || (record - 0x800C07C8u) % 0xF8 ||
        (record - 0x800C07C8u) / 0xF8 >= 6)
        return;
    // Also validate the newly copied record without modifying its checksum.
    // Native 207DC calculates its comparison before copying the read buffer.
    unsigned checksum = 0;
    for (unsigned offset = 0; offset < 0xF8; offset += 4)
        if (offset != 4) checksum += word(m, record + offset);
    if (word(m, record) == 4 && checksum == word(m, record + 4) && completed(m, record)) {
        unlock(m, record);
        if (rr64_campaign_bonus_record(m, record))
            tier(m, 6);
    }
}
