#include "rr64_offline_modifiers_bikes.hpp"

#include "rr64_offline_modifiers.hpp"
#include "rr64_custom_cop.hpp"
#include "rr64_engine_layout.hpp"
#include "rr64_local_players.hpp"
#include "rr64_local_race_options.hpp"
#include "rr64_thrash_options.hpp"
#include "librecomp/addresses.hpp"

#include <array>
#include <cstdint>

namespace {
using namespace rr64::engine;

// Original paid shop records, followed by Scooter and the two Insanity bikes.
// IDs absent from the native selectable lists are not invented as menu bikes.
constexpr std::array<unsigned, 23> kBikeIds{
    0, 1, 12, 13, 2, 3, 14, 15, 4, 5, 16, 17,
    6, 7, 18, 19, 8, 9, 20, 21, 23, 25, 26};
constexpr unsigned kCopEntry = 0x800A684C;
constexpr unsigned kSingleCursor = 0x800A66B0;
constexpr unsigned kMultiCursor = 0x8009EF3C;
unsigned char* shop_owner = nullptr;
unsigned shop_table = 0;

unsigned word(unsigned char* memory, unsigned address) {
    unsigned value = 0;
    read_u32(memory, address, value);
    return value;
}

constexpr unsigned native_entry(unsigned index) {
    return index < 20 ? 0x800A66F4 + index * 8 : 0x800A6834 + (index - 20) * 8;
}

bool menu_enabled(unsigned char* memory, unsigned campaign) {
    if (!memory || campaign > 1 ||
        !rr64::offline_modifiers::enabled(rr64::offline_modifiers::Flag::AllBikes))
        return false;
    if (campaign)
        return true; // Only called at 2CD60 / 2DC30 shop reads.
    if (!rr64_thrash_options_active() && !rr64::local_players::active.load())
        return false;
    const unsigned level = rr64_local_bike_menu_level(word(memory, 0x800A6690));
    // The stock cop-only selector must remain cop-only. Custom Cop has its own
    // explicit appended entry and retains the existing cop-rider restriction.
    return level < 15 && (level != 7 || rr64_custom_cop_enabled());
}

unsigned ensure_shop_table(unsigned char* memory) {
    if (shop_owner == memory && shop_table)
        return shop_table;
    // Keep the game's price/count tables immutable. A contiguous menu copy is
    // needed by 2CD60's affordability scan as well as its selected-bike reads.
    for (unsigned index = 0; index < kBikeIds.size(); ++index)
        if (word(memory, native_entry(index) + 4) != kBikeIds[index])
            return 0;
    auto* bytes = static_cast<unsigned char*>(recomp::alloc(memory, kBikeIds.size() * 8));
    if (!bytes)
        return 0;
    const auto offset = static_cast<std::uint64_t>(bytes - memory);
    if (offset < kRdramSize || offset > 0x20000000u - kBikeIds.size() * 8) {
        recomp::free(memory, bytes);
        return 0;
    }
    shop_owner = memory;
    shop_table = kRdramBegin + static_cast<unsigned>(offset);
    auto* rdram = memory; // MEM_W uses this name for the extended guest heap.
    for (unsigned index = 0; index < kBikeIds.size(); ++index) {
        MEM_W(index * 8, guest_address(shop_table)) = word(memory, native_entry(index));
        MEM_W(index * 8 + 4, guest_address(shop_table)) = kBikeIds[index];
    }
    return shop_table;
}
} // namespace

extern "C" int rr64_offline_bikes_active(unsigned char* memory, unsigned campaign) {
    return menu_enabled(memory, campaign);
}

extern "C" unsigned rr64_offline_bikes_count(unsigned char* memory, unsigned original,
                                               unsigned campaign) {
    if (!menu_enabled(memory, campaign) || (campaign && !ensure_shop_table(memory)))
        return original;
    return static_cast<unsigned>(kBikeIds.size()) +
           (!campaign && rr64_custom_cop_enabled() ? 1u : 0u);
}

extern "C" unsigned rr64_offline_bikes_entry(unsigned char* memory, unsigned player,
                                               unsigned original) {
    if (!menu_enabled(memory, 0) || player >= 4)
        return original;
    const bool single = rr64_thrash_options_active();
    if (single && player != 0)
        return original;
    const unsigned index = word(memory, single ? kSingleCursor : kMultiCursor + player * 4);
    if (index < kBikeIds.size())
        return native_entry(index);
    if (index == kBikeIds.size() && rr64_custom_cop_enabled())
        return kCopEntry;
    return original;
}

extern "C" unsigned rr64_offline_bikes_shop_table(unsigned char* memory, unsigned original,
                                                    unsigned index) {
    if (!menu_enabled(memory, 1) || index >= kBikeIds.size())
        return original;
    const unsigned table = ensure_shop_table(memory);
    return table ? table + index * 8 : original;
}

extern "C" void rr64_offline_bikes_reset() {
    // The session owner calls this before resetting/replacing the guest heap.
    // Freeing through an already replaced mapping would be unsafe.
    shop_owner = nullptr;
    shop_table = 0;
}
