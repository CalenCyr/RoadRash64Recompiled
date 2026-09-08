# RecompFrontend pre-change audit

Local only. No publication or game launch. Baseline matches the current RT64-cleanup candidate exactly.

Original patch: 1517 lines, 77988 bytes, 18 files.

Remove only startup progress tracing, logging-only temporaries, routine focus messages and redundant EOF changes. Preserve exception diagnostics, rumble throttling/lifetime, popup input guards, bindings, graphics conversion and supported configuration. Correct framerate help text. Local-player wiring is deferred to the next implementation step. No performance gain is claimed.

## recompinput/include/recompinput/input_state.h

Active declarations/state: throttled rumble, focus boundaries, input-device detection. Retain.

- `@@ -20,6 +20,10 @@ namespace recompinput {`: classification follows the logical subsets above.
- `@@ -29,7 +33,13 @@ namespace recompinput {`: classification follows the logical subsets above.
- `@@ -42,6 +52,10 @@ namespace recompinput {`: classification follows the logical subsets above.

## recompinput/include/recompinput/input_types.h

Active eject/spoke-jam actions; retain enum names/order. EOF-only addition redundant.

- `@@ -40,7 +40,9 @@ namespace recompinput {`: classification follows the logical subsets above.
- `@@ -132,3 +134,4 @@ namespace recompinput {`: classification follows the logical subsets above.

## recompinput/include/recompinput/profiles.h

Active profile action lookup declaration; retain. EOF-only addition redundant.

- `@@ -61,6 +61,7 @@ namespace recompinput {`: classification follows the logical subsets above.
- `@@ -69,3 +70,4 @@ namespace recompinput {`: classification follows the logical subsets above.

## recompinput/src/input_events.cpp

Active device/focus event routing. Keep hotplug error context; no behavioral changes.

- `@@ -1,3 +1,5 @@`: classification follows the logical subsets above.
- `@@ -43,6 +45,7 @@ bool sdl_event_filter(void* userdata, SDL_Event* event) {`: classification follows the logical subsets above.
- `@@ -74,10 +77,18 @@ bool sdl_event_filter(void* userdata, SDL_Event* event) {`: classification follows the logical subsets above.
- `@@ -100,10 +111,30 @@ bool sdl_event_filter(void* userdata, SDL_Event* event) {`: classification follows the logical subsets above.
- `@@ -122,6 +153,7 @@ bool sdl_event_filter(void* userdata, SDL_Event* event) {`: classification follows the logical subsets above.
- `@@ -152,6 +184,9 @@ bool sdl_event_filter(void* userdata, SDL_Event* event) {`: classification follows the logical subsets above.
- `@@ -270,6 +305,10 @@ void handle_events() {`: classification follows the logical subsets above.

## recompinput/src/input_mapping.cpp

Active default action bindings. EOF-only addition redundant.

- `@@ -35,6 +35,8 @@ namespace recompinput {`: classification follows the logical subsets above.
- `@@ -97,3 +99,4 @@ namespace recompinput {`: classification follows the logical subsets above.

## recompinput/src/input_state.cpp

Active synchronized rumble, finite-duration/coalesced output and hotplug cleanup; active prompt-device tracking. Keep all behavior. Remove only routine focus printf/flush; retain actual SDL failure report.

- `@@ -1,7 +1,8 @@`: classification follows the logical subsets above.
- `@@ -22,13 +23,19 @@ static struct {`: classification follows the logical subsets above.
- `@@ -69,7 +76,101 @@ void recompinput::poll_inputs() {`: classification follows the logical subsets above.
- `@@ -79,55 +180,141 @@ static float smoothstep(float from, float to, float amount) {`: classification follows the logical subsets above.
- `@@ -374,6 +561,38 @@ bool recompinput::all_input_disabled() {`: classification follows the logical subsets above.
- `@@ -392,11 +611,35 @@ void recompinput::add_controller_state(SDL_JoystickID joystick_id, SDL_GameContr`: classification follows the logical subsets above.

