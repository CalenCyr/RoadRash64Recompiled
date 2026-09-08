# Multiplayer terrain fallback

Requested fallback: two-to-four-view rendering uses original terrain and distant
scenery submissions, as in the accepted Split-Screen-LOD-Correction baseline.
No extra cached world draws are injected for these layouts. Single-view world
extensions, rider/bike MAX LOD, controller setup and camera ownership are retained.
The gate follows the engine view count, not player count or network membership.

Offline checks pass for all 2/3/4-view slots with unchanged display-list pointers,
single-view terrain/scenery rendering, and actor render snapshots. Live acceptance
is pending. Diagnostics remain opt-in via the verified launcher. No game launched
or publication. Previous candidates are preserved.

Online race layout selects one fullscreen local view per peer. This avoids
split-screen multiplication of terrain submissions, but a heavy single view can
still exceed frame budget. Online performance is not established by these tests.
