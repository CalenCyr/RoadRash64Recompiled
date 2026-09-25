#include "rr64_offline_modifiers_health.hpp"
#include "rr64_engine_layout.hpp"

#include <cmath>

namespace rr64::offline_modifiers {
namespace {
using namespace engine;
constexpr std::uint32_t actors = 0x800D8570u;
constexpr std::uint32_t actor_stride = 0x118u;
// Native 61224/616BC subtract from +310; 36C88 tests depletion and
// 37048..37080 regenerates toward +30C. These are not the nearby stun timers.
constexpr std::uint32_t rider_current = 0x310u;
constexpr std::uint32_t rider_capacity = 0x30Cu;

std::uint32_t word(unsigned char *memory, std::uint32_t address) noexcept {
    std::uint32_t value = 0;
    read_u32(memory, address, value);
    return value;
}
std::uint16_t half(unsigned char *memory, std::uint32_t address) noexcept {
    std::uint16_t value = 0;
    read_u16(memory, address, value);
    return value;
}
bool entity_range(std::uint32_t address, std::uint32_t bytes) noexcept {
    return !(address & 3u) && valid_guest_range(address, bytes);
}
bool owned_human(unsigned char *memory, std::uint32_t entity, bool is_rider) noexcept {
    if (!memory || !entity_range(entity, is_rider ? rider::stride : bike::stride))
        return false;
    const auto actor = word(memory, entity + 4);
    if (actor < actors || (actor - actors) % actor_stride ||
        (actor - actors) / actor_stride >= kMaximumRacers ||
        !half(memory, actor + 0x24) || half(memory, actor + 0x26) != 0 ||
        word(memory, actor + 8) >= 4)
        return false;
    const auto bike_address = word(memory, actor + 0xE0);
    const auto rider_address = word(memory, actor + 0xE4);
    return entity == (is_rider ? rider_address : bike_address) &&
           entity_range(bike_address, bike::stride) &&
           entity_range(rider_address, rider::stride) &&
           word(memory, bike_address + 4) == actor &&
           word(memory, rider_address + 4) == actor &&
           word(memory, bike_address + bike::rider_pointer) == rider_address &&
           word(memory, rider_address + rider::bike_pointer) == bike_address;
}
bool positive_resource(unsigned char *memory, std::uint32_t entity,
                       std::uint32_t current_offset, std::uint32_t capacity_offset) noexcept {
    float current = 0, capacity = 0;
    return read_float(memory, entity + current_offset, current) &&
           read_float(memory, entity + capacity_offset, capacity) &&
           std::isfinite(current) && std::isfinite(capacity) && current > 0 && capacity > 0;
}
} // namespace

bool rider_damage_protected(unsigned char *memory, std::uint32_t rider_address,
                            bool enabled) noexcept {
    // 61224: before 613D8 -> 61408; 616BC: before 61708 -> 61744.
    // Skip only the subtraction/clamp block. Stun effects before these blocks,
    // subsequent native responses and independent physical ejection stay live.
    // The crash-state zero at 3775C remains native, followed by ordinary
    // recovery initialization; this is damage protection, not crash immunity.
    return enabled && owned_human(memory, rider_address, true) &&
           positive_resource(memory, rider_address, rider_current, rider_capacity);
}

bool bike_damage_protected(unsigned char *memory, std::uint32_t bike_address,
                           bool enabled) noexcept {
    // 37554: before 37598 -> 3759C, suppressing only the durability store.
    // Do not return from 37554: its body detach, momentum and recovery writes
    // are required even when the native 19-point crash charge is disabled.
    return enabled && owned_human(memory, bike_address, false) &&
           positive_resource(memory, bike_address, bike::durability_current,
                             bike::durability_capacity);
}

} // namespace rr64::offline_modifiers
