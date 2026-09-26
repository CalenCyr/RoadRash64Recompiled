#include "rr64_campaign_completion.hpp"
#include "rr64_engine_layout.hpp"

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
    if (!m || word(m, record + 0x3C) != 4 || word(m, record + 0x20) > 4)
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
    // Preserve the native ending reward (23340..23400), including difficulty.
    const unsigned difficulty = word(m, record + 0x20);
    tier(m, difficulty == 1 || difficulty == 2 ? 6 : difficulty >= 3 ? 5 : 7);
}
}

extern "C" unsigned rr64_campaign_ending_exit(unsigned char *m, unsigned original) {
    ending_return = nullptr;
    if (original != 1 || !completed(m, profile))
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
    if (word(m, record) == 4 && checksum == word(m, record + 4) && completed(m, record))
        unlock(m, record);
}
