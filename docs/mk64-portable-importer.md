# Local MK64 course conversion

The packaged importer builds the optional course mod from the player's supported
USA MK64 ROM and checks it against the player's supported USA Road Rash 64 ROM.
The converter does not download assets and does not include either game's ROM,
extracted meshes, textures, animation streams, sound banks, or music.

`scripts/rr64_mk64_importer.py` is the source entry point. The release bundles it
with its Python and NumPy runtime and two small native helpers, so players do
not need Python, a compiler, a source checkout, or a separately installed tool.
All three normal N64 ROM byte orders are accepted after normalization and full
image identity verification.

## Data ownership

The game owns selection, progress display, cancellation, and installation. It
creates an isolated job directory and passes ROM paths and staging paths to the
converter. The converter writes generated files under the provided staging
directory, progress to the supplied progress file, and a final result next to
the staged `pack` directory. It never modifies the selected ROMs or installs a
mod itself.

The native worker independently checks the complete catalogue and file digests
before replacing an installed pack. A failed or cancelled conversion therefore
cannot become the active mod. The importer uses no network services.

## Conversion stages

- `source_rom`, `source_displaylists`, and `extract_courses` read the cartridge
  layout and execute the supported display-list state transitions.
- `baseline`, `terrain`, and the shared `race_pack_*` helpers apply the verified
  surface corrections, finite exploration contacts, and rail adjustments.
- `route_assembly` derives routes and asks the bundled contact helper to verify
  lane widths and recovery support against regenerated native collision cells.
- `assets`, `actor_*`, and `actors` derive previews, skies, items, moving objects,
  models, animation poses, and actor paths. The motion helper preserves the
  established native floating-point path arithmetic.
- `audio` and `music` decode source sequence and instrument data into the
  existing sound and music bank formats. Host resampling and reverb remain the
  documented approximations used by the current course mod.
- `pipeline` writes the complete deterministic catalogue and verifies its files.

For contributor changes, start at `cli.main`, then `pipeline.convert`. The
`prepare_courses` stage retains the original decoded source for actor placement
while producing corrected terrain separately. `PackWriter` owns generated file
records and checks their staged bytes before writing the final catalogue.
Temporary textures and helper inputs belong in the sibling work directory.

Adjacent layout JSON files contain address, size, format, identity, and authored
conversion recipes. Actual source geometry, pixels, poses, paths, and samples
are read or computed from the supplied ROM during conversion. No development
`analysis` directory or previously extracted pack is a runtime dependency.

## Compatibility decisions

Stable texture IDs and the established sequence of atlas scales are retained so
the regenerated terrain, actor models, items, routes, and collision agree with
the tested course pack. Six water materials retain the conversion's established
static animation phase. Rainbow Road's enclosing sky retains the original
authored upper bound even though its guardrails are shortened for gameplay.

The geometry decoder follows native display-list fallthrough. Camera variant
classification separately respects named declaration boundaries; those two
views serve different purposes and must not be merged. This distinction is
required for Yoshi Valley's mutually exclusive cliff surface variants.

Build the packaged tool with `scripts/build_mk64_importer.py`. A recipe or
conversion change must update the converter version and repeat the bounded
ROM-to-pack checks and packaged-tool comparison. Reusing historical proof
hashes after modifying the converter is insufficient.

The converter version describes generated-pack compatibility, independently of
the game's release version. Readability-only cleanup can retain it when a fresh
full conversion proves that every file, including the catalogue, is unchanged.
Do not replace the historical scale steps with one multiplication or change
floating-point evaluation order during such cleanup: rounded native coordinates
and stored metadata are part of the deterministic output contract.

The Python modules use ordinary multiline control flow and a 100-column format.
Formatting tools are development-only and are not part of the shipped importer.
Small comments explain compatibility constraints; the adjacent recipes retain
their existing identities and ordering.

Generated packs are derived from the player's ROM. The importer does not grant
redistribution rights to those assets. See `scripts/MK64-IMPORTER-NOTICES.md` for
source-reference and bundled-runtime notices.
