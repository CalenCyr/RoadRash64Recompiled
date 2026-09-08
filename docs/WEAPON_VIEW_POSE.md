# Per-view weapon pose candidate

The diagnostic session 20260907-213216-730 completed normally and captured
weapon roots retaining the previous view's translation. Example view 1 rider
(15238.583,-2312.451,909.644), weapon (-408.418,13.281,14.125): the latter
matches view 0. Packed values equal float values in the captured samples.

For an owned weapon in a MAX LOD split-screen draw, temporarily copy the
current rider root's seven pose words (translation and quaternion), matching
the original pose producer's copy. Restore the exact weapon root after draw.
Weapon child animation and simulation state are retained. Rolling terrain is
still reverted; single-view rendering is unchanged.

Offline tests pass with diagnostics enabled/disabled, including the captured
stale-position case, exact restoration, interrupted scope and one-view exclusion.
Visual acceptance pending. Reproduce with two players, near and far attacks,
both player roles and NPCs. Diagnostics remain enabled for comparison.
Nothing published. Launch only after ready.
