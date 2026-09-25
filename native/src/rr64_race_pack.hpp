#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string_view>

namespace rr64::race_pack {
// Catalogue storage is immutable for one game session. Selection is a draft
// menu choice; the loaded course changes only at the native race boundary.
struct CourseMenuEntry {
    std::string_view group_id, group_name, course_id, course_name;
    std::span<const std::uint8_t> preview_rgba16_be;
    unsigned preview_width, preview_height;
    unsigned safe_stock_level, safe_stock_race;
};
std::span<const CourseMenuEntry> menu_courses() noexcept;
std::optional<std::size_t> selected_course() noexcept;
bool select_course(std::size_t index);
void select_stock() noexcept;
} // namespace rr64::race_pack
