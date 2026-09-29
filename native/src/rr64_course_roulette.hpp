#pragma once
#include "rr64_netplay.hpp"
#include "rr64_mk64_item_state.hpp"
#include <algorithm>

namespace rr64::course_items {
struct RouletteDisplay {
    // Only this field may enter the original weapon/inventory path. Reward
    // tags 15/16 are presentation values, never native inventory indices.
    unsigned weapon = 0;
    bool rolling = false, blink_off = false;
    unsigned reward = 0;
    bool reward_visible = false;
};
inline unsigned roulette_sprite(const RouletteDisplay &display) noexcept {
    if (display.reward == netplay::kCourseRewardAttackX2)
        return 0xBF;
    if (display.reward == netplay::kCourseRewardAttackX4)
        return 0xC0;
    return 0xA3 + display.weapon;
}
inline unsigned roulette_reward(const netplay::CourseWeaponRoll &roll, unsigned clock,
                                unsigned slot, bool mk64_enabled = true) noexcept {
    const unsigned age = clock >= roll.start_clock ? clock - roll.start_clock : 0;
    const unsigned t = std::min(age, netplay::kCourseRouletteTicks - 1);
    const unsigned step = t < 24 ? t / 2 : 12 + (t - 24) / 4;
    unsigned seed = roll.generation * 0x9e3779b9u + slot * 0x85ebca6bu;
    seed ^= seed >> 16;
    // OFF retains all thirteen native weapons and both native multipliers.
    // Seven is coprime to either pool; presentation never consumes gameplay RNG.
    const unsigned count = mk64_enabled ? 30 : 15;
    return 2 + ((seed % count + step * 7) % count);
}
inline RouletteDisplay roulette_display(const netplay::CourseWeaponRoll &roll,
                                        unsigned clock, unsigned slot,
                                        unsigned equipped,
                                        unsigned active_effect = 0,
                                        bool mk64_enabled = true) noexcept {
    if (!roll.phase || roll.phase > 2 || clock < roll.start_clock ||
        !netplay::valid_course_reward(roll.weapon))
        return {equipped};
    const unsigned age = clock - roll.start_clock;
    if (roll.phase == 1) {
        const unsigned reward = roulette_reward(roll, clock, slot, mk64_enabled);
        const unsigned safe_weapon = netplay::is_course_weapon_reward(reward)
            ? reward : (equipped <= 14 ? equipped : 0);
        return {safe_weapon, true, false, reward};
    }
    // Imported-item art replaces the queued weapon sprite separately. Keep
    // the real weapon ID here for all native inventory reads.
    if (mk64_items::is_reward(roll.weapon))
        return {equipped};
    // Switching weapons immediately remains possible after the real award.
    const unsigned effect = netplay::course_reward_effect(roll.weapon);
    const bool matches = effect ? active_effect == effect : equipped == roll.weapon;
    const bool show = matches && age >= netplay::kCourseRouletteTicks &&
                      age < netplay::kCourseRouletteTicks + netplay::kCourseRewardBlinkTicks;
    return {equipped, false,
            show && ((age - netplay::kCourseRouletteTicks) / 3) % 2 != 0,
            show ? unsigned(roll.weapon) : 0u, show};
}
} // namespace rr64::course_items
