# Experimental MK64 items (1.4.2)

**Still being tested.** Enable **MK64 Items** in imported-track race options;
the choice is remembered. Off keeps Road Rash weapons and x2/x4 multipliers.
Use your own MK64 ROM through Mods > MK64 Race Tracks > Import. Converter c40
is required for item art/audio; existing c40 packs need no new import.

Items include shells (green/red/ground-running blue), triples, bananas/bunches,
fake boxes, mushrooms/triples/golden mushrooms, Lightning, Star and Boo. They
share the weapon HUD square and roulette. Normal weapon cycling passes through
native weapons and returns to the retained MK64 item without consuming it.
Use MK64 Item is separate from attack (D-pad Down / X by default, remappable).
First use deploys orbiting triples or a banana bunch; later uses launch one.
Backward stick aims green shells behind; forward stick throws a banana ahead.

Lightning shrinks/slows riders without crashing everyone. Later contact with a
normal rider can knock down a shrunken rider. A brief per-rider bolt/dark flash
marks the hit. Mounted bike/rider/weapon roots share a physical pivot; detached
riders retain their own anchor. Star/Boo retain their protection rules. Falls
and manual ejection remain possible. Online host authority resolves rewards and
hits; all peers require protocol65. Highlights replay recorded effect state.

## Module guide

| Responsibility | Files | Contract |
| --- | --- | --- |
| Bounded state and deterministic simulation | rr64_mk64_item_state.hpp, rr64_mk64_item_kernel.* | No render/audio side effects; validated network snapshots. |
| Native physics and actor mapping | rr64_mk64_items.*, rr64_mk64_item_native.hpp | Canonical racer identity; restore temporary state; omit live effects in private prediction. |
| Native reward and weapon cycle | rr64_course_items.*, rr64_mk64_item_hud.* | Preserve both inventories; admit local cycle input before publication/history; suppress held edge until release. |
| World/HUD rendering | rr64_mk64_item_render.*, rr64_mk64_item_dimensions.hpp | Shared contact dimensions; native HUD rectangle; bounded per-view buffers. |
| Temporary materials | rr64_mk64_item_material.*, rr64_mk64_item_lightning.hpp | Scoped state restoration; visual timing from recorded effect deadlines. |
| Item audio | rr64_mk64_item_audio* | Bounded mixer; private prediction silent; music ducking preserves effects. |
| Imported assets | scripts/mk64_importer/item_assets.py, item_audio.py | Locally extracted user-ROM data; never distribute generated packs. |

All source files are under native/src unless a path says otherwise. Hook sites
are in config/roadrash64.us.toml; regenerate native output when those change.
Tests cover original native calls, input/HUD workers, snapshot validation,
materials, geometry, audio, imports and restarts. Offline results do not prove
GPU appearance, audible balance or Internet smoothness. See RELEASE_NOTES.md.

Original shell art remains a billboard, not a new 3D shell. Trap retirement
removes its model; it does not yet reproduce every donor flatten/break animation.
Audio retains the importer's resampling/reverb approximation. No new assets are
distributed. See MK64 importer notices and CREDITS.md for donor/tool provenance.
