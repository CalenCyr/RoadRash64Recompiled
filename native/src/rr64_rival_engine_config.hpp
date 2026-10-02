#pragma once

namespace recomp::config {
class Config;
}

namespace rr64::rival_engine {
inline constexpr const char *enabled_option = "rr64_rival_engines_enabled";
inline constexpr const char *volume_option = "rr64_rival_engine_volume";
inline constexpr double default_volume_percent = 35.0;
void configure_volume(recomp::config::Config &config);
void apply_volume_config(const recomp::config::Config &config);
}
