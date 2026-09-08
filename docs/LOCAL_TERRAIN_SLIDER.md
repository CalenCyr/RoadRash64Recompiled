# Local split-screen terrain slider

Settings > Graphics > Split-screen Terrain Distance: 0–12000, step 1000.
Default 0 preserves fallback terrain. Enable MAX World Distance before starting.
The slider itself applies live and persists. Start at 6000, compare the same
scene at lower/higher values, then test more players. No stable-FPS guarantee.

Applies to local 2–4-view terrain only. Single-view and online extension policy
remain unchanged. Extra roadside objects have not been enabled in split-screen.
Terrain admission uses nearest cell bounds to camera eye, with 25% retention
margin. This does not fade terrain or eliminate all pop-in. Zero removes extras;
original stock terrain continues. Original course-region safeguards remain.

The per-view weapon pose fix passed the user's two-player test and is retained.
Offline terrain smoke passed including zero, radius change, retention bounds,
per-view admission, source matrix parity and frame-allocation guards.
Live slider UI, performance and four-player validation pending.
Nothing published. Wait for ready before launching.
