# Source layout and editing conventions

The native project code uses the formatting rules in `native/src/.clang-format`.
The readability pass expands compressed functions, branches and data structures
without changing names, expressions, constants, includes or execution order.
Use the formatter on the files you edit, rather than reformatting dependencies
or generated guest output. Existing comments explain original-engine contracts.

## Where to start

For the recent traffic, per-player eject, menu borders and geometry reuse changes,
see [Traffic and presentation editing](TRAFFIC_AND_PRESENTATION_EDITING.md).

| Area | Project-owned source | Preserve when editing |
| --- | --- | --- |
| Startup and settings | `main.cpp` | Initialization order, saved settings keys and device lifetime. This line-sensitive entry point was excluded from the formatting pass. |
| Runtime bridges | `rr64_runtime_shims.cpp`, `rr64_engine_layout.hpp` | Original argument/register contracts and checked guest-memory ranges. |
| Actor presentation | `rr64_actor_render_snapshot.cpp`, `rr64_actor_render_runtime.cpp`, `rr64_actor_held_pose.cpp` | Authored pose ownership and independently moving detached riders. |
| Weapons | `rr64_weapon_render.cpp` | Per-view rider roots, attachment ownership and restoration after drawing. |
| Terrain | `rr64_world_terrain.cpp`, `rr64_world_terrain_assets.cpp` | Cached original geometry, draw-distance admission and safe frame-buffer ownership. |
| Scenery | `rr64_world_objects.cpp`, `rr64_world_object_assets.cpp` | Stable placement identities, animation state and consistent course bounds. |
| Video | `rr64_video_mode.cpp`, view/frustum headers | Separate camera projection, HUD policy and original framebuffer restrictions. |
| Audio and music | `rr64_audio_output.cpp`, `rr64_music.cpp` | Queue lifetime, original fades and the shared music-volume setting. |
| Achievements | `rr64_achievements.cpp`, achievement helpers | Local save identity and bounded UI work. |
| Player profile names | `rr64_profile_names.cpp` | New-profile/solo boundaries; never overwrite loaded saves. |
| Campaign completion | `rr64_campaign_completion.cpp` | Native ending rewards, profile checksum and explicit save confirmation. |
| Offline cheats | `rr64_offline_modifiers.cpp` | Exclude humans, online, prediction, attract and highlight playback. |
| Imported-course progress | `rr64_experimental_course_route.cpp` | Signed movement and eligible human/AI authority; stock roads remain native. |
| Local race additions | `docs/CUSTOM_COP_EDITING_GUIDE.md` | Player counts, local-only gates and existing control IDs. |
| Online play | `rr64_netplay.cpp`, `rr64_online_menu.cpp` | Follow `docs/multiplayer-plan.md`; transport remains experimental. |

Guest hook locations are declared in `config/roadrash64.us.toml`; regenerate
`build/RecompiledFuncs` when those change. Do not hand-edit generated functions.
Dependency changes belong in the locked patch/override export, not just the
private dependency worktree. Keep compiler-generated files, ROMs, captures and
old test packages out of the source distribution.

## Reviewing a readability-only change

Keep include order and preprocessor boundaries. Do not rename persisted keys or
change numerical constants, pointer layouts, timing, branches or memory access
as incidental cleanup. Avoid moving code with line-sensitive macros. Small
contract comments are more useful than narrating each statement.

Compare source tokens and directives, then rebuild and run relevant offline
checks. Formatting can change debug line information and binary hashes; treat
the rebuilt candidate as a new artifact. Runtime acceptance and publication
remain separate steps. Optional diagnostics and validation guards are not dead
code merely because ordinary play leaves them disabled.
