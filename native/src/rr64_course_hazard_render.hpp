#pragma once
#include <array>
#include <cstdint>
#include <span>
#include <string>

namespace rr64::course_hazards {
inline constexpr unsigned maximum_render_hazards = 128;
struct HazardDrawState {
    std::uint32_t id = 0, model = 0;
    std::array<float, 3> position{};         // RR rider world, atlas already included.
    std::array<std::uint16_t, 3> rotation{}; // MK x/y/z angles; yaw is element1.
    float scale = 1;                         // model's authored source scale is applied separately.
    bool visible = true;
    // Source-local model vertices only; position is already world-space.
    float source_to_world_scale = .05f;
    // 0=authored model rotation,1=yaw-facing sprite,2=full camera-facing sprite.
    unsigned billboard = 0;
    unsigned opacity = 255;
    unsigned tint = 0xffffff, environment_tint = 0;
};
struct HazardModelBounds {
    std::array<float, 3> minimum{}, maximum{}; // source local coordinates.
    float source_scale = 1;
    unsigned triangles = 0;
};
bool install_hazard_asset(std::span<const std::uint8_t>, std::string &error);
void reset_hazard_render_session();
void clear_hazard_asset();
bool hazard_model_bounds(unsigned model, HazardModelBounds &out);
bool draw_hazards(unsigned char *rdram, std::span<const HazardDrawState>);
struct HazardRenderStatistics {
    std::uint64_t draws = 0, refusals = 0;
    unsigned visible = 0, triangles = 0, command_bytes = 0, matrix_bytes = 0;
};
HazardRenderStatistics hazard_render_statistics();
} // namespace rr64::course_hazards
