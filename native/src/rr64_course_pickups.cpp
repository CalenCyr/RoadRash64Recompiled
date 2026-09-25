#include "rr64_course_pickups.hpp"
#include "rr64_engine_layout.hpp"
#include "rr64_experimental_course.hpp"
#include <bit>
#include <cmath>

extern "C" int rr64_course_loose_weapon(unsigned type) {
    // This is a deterministic course rule on every peer, including prediction.
    // It creates no reward, consumes no RNG and changes no actor inventory.
    return rr64::experimental_course::active() && type >= 1 && type <= 16;
}

extern "C" int rr64_course_loose_weapon_record(unsigned char *rdram, unsigned record) {
    if (!rr64::experimental_course::active() ||
        !rr64::engine::valid_guest_range(record, 16))
        return 0;
    unsigned bits = 0;
    if (!rr64::engine::read_u32(rdram, record + 12, bits))
        return 0;
    const float type = std::bit_cast<float>(bits);
    // Native placement records store the integral pickup identifier as float.
    // Do not classify malformed/fractional records by a lossy integer cast.
    return std::isfinite(type) && type >= 1 && type <= 16 && std::floor(type) == type;
}
