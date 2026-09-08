# Course geometry diagnostic candidate

This candidate adds observations, not a new culling or performance fix. It retains
the preceding triangle-bookkeeping optimization and all current LOD/distance settings.

Set `RR64_COURSE_DIAGNOSTICS=1` before startup to capture terrain observations.
The verified launcher also sets `RR64_DIAGNOSTICS=1` for the existing periodic log.
The separate course switch allows timing-only comparisons. Detailed renderer timing
has its own overhead, which has not yet been measured in an equivalent live scene.

The existing health logger writes `[RR64-COURSE]` snapshots roughly every ten seconds.
There is no added per-cell file I/O, guest-state mutation, or per-frame allocation.
Capture uses the terrain cache's existing mutex. Each valid terrain pass records:

- View index, authored epoch and terrain camera XY origin.
- Stock cell count, additional drawn cells/triangles and connected-island exclusions.
- Union of observed stock cells, AFTER the existing island filter.
- Exact last-pass additional-cell bitmap, independent for each of four screens.
- Raw main/pending modes and the 17 engine setup words, in the address order in
  `rr64_engine_layout.hpp::globals::multiplayer_game_setup_words`.

The raw words have NOT been decoded into a reliable map identifier. Record the
map name and player count manually for the test. A cell absent from the stock union
is not proven unrelated to the course. Connected islands are not course boundaries.

Each bitmap contains 77 comma-separated 64-bit hexadecimal words. Word k, bit b
is grid cell k*64+b; row=cell/70 and column=cell%70. The final 28 unused bits are zero.
The stock union resets on a raw setup change or runtime heap/session reset; it is
not a certified race-boundary recorder. View epochs expose stale last-pass samples.
The logger samples last-pass geometry, not every intermediate frame. Unchanged raw
setup can span repeated races, and a changing opaque word can reset the union.

Offline terrain tests pass enabled and disabled, including four-view isolation,
empty-pass clearing, setup resets, draw command invariants and buffer lifetime.
Live capture and visual acceptance remain pending. No publication is authorized.

For the next run: use the same local split-screen course and player count, keep MAX
LOD/world distance enabled, spend at least 15 seconds facing the slow direction,
then at least 15 seconds facing the faster direction. Note the course and sequence,
drive through the area, and close normally. Launch only after the user says ready.
