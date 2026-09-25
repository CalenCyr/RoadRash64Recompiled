# Rebuilding imported race routes

`scripts/mk64_importer/route_assembly.py` reconstructs routes from the selected
MK64 ROM. `route_layout.json` contains authored atlas placements and scale
choices only; it contains no track points, terrain, lane widths, or reset masks.

The accepted conversion grew through scales 0.05, 0.12, 0.15, 0.1875, and
0.234375. Scaling once to the final size changes binary32 rounding and native
lap thresholds. The importer therefore fits the original route once and applies
the same successive transforms, preserving the native start/finish parameters,
14 starting positions, heights, and accumulated route distances.

Lane widths and safe recovery positions are regenerated with the native fresh
and cached floor-query helper. The first four stages rebuild their own terrain
cells from ROM geometry; they never read previously converted assets. The first
stage uses stock contact policy. Later stages enable the imported-course
adjacent-cell contact repair. The final scale retains those established recovery
exclusions rather than classifying scale-amplified height differences as holes.
These masks restrict recovery anchors, not normal driving or exploration.

Two historical conversion details are explicit: Mario Raceway's duplicate pipe
is included only in the first two temporary contact stages, and its authenticated
coarse camera alternatives are removed in stage four. Rainbow Road's established
23-unit contact margin at curve 125 is retained. The final installed terrain is
produced separately by the current terrain converter.

Koopa Troopa Beach retains its later verified starting-grid correction. The
original GP stagger is extended to 14 slots and projected onto source collision
floors after final scaling. Other courses retain their fitted route grids.
Camera-variant membership uses named ROM declaration boundaries; geometry
execution separately preserves native display-list fallthrough. This matters
for Yoshi Valley's BC0 declaration, which falls into the next camera at CC0.

## Pipeline API

1. Decode `source` and textures with `extract_course(donor, slug)`.
2. Compute `roots = root_membership(source, DisplayLists(...))`.
3. For Mario only, also decode `include_legacy_pipe=True`, retaining its texture
   files in the same temporary directory.
4. Create `historical_cell_provider(source, texture_dir, roots, legacy_source)`.
5. Pass that provider to `assemble_route(source, provider, helper, work_dir)`.

The result is the route metadata dictionary and native route bytes. The caller
owns temporary files and cancellation of the importer process tree. Native
queries run in a hidden, headless helper with bounded inputs; they do not launch
the game.

Offline C36 verification reproduced all 16 accepted route binaries, lane widths,
and recovery masks from the supplied ROM and newly generated contact cells.
Every intermediate route also matched its historical geometry and thresholds.
This establishes conversion parity; it does not substitute for live race tests.
