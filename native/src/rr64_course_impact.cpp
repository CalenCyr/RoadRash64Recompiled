#include "rr64_course_impact.hpp"
#include "rr64_engine_layout.hpp"
#include "rr64_experimental_course.hpp"
#include "rr64_netplay.hpp"
#include "rr64_prediction_replay.hpp"

#include <algorithm>
#include <bit>
#include <cmath>
#include <cstring>

extern "C" void func_80048FA4(unsigned char *, recomp_context *);
extern "C" void func_80034594(unsigned char *, recomp_context *);
extern "C" void func_80056DA8(unsigned char *, recomp_context *);
extern "C" void func_800565BC(unsigned char *, recomp_context *);

namespace rr64::course_impact {
namespace {
using namespace engine;
unsigned word(unsigned char *m, unsigned a) {
    unsigned value = 0;
    read_u32(m, a, value);
    return value;
}
unsigned half(unsigned char *m, unsigned a) {
    std::uint16_t value = 0;
    read_u16(m, a, value);
    return value;
}
float scalar(unsigned char *m, unsigned a) {
    float value = 0;
    read_float(m, a, value);
    return value;
}
bool finite(const Vec3 &v) {
    return std::all_of(v.begin(), v.end(), [](float f) { return std::isfinite(f); });
}
bool finite_range(unsigned char *m, unsigned address, unsigned count) {
    for (unsigned i = 0; i < count; ++i)
        if (!std::isfinite(scalar(m, address + 4 * i)))
            return false;
    return true;
}
// Native helpers use ordinary guest stack frames. Preserve both their stack
// and the private body/contact scratch so live caller and replay memory never
// retain temporary objects. The deepest helper chain is below 0x200 bytes.
struct Scratch {
    unsigned char *memory;
    unsigned address;
    std::array<unsigned char, 0x900> saved;
    Scratch(unsigned char *m, unsigned a) : memory(m), address(a) {
        std::memcpy(saved.data(), memory + (address - kRdramBegin), saved.size());
    }
    ~Scratch() {
        std::memcpy(memory + (address - kRdramBegin), saved.data(), saved.size());
    }
};
}

Result apply(unsigned char *m, const recomp_context &context, std::uint32_t actor_index,
             Body kind, const Contact &contact) {
    Result result;
    if (!m || !experimental_course::active() || prediction::active() ||
        actor_index >= kMaximumRacers || half(m, globals::gameplay_pause_state))
        return result;
    const auto status = netplay::get_physics_rules();
    if (status.active && (!status.connected || !status.authoritative || !status.is_host ||
                          status.phase != netplay::Phase::Race))
        return result;
    if (!finite(contact.point) || !finite(contact.normal) || !finite(contact.surface_velocity) ||
        !std::isfinite(contact.penetration) || contact.penetration < 0 ||
        !std::isfinite(contact.response) || contact.response < 1 || contact.response > 2)
        return result;
    float norm2 = 0;
    for (float n : contact.normal)
        norm2 += n * n;
    if (!std::isfinite(norm2) || norm2 < 1e-12f)
        return result;
    Vec3 normal = contact.normal;
    for (float &n : normal)
        n /= std::sqrt(norm2);

    const unsigned actor = 0x800D8570u + actor_index * 0x118u;
    const unsigned bike = word(m, actor + 0xE0), rider = word(m, actor + 0xE4);
    if (!half(m, actor + 0x24) || !valid_guest_range(bike, bike::stride) ||
        !valid_guest_range(rider, rider::stride) || word(m, bike + 4) != actor ||
        word(m, rider + 4) != actor || word(m, bike + bike::rider_pointer) != rider ||
        word(m, rider + rider::bike_pointer) != bike ||
        (kind == Body::Rider && half(m, bike + bike::rider_attached)))
        return result;
    const unsigned body = kind == Body::Bike ? bike + 0x108u : rider + 0x28u;
    const unsigned stack = static_cast<unsigned>(context.r29);
    if (!valid_guest_range(stack - 0x900u, 0x900u) ||
        !finite_range(m, body, 9) || !finite_range(m, body + 0x64, 18) ||
        !finite_range(m, body + 0xA8, 3) ||
        !finite_range(m, body + 0xC0, 9) || !finite_range(m, body + 0x118, 9) ||
        !finite_range(m, body + 0x170, 3) || scalar(m, body) <= 0 ||
        scalar(m, body + 0xC) <= 0 || scalar(m, body + 0x10) <= 0 ||
        scalar(m, body + 0x14) <= 0 || !std::isfinite(scalar(m, 0x8009CBA8)) ||
        scalar(m, 0x8009CBA8) <= 0 || !std::isfinite(scalar(m, 0x8009CBB4)) ||
        scalar(m, 0x8009CBB4) <= 0)
        return result;
    Vec3 velocity{};
    float approach = 0;
    for (unsigned i = 0; i < 3; ++i) {
        velocity[i] = scalar(m, body + 0x70 + i * 4);
        approach += (velocity[i] - contact.surface_velocity[i]) * normal[i];
    }
    Vec3 lever{};
    for (unsigned i = 0; i < 3; ++i)
        lever[i] = contact.point[i] - scalar(m, body + 0x64 + i * 4);
    const Vec3 lever_cross_normal{lever[1] * normal[2] - lever[2] * normal[1],
                                 lever[2] * normal[0] - lever[0] * normal[2],
                                 lever[0] * normal[1] - lever[1] * normal[0]};
    for (unsigned i = 0; i < 3; ++i)
        approach += scalar(m, body + 0xA8 + 4 * i) * lever_cross_normal[i];
    // Do not let the native minimum impulse continually kick a stationary or
    // separating body. Positional overlap resolution remains the caller's job.
    if (!std::isfinite(approach) || approach >= 0)
        return result;

    Scratch scratch(m, stack - 0x900u);
    const unsigned temporary = stack - 0x300u, record = stack - 0x70u;
    std::memcpy(m + (temporary - kRdramBegin), m + (body - kRdramBegin), 0x200);
    for (unsigned i = 0; i < 3; ++i) {
        write_float(m, temporary + 0x98 + 4 * i, velocity[i] - contact.surface_velocity[i]);
        write_float(m, temporary + 0x8C + 4 * i, scalar(m, body + 0x64 + 4 * i));
        write_float(m, temporary + 0xF4 + 4 * i, 0);
        write_float(m, temporary + 0x100 + 4 * i, 0);
        write_float(m, record + 0x10 + 4 * i, contact.point[i]);
        write_float(m, record + 0x1C + 4 * i, normal[i]);
    }
    write_float(m, record, 0);
    write_float(m, record + 4, contact.penetration);
    // A fresh moving contact wakes the body; the native integration still
    // supplies its ordinary mass/time scaling and maximum-speed safeguard.
    write_u16(m, temporary + 0x60, half(m, temporary + 0x60) & ~1u);
    auto call = context;
    call.f_odd = &call.f0.u32h;
    call.r29 = guest_address(stack - 0x380u);
    call.r4 = guest_address(temporary);
    call.r5 = guest_address(record);
    call.r6 = static_cast<std::int32_t>(std::bit_cast<std::uint32_t>(contact.response));
    func_80048FA4(m, &call);
    result.impulse = call.f0.fl;
    call.r4 = guest_address(temporary);
    func_80034594(m, &call);
    if (!std::isfinite(result.impulse) || result.impulse <= 0 ||
        !finite_range(m, temporary + 0x70, 4) || !finite_range(m, temporary + 0x170, 3))
        return {};
    for (unsigned i = 0; i < 3; ++i) {
        const float updated = scalar(m, temporary + 0x70 + 4 * i);
        result.velocity_delta[i] = updated - velocity[i];
        write_float(m, body + 0x70 + 4 * i, updated);
        write_float(m, body + 0x170 + 4 * i, scalar(m, temporary + 0x170 + 4 * i));
    }
    write_float(m, body + 0x7C, scalar(m, temporary + 0x7C));
    write_u16(m, body + 0x60, half(m, temporary + 0x60));
    result.applied = true;
    // Native static rails (62E6C) and solid scenery (5FFD0) send the accepted
    // impulse to56DA8; detached-body impacts use565BC. These retain the original
    // strength thresholds, sound selection, distance/volume rules and per-actor
    // sound-class cooldown (5992C: 0.4s bike / 0.3s rider). Calling the impulse
    // math alone omitted this feedback. Replay/client rejection above applies
    // to sound as well; rejected or separating contacts cannot emit anything.
    const unsigned listener = word(m, 0x800D8654u);
    const bool local = static_cast<std::int32_t>(word(m, actor + 8)) >= 0;
    if (word(m, actor) == actor_index &&
        (local || (valid_guest_range(listener, rider::stride) &&
                   finite_range(m, listener + 0x8C, 3)))) {
        call.r4 = guest_address(kind == Body::Bike ? bike : rider);
        call.r5 = static_cast<std::int32_t>(std::bit_cast<std::uint32_t>(result.impulse));
        if (kind == Body::Bike)
            func_80056DA8(m, &call);
        else
            func_800565BC(m, &call);
    }
    return result;
}
}
