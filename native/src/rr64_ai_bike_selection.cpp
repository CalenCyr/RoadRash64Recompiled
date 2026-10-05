#include "rr64_ai_bike_selection.hpp"
#include "rr64_engine_layout.hpp"
#include <algorithm>
#include <array>
#include <cstdlib>

extern "C" void func_8001A500(unsigned char *, recomp_context *);
extern "C" unsigned rr64_online_race_choice(unsigned guest, unsigned original, unsigned bike);

extern "C" unsigned rr64_ai_bike_profile(unsigned char *rdram, void *context, unsigned profile) {
    using namespace rr64::engine;
    constexpr unsigned profiles = 0x800A3460u, profile_limit = 160;
    constexpr unsigned actors = 0x800D8570u, actor_stride = 0x118u;
    if (!rdram || !context || profile < profiles + 4 * 16 ||
        profile >= profiles + profile_limit * 16 || (profile - profiles) % 16)
        return profile;
    const auto &ctx = *static_cast<recomp_context *>(context);
    const unsigned actor = static_cast<unsigned>(ctx.r4);
    if (actor < actors || actor >= actors + kMaximumRacers * actor_stride ||
        (actor - actors) % actor_stride)
        return profile;
    if ((MEM_HU(0x24, guest_address(actor)) && !MEM_HU(0x26, guest_address(actor))) ||
        rr64_online_race_choice((actor - actors) / actor_stride, ~0u, 1) != ~0u)
        return profile;

    const auto byte = [&](unsigned row, unsigned offset) {
        return unsigned(MEM_BU(offset, guest_address(row)));
    };
    const unsigned tier = byte(profile, 8), family = byte(profile, 9);
    if (tier < 1 || tier > 7 || family < 1 || family > 4)
        return profile; // Police have a separate quota, even on Insanity bikes.

    std::array<unsigned, profile_limit> models{};
    unsigned count = 0;
    const auto eligible = [&](unsigned row) {
        return byte(row, 8) == tier && byte(row, 9) >= 1 && byte(row, 9) <= 4;
    };
    for (unsigned i = 4; i < profile_limit; ++i) {
        const unsigned row = profiles + i * 16;
        if (byte(row, 8) == 12)
            break;
        if (!eligible(row))
            continue;
        const unsigned model = byte(row, 10);
        if (std::find(models.begin(), models.begin() + count, model) == models.begin() + count)
            models[count++] = model;
    }
    if (count < 2)
        return profile; // Scooter profiles all use the same model.

    const auto draw = [&](unsigned bound) {
        auto random = ctx;
        func_8001A500(rdram, &random); // Shared native seed, no caller-register or stack changes.
        return unsigned((std::uint64_t(std::uint32_t(random.r2)) * bound) >> 32);
    };
    // Deduplicate model IDs: Insanity has unequal donor counts, and Level 1
    // reverses the Firecracker/DuMoto family ordering used by later tiers.
    const unsigned model = models[draw(count)];
    if (model == byte(profile, 10))
        return profile;

    std::array<unsigned, profile_limit> donors{};
    unsigned donor_count = 0;
    int closest = 256;
    for (unsigned i = 4; i < profile_limit; ++i) {
        const unsigned row = profiles + i * 16;
        if (byte(row, 8) == 12)
            break;
        if (!eligible(row) || byte(row, 10) != model)
            continue;
        const int distance = std::abs(int(byte(row, 13)) - int(byte(profile, 13)));
        if (distance < closest) {
            closest = distance;
            donor_count = 0;
        }
        if (distance == closest)
            donors[donor_count++] = row;
    }
    return donor_count ? donors[donor_count == 1 ? 0 : draw(donor_count)] : profile;
}
