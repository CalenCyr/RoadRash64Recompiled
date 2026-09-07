#pragma once

namespace rr64::achievements {

void initialize();
void set_enabled(bool enabled);
bool enabled();
void register_config_tab();
void initialize_toast_ui();
void update_ui();

} // namespace rr64::achievements
