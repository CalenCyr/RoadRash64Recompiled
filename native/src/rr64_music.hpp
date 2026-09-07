#pragma once
#include <cstdint>
#include <filesystem>
#include <span>
namespace recomp::config { class Config; }
namespace rr64::music {
void configure(recomp::config::Config&, const std::filesystem::path&);
void update_ui();
void mix(std::span<std::int16_t>, std::uint32_t);
}
extern "C" unsigned int rr64_music_volume_update(unsigned int);
extern "C" unsigned int rr64_music_stock_volume(unsigned int);
