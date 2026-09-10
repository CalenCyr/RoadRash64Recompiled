#pragma once
#include <cstdint>
#include <filesystem>

namespace rr64::local_race_options {
// UI-thread persistence; guest hooks never perform disk I/O.
void initialize(const std::filesystem::path &directory);
void flush();
// Candidate roster ceiling, not the size of the engine's actor allocation.
constexpr unsigned max_ai(unsigned humans) {
    return humans >= 1 && humans <= 4 ? 11u - humans : 0u;
}
constexpr unsigned roster(unsigned ai, unsigned humans, unsigned stock) {
    return humans >= 1 && humans <= 4 ? humans + (ai < max_ai(humans) ? ai : max_ai(humans))
                                      : stock;
}
constexpr unsigned next_row(unsigned row, int direction, unsigned mask) {
    for (unsigned attempts = 0; attempts < 10; ++attempts) {
        row = (row + (direction < 0 ? 9u : 1u)) % 10u;
        if (mask & (1u << row))
            return row;
    }
    return 0;
}
}

extern "C" {
int rr64_local_player_roaming(unsigned char *memory, unsigned actor);
void rr64_local_options_menu_begin(unsigned char *memory);
void rr64_local_options_reset_race();
int rr64_local_options_input(unsigned char *memory);
int rr64_local_options_navigation(unsigned char *memory);
unsigned rr64_local_options_table(unsigned stock);
unsigned rr64_local_options_visible(unsigned row, unsigned stock);
void rr64_local_options_text(unsigned char *memory, unsigned row, unsigned buffer);
void rr64_local_options_finish(unsigned char *memory);
void rr64_local_options_race(unsigned char *memory);
unsigned rr64_local_bike_level(unsigned original);
unsigned rr64_local_bike_profile(unsigned char *memory, unsigned racer, unsigned profile);
}
