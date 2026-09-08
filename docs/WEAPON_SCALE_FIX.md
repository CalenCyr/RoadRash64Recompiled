# Weapon scale correction candidate

Rolling terrain remains reverted. Candidate visual acceptance pending.
Original func_80011CC0 calls func_8000F9E8 for the rider's graph at +0x5BC
when +0x5B4/+0x5B6 permit it. This happens after the LOD pose transaction ends.
That separate renderer selects its own camera source at 0x8000FA8C and packs its
root independently. Existing rider root normalization does not cover this path.

Scope only the verified rider-to-weapon call, require 2–4 views and MAX LOD,
validate graph ownership and active view, and normalize source-1 absolute roots
from scale100 to scale10 while selecting source2 with original func_80016A18.
Keep local children and homogeneous matrix components intact. Clear scope on
return. The helper's entire CPU/FPU register context is restored. Asset data and
simulation state are not rewritten. Other callers of func_8000F9E8 stay unchanged.

Synthetic scope/scale tests pass for 1–4 views, wrong graph, changed camera,
disabled LOD, register restoration, and single-application conversion. Existing
actor runtime regression tests pass. Recompilation from config and native build
pass. This establishes candidate mechanics, not the live artifact's disappearance.
Earlier near-limit logs were pre-normalization rider/bike roots, not confirmed
weapon overflow. Weapon-path identification comes from the original caller graph.

No launch or publication. Compare two local players far apart, repeated attacks,
then nearby combat and NPC attacks. Keep the prior diagnostic candidate available.
