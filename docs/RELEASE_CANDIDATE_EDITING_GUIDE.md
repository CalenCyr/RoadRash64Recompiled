# Release candidate: where to edit

This guide describes the retained HUD-Rollback behavior. Edit source modules,
not generated files in build/RecompiledFuncs. Game hooks are declared in
config/roadrash64.us.toml; their native entry points are in rr64_native.hpp.

| Change | Start here | Contract to preserve |
| --- | --- | --- |
| Graphics options | native/src/main.cpp; frontend ui_config_tab_graphics.cpp | Draw Distance owns terrain and scenery range. MAX LOD is a separate rider/bike policy. FPS changes presentation, not simulation. |
| World visibility | native/src/rr64_world_terrain.cpp, rr64_world_objects.cpp, rr64_local_world_window.hpp | The historical local filename now supports the global slider. Share terrain-cell admission with scenery; retain course-island bounds. |
| Rider/bike presentation | native/src/rr64_actor_render_snapshot.cpp, rr64_weapon_render.cpp | Weapon roots need the current view's rider pose and exact restoration after rendering. Keep recovery and animation ownership intact. |
| Music volume | native/src/rr64_music.cpp | The saved custom_music_volume key controls both soundtracks. Track original sequence changes independently; retain authored fades. |
| Local players | native/src/rr64_local_players.hpp and main.cpp; frontend controls page | Names and device assignments are separate. Connected controllers determine available input. |
| Catch-up policy | config/roadrash64.us.toml; docs/MULTIPLAYER_CATCHUP.md | Only the dedicated relocation routine and six warning predicates are disabled. Shared crash recovery remains. |
| HUD and aspect | docs/RT64_EDITING_GUIDE.md; native/src/rr64_video_mode.hpp | Recent split-HUD and forced-Wide experiments are removed. Retain earlier presentation limits rather than silently reinstating those changes. |
| Online multiplayer | docs/multiplayer-plan.md | Follow current in-game design; legacy relay/lobby approaches are not the foundation. Online remains experimental. |
| Source distribution | dependencies.lock.json; scripts/setup_dependencies.py | Export tracked dependency patches and byte-locked overrides together; check a fresh reconstruction. |

Runtime acceptance: user accepted HUD-Rollback as a release candidate. Cleanup
verification is offline until a separately authorized test. Prior Wide-mode
experiments did not pass visual acceptance. No new sky/HUD fix is claimed.

Diagnostics are optional developer support, not automatically dead code.
Do not remove cadence, topology or buffer-lifetime checks as logging: they can
authorize rendering work. Use the existing RT64 and frontend editing guides
for deeper ownership constraints. Do not publish without authorization.
