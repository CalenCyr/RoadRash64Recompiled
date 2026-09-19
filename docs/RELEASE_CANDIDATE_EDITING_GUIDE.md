# Release candidate: contributor guide

Current active source is `work/release-1.2-online`, based on published v1.2.0.
The parent workspace's old renderer experiment is not a build source. Online
remains experimental; local gameplay acceptance does not prove online parity.

## Where to make changes

| Feature | Source entry point | Constraint |
| --- | --- | --- |
| Launcher, input actions, version | `native/src/main.cpp` | Preserve per-player device ownership and dedicated-action vs directional-attack separation. |
| Local race options and AI cops | `native/src/rr64_local_race_options.cpp` | Packed options are shared with online setup; preserve defaults and roster limits. |
| Cop roles, arrests, victory | `native/src/rr64_custom_cop.cpp` | Use the same eligible racer set for initial counts and victory. |
| Cop controls, recovery and posts | `native/src/rr64_custom_cop_runtime.cpp` | Replay state must stay isolated; LB gestures and RB trick must not consume down attacks. |
| Cop selection reminder / bust display | `native/src/rr64_custom_cop_ui.cpp` | Keep menu footer separate from per-view race HUD. |
| Backtracking / nearest-road recovery | `native/src/rr64_roaming_route.cpp` | Human-only reverse progress; native recovery tail owns pose, terrain height and camera. |
| Split-screen HUD and countdown | `native/src/rr64_hud_widgets.cpp` | Scope each native text/sprite producer, restore state, never anchor world pickups as HUD. |
| Plain sky | `native/src/rr64_sky_sprites.cpp` | Remove only the shared producer's cloud queue entries; preserve all other sprites. |
| Terrain and objects | `native/src/rr64_world_terrain.cpp`, `rr64_world_objects.cpp` | Visual range and collision residency have different ownership. |
| Rider, bike, weapon presentation | `native/src/rr64_actor_render_snapshot.cpp`, `rr64_weapon_render.cpp` | Per-view transforms must be restored after drawing. |
| Online transport | `native/src/rr64_netplay.cpp` | Protocol 38 peers only; follow `docs/multiplayer-plan.md`. |
| Online simulation / prediction | `native/src/rr64_authoritative_step.cpp`, `rr64_prediction_reconcile.cpp` | Preserve authoritative ownership and replay isolation. |
| Positional voice | `native/src/rr64_voice_chat.cpp` | Use body location, not bike location; no wall occlusion claim. |

## Hooks and memory

Edit `config/roadrash64.us.toml` and handwritten native helpers. Never hand-edit
`build/RecompiledFuncs`; regenerate it with N64Recomp. `rr64_native.hpp` is the
C-compatible hook surface; Custom Cop declarations live in `rr64_custom_cop.hpp`.
`rr64_engine_layout.hpp` contains validated guest-memory access helpers. Native
addresses refer to the supported USA ROM revision, not host pointers.

Comments should explain ownership, the native call-site contract, and why a guard
exists. Keep cosmetic changes separate from gameplay corrections. Do not remove
fallbacks, prediction fixtures or optional diagnostics because one mode bypasses them.

## Checks and release handoff

Build Release and run the affected smoke targets: `RR64LocalRaceOptionsSmoke`,
`RR64RoamingRouteSmoke`, `RR64HUDWidgetsSmoke`, `RR64SkySpritesSmoke`,
`RR64OnlineViewportSmoke`, `RR64MenuEjectSmoke`, `RR64CombatCreditSmoke`, and
`RR64VoiceChatSmoke`. Some other targets require arguments or generated fixtures;
read their source before invoking them. Passing these is not an online playtest.

Package runtime files from an explicit inventory. Exclude ROMs, saves, private
replay captures, generated sky experiments, diagnostics launchers and historical
builds. Normal capture remains opt-in. Keep error reporting. Test the packaged
executable before publication; wait for the user's launch signal.

Deeper notes: `roaming-distance-recovery.md`, `custom-cop-backhand.md`,
`custom-cop-victory-jam.md`, `pickup-hud-exclusion.md`, `RT64_EDITING_GUIDE.md`,
`FRONTEND_EDITING_GUIDE.md`, and `multiplayer-plan.md`.
