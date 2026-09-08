# Weapon inherited-source candidate

Experimental local split-screen weapon correction; not visually verified.
The weapon pose is copied from the current rider model. Select that model's
camera source for the owned weapon root when its asset source differs.
Do not rescale the copied pose. Prior scale/path candidates failed user testing.

Rolling terrain remains reverted. Single-view rendering is unchanged.
Offline source-transition, viewport scope and CPU-context checks passed with
weapon diagnostics enabled and disabled. These do not establish visual success.

Test with two local players separated by the distance that reproduced floating
weapons. Attack with player one and watch player two's view, then swap roles.
Also check nearby attacks and NPC weapons. Launch only after the user says ready.
Nothing published.
