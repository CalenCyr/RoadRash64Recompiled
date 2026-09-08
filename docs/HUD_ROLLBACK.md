# HUD rollback

At user request, restored framebuffer renderer exactly from Video-Controls,
before the split-HUD localization and subsequent anchoring experiments.
Removed forced original Wide selection/dispatch and its diagnostic worker,
workload flags and generated hooks. Original video callback behavior restored.

This returns to the earlier presentation, including its known limitations in
second-player HUD expansion. It does not claim to fix those original limits.
Retains consolidated Draw Distance, aspect/FPS settings, shared original/custom
music volume, local player setup, accepted floating-weapon fix, and removal of
multiplayer forced catch-up relocation and its warning.

Build and video smoke checks passed. No game launched; runtime acceptance
pending. Nothing published. Historical candidates retained for comparison.
