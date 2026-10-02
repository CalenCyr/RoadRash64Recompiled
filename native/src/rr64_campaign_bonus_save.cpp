#include "rr64_campaign_bonus_save.hpp"
#include "rr64_engine_layout.hpp"

#include <array>
#include <cstdint>

namespace {
using namespace rr64::engine;
constexpr unsigned profile = 0x800D6A40u;
constexpr unsigned slots = 0x800C07C8u;
constexpr unsigned staging = 0x800C0470u;
constexpr unsigned profile_size = 0xF8u;
constexpr unsigned file_size = 0x100u;
constexpr unsigned slot_count = 6u;

struct SavedBonus {
    std::array<unsigned, profile_size / 4> record{};
    unsigned results = 0;
    bool valid = false;
};
struct BonusState {
    unsigned char *memory = nullptr;
    unsigned results = 0;
    std::array<SavedBonus, slot_count> saved{};
};
BonusState state;

unsigned word(unsigned char *m, unsigned address) {
    unsigned result = 0;
    read_u32(m, address, result);
    return result;
}

void bind(unsigned char *m) {
    if (state.memory != m) {
        state = {};
        state.memory = m;
    }
}

unsigned checksum(unsigned char *m, unsigned record) {
    unsigned result = 0;
    for (unsigned offset = 0; offset < profile_size; offset += 4)
        if (offset != 4)
            result += word(m, record + offset);
    return result;
}

bool completed_base(unsigned char *m, unsigned record) {
    if (word(m, record) != 4 || word(m, record + 0x3C) != 4 || word(m, record + 0x20) > 4)
        return false;
    const unsigned results = word(m, record + 0x50);
    for (unsigned track = 0; track < 8; ++track)
        if (((results >> (track * 4)) & 15u) == 0)
            return false;
    return checksum(m, record) == word(m, record + 4);
}

// Versioned, salted CRC-32 over the complete serialized native record plus
// the new qualification word. Zero is reserved for native/absent padding.
// This binds each extension to its exact profile, including native checksum.
unsigned signature(unsigned char *m, unsigned record, unsigned results) {
    unsigned crc = 0xFFFFFFFFu;
    const auto byte = [&crc](unsigned value) {
        crc ^= value & 255u;
        for (unsigned bit = 0; bit < 8; ++bit)
            crc = (crc >> 1) ^ (0xEDB88320u & (0u - (crc & 1u)));
    };
    for (unsigned value : {0x52u, 0x52u, 0x36u, 0x34u, 0x42u, 0x01u})
        byte(value);
    for (unsigned offset = 0; offset < profile_size; ++offset) {
        std::uint8_t value = 0;
        read_u8(m, record + offset, value);
        byte(value);
    }
    for (unsigned shift : {24u, 16u, 8u, 0u})
        byte(results >> shift);
    crc = ~crc;
    return crc == 0 ? 1u : crc;
}

bool active_qualification(unsigned char *m, unsigned address) {
    return m && address == profile + 0x54 && word(m, profile + 0x3C) == 5;
}
} // namespace

extern "C" void rr64_campaign_begin_bonus(unsigned char *m) {
    bind(m);
    state.results = 0;
}

extern "C" void rr64_campaign_bonus_new_profile(unsigned char *m, unsigned record) {
    if (record == profile) {
        bind(m);
        // Starting a new campaign resets only its live results. Native 5F480
        // does not invalidate the Pak cache; its matching extensions must live
        // until a real rescan, including when the player next selects Load Game.
        state.results = 0;
    }
}

extern "C" unsigned rr64_campaign_qualification_word(unsigned char *m, unsigned address,
                                                     unsigned native_word) {
    bind(m);
    return active_qualification(m, address) ? state.results : native_word;
}

extern "C" unsigned rr64_campaign_qualification_store(unsigned char *m, unsigned address,
                                                      unsigned new_word) {
    bind(m);
    if (!active_qualification(m, address))
        return new_word;
    state.results = new_word;
    // The original instruction still writes its original reputation word.
    // Its value register is dead after each audited store (72F98/72FB8).
    return word(m, address);
}

extern "C" void rr64_campaign_bonus_scan(unsigned char *m) {
    bind(m);
    // This follows the native scan cache guards. Clearing on a cached lookup
    // loses the bonus marker because no Pak read follows to recapture its tail.
    state.saved = {};
}

extern "C" void rr64_campaign_bonus_capture(unsigned char *m, unsigned record) {
    bind(m);
    if (!m || record < slots || (record - slots) % profile_size != 0 ||
        (record - slots) / profile_size >= slot_count)
        return;
    auto &saved = state.saved[(record - slots) / profile_size];
    saved = {};
    // Called immediately after the native 248-byte staging-to-slot copy.
    // Do not trust its status flag: native sets it before the Pak read.
    if (!completed_base(m, record) || !completed_base(m, staging))
        return;
    for (unsigned i = 0; i < saved.record.size(); ++i) {
        saved.record[i] = word(m, record + i * 4);
        if (saved.record[i] != word(m, staging + i * 4))
            return;
    }
    saved.results = word(m, staging + profile_size);
    const unsigned tag = word(m, staging + profile_size + 4);
    saved.valid = tag != 0 && tag == signature(m, staging, saved.results);
}

extern "C" void rr64_campaign_bonus_load(unsigned char *m, unsigned slot, unsigned destination) {
    bind(m);
    if (!m || destination != profile)
        return;
    // Only the successful native 248-byte copy calls this helper. A failed
    // load leaves the current unsaved campaign and its bonus word untouched.
    state.results = 0;
    if (slot >= slot_count || !state.saved[slot].valid || !completed_base(m, destination))
        return;
    const auto &saved = state.saved[slot];
    for (unsigned i = 0; i < saved.record.size(); ++i)
        if (saved.record[i] != word(m, destination + i * 4))
            return;
    state.results = saved.results;
    write_u32(m, destination + 0x3C, 5);
    write_u32(m, destination + 4, checksum(m, destination));
}

extern "C" int rr64_campaign_bonus_record(unsigned char *m, unsigned record) {
    bind(m);
    if (!m || record < slots || (record - slots) % profile_size != 0 ||
        (record - slots) / profile_size >= slot_count)
        return 0;
    const auto &saved = state.saved[(record - slots) / profile_size];
    if (!saved.valid)
        return 0;
    for (unsigned i = 0; i < saved.record.size(); ++i)
        if (saved.record[i] != word(m, record + i * 4))
            return 0;
    return 1;
}

extern "C" void rr64_campaign_bonus_save(unsigned char *m, unsigned source, unsigned buffer) {
    bind(m);
    if (!m || source != profile || buffer != staging || !valid_guest_range(buffer, file_size) ||
        word(m, source + 0x3C) != 5)
        return;
    // Native 20E10 cleared all 256 bytes and 20E3C copied exactly 248. Keep
    // the live bonus index5 and all inventory/statistics untouched. Older
    // builds read serialized index4 as a completed original Level5 campaign.
    write_u32(m, buffer + 0x3C, 4);
    write_u32(m, buffer + 4, checksum(m, buffer));
    if (!completed_base(m, buffer))
        return;
    write_u32(m, buffer + profile_size, state.results);
    write_u32(m, buffer + profile_size + 4, signature(m, buffer, state.results));
}
