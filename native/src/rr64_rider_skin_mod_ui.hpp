#pragma once
#include <string_view>
namespace recomp::mods {
struct ModFileHandle;
}
namespace rr64::rider_skin_mod_ui {
void install();
bool load_archive(const recomp::mods::ModFileHandle &file, std::string_view id);
}
