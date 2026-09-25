#include "rr64_race_pack_mod_ui.hpp"
#include "rr64_race_pack_mod.hpp"
#include "rr64_mk64_import.hpp"
#include "recompui/recompui.h"
#include "composites/ui_mod_menu.h"
#include "base/ui_launcher.h"
#include "util/file.h"
#include "librecomp/mods.hpp"
#include "librecomp/config.hpp"
#include "librecomp/game.hpp"
#include "librecomp/files.hpp"
#include <fstream>

namespace rr64::race_pack_mod_ui {
namespace {
constexpr const char *mod_id = "rr64_mk64_race_pack";
#include "rr64_race_pack_mod_manifest.inc"

bool available() {
    const auto state = race_pack_mod::availability();
    return state == race_pack_mod::Availability::Enabled ||
           state == race_pack_mod::Availability::Disabled;
}

bool legacy_default() {
    // Migration only: the normal manager ignores this default once this ID
    // appears in mods.json. Never create, rewrite or delete the old preference.
    const auto path = recomp::get_config_path() / "race_packs.json";
    auto read = [](std::ifstream input) {
        return input ? nlohmann::json::parse(input, nullptr, false) : nlohmann::json{};
    };
    auto value = read(std::ifstream(path));
    if (value.is_discarded() || value.is_null()) value = read(recomp::open_input_backup_file(path));
    if (!value.is_object()) return true;
    const auto found = value.find("mk64_enabled");
    return found == value.end() || !found->is_boolean() ? true : found->get<bool>();
}

std::string status() {
    const auto importer = mk64_import::status();
    if (!importer.message.empty()) return importer.message;
    using A = race_pack_mod::Availability;
    switch (race_pack_mod::availability()) {
    case A::Unsupported: return "Unavailable in this build.";
    case A::Missing:
        return importer.tool_available
            ? "Not installed. Choose Import MK64 ROM to create the tracks from your Mario Kart 64 USA ROM. Your ROM stays on this computer."
            : "The bundled importer is missing. Re-extract the complete build download.";
    case A::Disabled:
    case A::Enabled:
        return race_pack_mod::can_change()
            ? "Set before starting the game."
            : "Restart the application to change; leave any online session first.";
    }
    return {};
}

std::string action_label() {
    const auto importer = mk64_import::status();
    if (importer.busy) return importer.can_cancel ? "Cancel Import" : "Please Wait";
    return available() ? "Reimport MK64 ROM" : "Import MK64 ROM";
}
bool action_enabled() {
    if (race_pack_mod::availability() == race_pack_mod::Availability::Unsupported) return false;
    const auto importer = mk64_import::status();
    return importer.busy ? importer.can_cancel : importer.tool_available && race_pack_mod::can_change();
}
void import_requested() {
    if (!action_enabled()) return;
    if (mk64_import::busy()) { mk64_import::cancel(); return; }
    recompui::file::open_file_dialog([](bool selected, const std::filesystem::path& path) {
        if (selected) mk64_import::start(path);
    });
}
}

void install() {
    static bool initialized = false;
    if (initialized) return;
    // The normal Mods entry owns the enabled preference. Its native action
    // creates ROM-derived content locally; the descriptor contains no assets.
    recomp::mods::register_mod_content_type({"rr64-course-pack.json", false,
                                             nullptr, nullptr, nullptr});
    recomp::mods::register_mod_toggle_policy(mod_id, {
        [] { return available() && race_pack_mod::can_change(); },
        [](bool enabled) {
            if (enabled && !available()) return false;
            return race_pack_mod::requested() == enabled || race_pack_mod::set_enabled(enabled);
        }, status, [] { return race_pack_mod::can_change(); }});
    const auto descriptor = available() && legacy_default()
        ? std::span<const unsigned char>(descriptor_enabled)
        : std::span<const unsigned char>(descriptor_disabled);
    recomp::mods::register_embedded_mod(mod_id, descriptor);
    recompui::register_mod_action(mod_id, action_label, action_enabled, import_requested);
    recompui::register_game_start_guard("rr64_mk64_import", [] {
        return mk64_import::busy()
            ? "Finish or cancel the MK64 import before starting the game. Progress is shown in Settings > Mods > MK64 Race Pack."
            : std::string{};
    });
    initialized = true;
}

void update() {
    if (mk64_import::update()) recomp::mods::enable_mod(mod_id, true);
}
}