## recompinput/src/input_types.cpp

Active descriptions for configurable actions. EOF-only addition redundant.

- `@@ -17,6 +17,8 @@ constexpr std::array<std::string, num_game_inputs> get_default_game_input_descri`: classification follows the logical subsets above.
- `@@ -266,3 +268,4 @@ std::string recompinput::InputField::to_string() const {`: classification follows the logical subsets above.

## recompinput/src/profiles.cpp

Active selected-profile action lookup. Preserve keyboard/controller and index guards. EOF-only addition redundant.

- `@@ -371,6 +371,20 @@ namespace recompinput {`: classification follows the logical subsets above.
- `@@ -575,3 +589,4 @@ namespace recompinput {`: classification follows the logical subsets above.

## recompui/include/recompui/config.h

Active persisted VSync key; retain.

- `@@ -58,6 +58,7 @@ namespace recompui {`: classification follows the logical subsets above.

## recompui/include/recompui/recompui.h

Active UI update callback declaration, used after launcher hides; retain.

- `@@ -42,6 +42,10 @@ namespace recompui {`: classification follows the logical subsets above.

## recompui/src/base/ui_launcher.cpp

Active raster background API and fallback title styling. The game removes the default title, but fallback styling is harmless supported behavior; removability uncertain, retain.

- `@@ -537,6 +537,12 @@ namespace recompui {`: classification follows the logical subsets above.
- `@@ -573,9 +579,10 @@ namespace recompui {`: classification follows the logical subsets above.
- `@@ -587,6 +594,22 @@ namespace recompui {`: classification follows the logical subsets above.

## recompui/src/base/ui_launcher.h

Declarations and lifetime state for raster background; retain.

- `@@ -6,6 +6,7 @@`: classification follows the logical subsets above.
- `@@ -56,6 +57,7 @@ namespace recompui {`: classification follows the logical subsets above.
- `@@ -71,6 +73,7 @@ namespace recompui {`: classification follows the logical subsets above.

## recompui/src/base/ui_state.cpp

Mixed: active popup dismissal input guard and post-input update callback. Constructor/menu progress tracing and logging-only temporaries are experimental leftovers. Retain exception reports and rethrow behavior.

- `@@ -1,3 +1,4 @@`: classification follows the logical subsets above.
- `@@ -5,6 +6,9 @@`: classification follows the logical subsets above.
- `@@ -34,6 +38,11 @@`: classification follows the logical subsets above.
- `@@ -223,28 +232,48 @@ public:`: classification follows the logical subsets above.
- `@@ -257,29 +286,47 @@ public:`: classification follows the logical subsets above.
- `@@ -419,6 +466,12 @@ public:`: classification follows the logical subsets above.
- `@@ -431,6 +484,9 @@ public:`: classification follows the logical subsets above.
- `@@ -513,8 +569,22 @@ void init_hook(plume::RenderInterface* interface, plume::RenderDevice* device) {`: classification follows the logical subsets above.
- `@@ -859,6 +929,12 @@ void draw_hook(plume::RenderCommandList* command_list, plume::RenderFramebuffer*`: classification follows the logical subsets above.
- `@@ -1133,3 +1209,4 @@ void recompui::drop_files(const std::list<std::filesystem::path> &file_list) {`: classification follows the logical subsets above.

## recompui/src/composites/ui_mod_installer.cpp

Active legacy archive rejection message; retains supported RTZ/NRM install flow.

- `@@ -1,5 +1,8 @@`: classification follows the logical subsets above.
- `@@ -313,6 +316,18 @@ namespace recompui {`: classification follows the logical subsets above.

## recompui/src/config/ui_config_tab_graphics.cpp

Active aspect/FPS/VSync options and conversion. Correct misleading always-synchronized help text; keep options and stored values unchanged.

