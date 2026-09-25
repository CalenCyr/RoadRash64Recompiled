#pragma once

namespace rr64::race_pack_mod_ui {
// Register before the first mod scan, after configuring the pack directory.
// The normal Mods list/config owns its enabled state. Never loads terrain here.
void install();
// Called on the UI owner to apply a completed background import to the normal
// mod manager and its saved enabled preference.
void update();
}
