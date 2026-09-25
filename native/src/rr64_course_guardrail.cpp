#include "rr64_course_guardrail.hpp"
#include "rr64_engine_layout.hpp"
#include "rr64_experimental_course.hpp"
#include "rr64_netplay.hpp"
#include "rr64_prediction_replay.hpp"

#include <algorithm>
#include <bit>
#include <cmath>
#include <cstring>

extern "C" void func_80012AF8(unsigned char *, recomp_context *);
extern "C" void func_80012B9C(unsigned char *, recomp_context *);
extern "C" void func_80012DBC(unsigned char *, recomp_context *);
extern "C" void func_80034594(unsigned char *, recomp_context *);
extern "C" void func_80048A1C(unsigned char *, recomp_context *);

namespace rr64::course_guardrail {
namespace {
using namespace engine;

unsigned word(unsigned char *m, unsigned address) noexcept {
    unsigned value = 0;
    read_u32(m, address, value);
    return value;
}
unsigned half(unsigned char *m, unsigned address) noexcept {
    std::uint16_t value = 0;
    read_u16(m, address, value);
    return value;
}
float scalar(unsigned char *m, unsigned address) noexcept {
    float value = 0;
    read_float(m, address, value);
    return value;
}
bool finite_range(unsigned char *m, unsigned address, unsigned count) noexcept {
    for (unsigned i = 0; i < count; ++i)
        if (!std::isfinite(scalar(m, address + 4 * i)))
            return false;
    return true;
}

unsigned mounted_bike(unsigned char *m, unsigned actor_index) noexcept {
    if (!m || !experimental_course::active() || actor_index >= kMaximumRacers ||
        half(m, globals::gameplay_pause_state))
        return 0;
    const unsigned actor = 0x800D8570u + actor_index * 0x118u;
    const unsigned bike = word(m, actor + 0xE0), rider = word(m, actor + 0xE4);
    if (!half(m, actor + 0x24) || !valid_guest_range(bike, bike::stride) ||
        !valid_guest_range(rider, rider::stride) || word(m, bike + 4) != actor ||
        word(m, rider + 4) != actor || word(m, bike + bike::rider_pointer) != rider ||
        word(m, rider + rider::bike_pointer) != bike ||
        !half(m, bike + bike::rider_attached) || !half(m, rider + rider::bike_attached))
        return 0;
    return bike;
}

bool rail_normal(const Contact &contact, Vec3 &normal) noexcept {
    if (!contact.tagged_low_rail || contact.sphere_index > 2 ||
        !std::isfinite(contact.top) ||
        !std::all_of(contact.normal.begin(), contact.normal.end(),
                     [](float value) { return std::isfinite(value); }))
        return false;
    // Native4C308 supplies a horizontal segment-side normal. A finite triangle
    // top edge can have a vertical component; recover its same horizontal side.
    const float norm2 = contact.normal[0] * contact.normal[0] +
                        contact.normal[1] * contact.normal[1];
    if (!std::isfinite(norm2) || norm2 < 1e-12f)
        return false;
    const float inverse = 1.0f / std::sqrt(norm2);
    normal = {contact.normal[0] * inverse, contact.normal[1] * inverse, 0};
    return true;
}

// Same scratch boundary as course_impact. Native helpers run below the private
// copied body; neither native call frames nor temporary bytes survive the call.
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

Eligibility eligibility(unsigned char *m, std::uint32_t actor_index,
                        const Contact &contact) noexcept {
    const unsigned bike = mounted_bike(m, actor_index);
    Vec3 normal;
    if (!bike || !rail_normal(contact, normal) ||
        !finite_range(m, bike + 0xB8, 3) || !finite_range(m, bike + 0x174, 1) ||
        !finite_range(m, bike + 0x4C8, 1) || !finite_range(m, 0x800A2174u, 1) ||
        !finite_range(m, 0x800A2188u, 1) || !finite_range(m, 0x80004CA8u, 1))
        return Eligibility::Blocked;
    const float top = contact.top -
        (contact.sphere_index ? scalar(m, 0x80004CA8u) : 0.0f);
    const float root_z = scalar(m, bike + 0x174);
    const unsigned latch = half(m, bike + 0x818);
    // 8004CD0C selects bike+B8; 8004CEE8..4CF68 uses that exact reference
    // vector, rail-top altitude and80048A1C. No object-height shortcut is used.
    float alignment = normal[0] * scalar(m, bike + 0xB8);
    alignment += normal[1] * scalar(m, bike + 0xBC);
    if (!(alignment <= scalar(m, 0x800A2188u) || top < root_z || latch) ||
        !(top < root_z + scalar(m, 0x800A2174u)))
        return Eligibility::Blocked;
    const unsigned state = word(m, bike + 0x100);
    // Read-only equivalent of80048A1C; an actual call is also made by apply.
    if (!(scalar(m, bike + 0x4C8) > 0 || state == 9 || state == 8 || latch))
        return Eligibility::Blocked;
    return latch ? Eligibility::ContinuingVault : Eligibility::FirstVault;
}

Result apply(unsigned char *m, const recomp_context &context,
             std::uint32_t actor_index, const Contact &contact) {
    Result result;
    if (!m || prediction::active())
        return result;
    const auto rules = netplay::get_physics_rules();
    if (rules.active && (!rules.connected || !rules.authoritative || !rules.is_host ||
                         rules.phase != netplay::Phase::Race))
        return result;
    const auto decision = eligibility(m, actor_index, contact);
    if (decision == Eligibility::Blocked)
        return result;
    if (decision == Eligibility::ContinuingVault) {
        result.consumed = true;
        return result;
    }
    const unsigned bike = mounted_bike(m, actor_index), body = bike + 0x108u;
    const unsigned stack = static_cast<unsigned>(context.r29);
    if (!valid_guest_range(stack - 0x900u, 0x900u) ||
        !finite_range(m, body, 9) || !finite_range(m, body + 0x64, 18) ||
        !finite_range(m, body + 0xC0, 9) || !finite_range(m, body + 0x118, 9) ||
        !finite_range(m, 0x800A2168u, 3) || scalar(m, body) <= 0 ||
        scalar(m, body + 0xC) <= 0 || scalar(m, body + 0x10) <= 0 ||
        scalar(m, body + 0x14) <= 0 || !std::isfinite(scalar(m, 0x8009CBA8u)) ||
        scalar(m, 0x8009CBA8u) <= 0 || !std::isfinite(scalar(m, 0x8009CBB4u)) ||
        scalar(m, 0x8009CBB4u) <= 0)
        return result;
    Vec3 normal;
    if (!rail_normal(contact, normal))
        return result;
    Vec3 velocity{};
    for (unsigned i = 0; i < 3; ++i)
        velocity[i] = scalar(m, body + 0x70 + 4 * i);
    float approach = -(velocity[0] * normal[0] + velocity[1] * normal[1]);
    approach = std::max(0.0f, approach);
    const float mass = scalar(m, body);
    const float impulse = (scalar(m, 0x800A2170u) +
                           scalar(m, 0x800A216Cu) * approach) * mass;
    if (!std::isfinite(impulse) || impulse < 0)
        return result;

    Scratch scratch(m, stack - 0x900u);
    const unsigned temporary = stack - 0x300u, direction = stack - 0x80u;
    std::memcpy(m + (temporary - kRdramBegin), m + (body - kRdramBegin), 0x200);
    auto call = context;
    call.f_odd = &call.f0.u32h;
    call.r29 = guest_address(stack - 0x380u);
    call.r4 = guest_address(bike);
    func_80048A1C(m, &call);
    if (!call.r2)
        return result;
    for (unsigned i = 0; i < 3; ++i) {
        write_float(m, temporary + 0x8C + 4 * i, scalar(m, body + 0x64 + 4 * i));
        write_float(m, temporary + 0x98 + 4 * i, velocity[i]);
        write_float(m, temporary + 0xF4 + 4 * i, 0);
        write_float(m, temporary + 0x100 + 4 * i, 0);
        write_float(m, direction + 4 * i,
                    i == 2 ? scalar(m, 0x800A2168u) : normal[i]);
    }
    // Exact static-rail branch8004CFA0..4D038, including cancellation of an
    // already-rising body's vertical momentum before choosing boost magnitude.
    call.r4 = guest_address(direction);
    func_80012AF8(m, &call);
    auto scale_impulse = [&](float magnitude) {
        call.r4 = guest_address(direction);
        call.r5 = guest_address(temporary + 0x100u);
        call.r6 = static_cast<std::int32_t>(std::bit_cast<std::uint32_t>(magnitude));
        func_80012DBC(m, &call);
    };
    scale_impulse(impulse);
    const float vertical = scalar(m, temporary + 0x108u) - mass * velocity[2];
    write_float(m, temporary + 0x108u, vertical);
    float magnitude = 0;
    if (vertical >= 0) {
        call.r4 = guest_address(temporary + 0x100u);
        func_80012B9C(m, &call);
        magnitude = call.f0.fl;
    }
    if (!std::isfinite(magnitude))
        return result;
    scale_impulse(magnitude);
    write_u16(m, temporary + 0x60u, half(m, temporary + 0x60u) & ~1u);
    call.r4 = guest_address(temporary);
    func_80034594(m, &call);
    if (!finite_range(m, temporary + 0x70u, 4))
        return result;
    for (unsigned i = 0; i < 3; ++i) {
        const float updated = scalar(m, temporary + 0x70 + 4 * i);
        result.velocity_delta[i] = updated - velocity[i];
        write_float(m, body + 0x70 + 4 * i, updated);
    }
    write_float(m, body + 0x7Cu, scalar(m, temporary + 0x7Cu));
    write_u16(m, body + 0x60u, half(m, temporary + 0x60u));
    write_u16(m, bike + 0x818u, 1);
    result.consumed = true;
    result.launched = true;
    return result;
}
}