- `@@ -66,7 +66,9 @@ namespace recompui {`: classification follows the logical subsets above.
- `@@ -77,10 +79,16 @@ namespace recompui {`: classification follows the logical subsets above.
- `@@ -151,12 +159,24 @@ namespace recompui {`: classification follows the logical subsets above.
- `@@ -171,9 +191,13 @@ namespace recompui {`: classification follows the logical subsets above.
- `@@ -206,7 +230,6 @@ namespace recompui {`: classification follows the logical subsets above.
- `@@ -286,7 +309,7 @@ namespace recompui {`: classification follows the logical subsets above.
- `@@ -304,23 +327,15 @@ namespace recompui {`: classification follows the logical subsets above.

## recompui/src/core/ui_context.cpp

Active binary stylesheet read with error checks; preserve. Replace development marker with short explanation.

- `@@ -2,6 +2,8 @@`: classification follows the logical subsets above.
- `@@ -235,12 +237,25 @@ void recompui::init_styling(const std::filesystem::path& rcss_file) {`: classification follows the logical subsets above.

## recompui/src/renderer/rt64_render_context.cpp

Mixed: retain graphics bridge, value initialization, frontend-owned data path and present lock. Remove startup progress tracing. Keep explicit opt-in UI-hook bypass as a troubleshooting path, not normal behavior; risk of removal uncertain.

- `@@ -2,6 +2,9 @@`: classification follows the logical subsets above.
- `@@ -79,8 +82,12 @@ RT64::UserConfiguration::AspectRatio to_rt64(ultramodern::renderer::AspectRatio`: classification follows the logical subsets above.
- `@@ -172,6 +179,15 @@ void set_application_user_config(RT64::Application* application, const ultramode`: classification follows the logical subsets above.
- `@@ -219,10 +235,29 @@ ultramodern::renderer::GraphicsApi map_graphics_api(RT64::UserConfiguration::Gra`: classification follows the logical subsets above.
- `@@ -267,17 +302,35 @@ renderer::RT64Context::RT64Context(uint8_t* rdram, ultramodern::renderer::Window`: classification follows the logical subsets above.
- `@@ -305,7 +358,13 @@ renderer::RT64Context::RT64Context(uint8_t* rdram, ultramodern::renderer::Window`: classification follows the logical subsets above.
- `@@ -313,8 +372,13 @@ renderer::RT64Context::RT64Context(uint8_t* rdram, ultramodern::renderer::Window`: classification follows the logical subsets above.
- `@@ -371,8 +435,19 @@ bool renderer::RT64Context::update_config(const ultramodern::renderer::GraphicsC`: classification follows the logical subsets above.

## recompui/src/renderer/ui_renderer.cpp

Mixed: startup tracing is experimental leftover. Retain sampler value initialization and exception report/rethrow. No shader/resource algorithm changes.

- `@@ -5,6 +5,9 @@`: classification follows the logical subsets above.
- `@@ -158,66 +161,126 @@ class RmlRenderInterface_RT64_impl : public Rml::RenderInterfaceCompatibility {`: classification follows the logical subsets above.
- `@@ -225,10 +288,15 @@ public:`: classification follows the logical subsets above.
- `@@ -254,19 +322,31 @@ public:`: classification follows the logical subsets above.
- `@@ -275,9 +355,19 @@ public:`: classification follows the logical subsets above.
- `@@ -692,7 +782,20 @@ void recompui::RmlRenderInterface_RT64::reset() {`: classification follows the logical subsets above.

## Verification and deferred concerns

Build main/direct-start and existing input, popup, settings, video and renderer reuse checks. Reconstruct the exported frontend patch from the pinned revision and compare all changed files. Keep the prior test folder and executable intact. Runtime confirmation needs a new ready.

The input system is still initialized in single-player mode; four-slot assignment exists upstream but is not exposed by this integration. Custom local in-game names are absent. These are follow-up implementation requirements, not dead frontend code. Per-frame focus atomics, device-type locking, rumble locking and post-input callback work remain active; optimization needs profiling rather than deletion.
