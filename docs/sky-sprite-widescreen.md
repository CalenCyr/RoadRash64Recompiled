# Current status: plain sky retained

The generated-cloud and continuous-sky prototypes below are historical findings.
The accepted implementation is `native/src/rr64_sky_sprites.cpp`: it removes only
cloud records from the shared sky producer. It retains native sky color and camera
state in every mode. The unused generated art/header/encoder were removed from the
active tree during release cleanup; recovery copies remain in the parent workspace
under `analysis/release-1.2-online/release-cleanup-20260919/retired/`.

---

# Sky-only widescreen correction

The original sky producer is `8005D100`, called by camera setup `8005D9A4`.
It selects a scrolling sequence of sky sprites and enqueues them through
`8001EB90`. These are deferred sprites, not immediate display-list writes.
The same queue later carries HUD sprites through `8001EF8C`.

`rr64_sky_sprites.cpp` records the queue span produced by the sky routine.
Both native allocation paths clear provenance when reusing a slot. At drawing,
only texture rectangles emitted for those records receive a private marker in
unused TEXRECT word1 bit31. No extra commands are inserted, and no guest sprite
fields, camera matrices, textures, viewport sizes, or HUD coordinates change.
The display list carries the marker into RT64; the rendering thread never needs
to inspect a newer frame's mutable guest sprite queue.

Live inspection found that sky enqueue and sprite drawing use different workers.
The initial thread-local provenance implementation failed and is not accepted.
Provenance is now published through atomic, RDRAM-owned entries per queue slot;
only the current producer/draw span remains thread-local. The regression fixture
explicitly enqueues on one thread and draws on another. Native triplets use
E4/E1/F1; raw opcode-like words in unrelated memory are not valid draw evidence.

The RT64 HLE decoder temporarily selects `G_EX_ASPECT_RR64_SKY` for a marked
rectangle and immediately restores the previous extended state. The framebuffer
renderer gives marked sky tiles a common horizontal stretch and no automatic HUD
anchor. This replaces the previous width/height/position sky heuristic, which
could transform neighboring tiles differently and miss lower split-screen skies.
Original scrolling, vertical position, per-view scissor and world occlusion remain.
This is a background correction, not a fixed image drawn over the race.

The final sky transform uses explicit sky provenance plus the output aspect,
not the live-race mode gate. That includes the opening attract demo. A private
demo inspection verified tagged sky rectangles and untagged Press Start glyphs.
This confirms activation, not absence of all seams or temporal flicker. The user
reported the initial build still broken; do not present that build as successful.

Validation must distinguish build/offline checks from visual acceptance. Test
4:3 and 16:9, camera turning, paused/racing HUD, and two/four-player layouts.
Compare original textures with replacement packs if authored seams remain; this
change cannot repair mismatched replacement texture artwork.

## September 19 follow-up: seams without flicker

The user confirmed flicker is gone but cloud joins remain visible. Targeted
reads of the tagged rectangles found contiguous screen edges and fractional
horizontal origins (for example 377 quarter-pixels). No replacement pack was
enabled in the inspected run. A diagnostic reconstruction of the referenced CI8
sky strips, using each strip's own palette, also shows coarse cloud joins; this
does not establish that every visible seam is a renderer error.

The next private correction preserves the fractional texture phase when sky
rectangles are rounded to raster pixels, and avoids intermediate 5-bit endpoint
quantization. It affects only explicitly tagged sky rectangles; other sprite UVs
and HUD layout are unchanged. Texture bounds use those same corrected endpoints.
Regression checks cover fractional origins, negative vertical derivatives and
two subdivisions of the same affine texture. Production build, sky fixture,
video-mode fixture, online viewport fixture and reverse dependency-patch check
passed. This is not yet visually accepted and must not be described as a complete
seam fix. Coarse artwork joins may still need a separate solution.

Prepared executable SHA256:
`ca877ec82dcae5d482e34f5f587dfe908205c19429400099f87854b8244de7b2`.
Private executable: `analysis/release-1.2-online/sky-review-20260919/`
relative to the workspace root. Previous executable preserved beside it.
No package was published and no launch followed this correction.

**Launch gate:** the user explicitly revoked unattended visual testing on
September 19. Keep the game closed until the user says **ready**.

## Generated panorama integration

The sampling-only candidate was rejected visually: the sky still did not align.
The user requested generated artwork based on the original's style and palette,
then explicitly requested integration. V2 in `native/assets/sky/` is now encoded
as one 640x100 panorama, sliced into sixteen 40x100 native sky sprites. One
palette covers every tile, with exact quantized wrap-edge equality. Native
32-row texture strips store rows bottom-up; reversing the whole image instead
would recreate horizontal seams. The encoder verifies every texel round-trips.

The queue producer validates stock IDs 0x2D..0x3C and 40x100 CI8 descriptors,
then substitutes owned, immutable render bindings. The original asset memory,
positions, scale, viewport/scissor selection, gameplay and HUD are unchanged.
Allocation failure or unknown dimensions retain stock artwork. The ~64KiB
allocation is created once, not every frame. Full texture addresses above 16MiB
are marked using unused SETTIMG bit18; the decoder enables extended addressing
only while consuming that command and restores the previous mode immediately.

Offline regression coverage includes all sixteen bindings, payload bytes,
original source/transform preservation, queue reuse, cross-thread provenance,
scoped texture-address markers, and guest addresses above 16MiB. An unsigned
offset/sign-extension fault caught by the fixture was corrected before staging.
Production build, sky/video/online viewport fixtures and reverse dependency
patch validation passed. The private executable was replaced with the integrated
build; the preceding executable is preserved as `before-generated-art`.
The first live test failed: the user supplied a screenshot with a black sky and
colored speckles. This is texture corruption, not an accepted artwork result.

## Generated artwork address-route correction

F3DEX2 maps SETTIMG to `GBI_F3D::setTextureImage`, bypassing the raw RDP command
handler previously updated for owned sky addresses. Without extended addressing,
RSP segment translation treats an address such as `0x81000400` as segment 1 plus
offset `0x400`, reading unrelated memory instead of the allocated panorama.
The marked sky command now forwards to the raw RDP handler before segment
translation. Unmarked HUD/world texture commands retain their existing path.
The RDP handler restores the previous extended-address mode after the load.

`scripts/test_sky_address_route.py` extracts and compiles the actual production
handlers and address conversions with minimal State bookkeeping. It verifies
the F3DEX2 dispatch mapping, every sky strip and palette address, both entry
paths, both initial addressing modes, and ordinary unmarked commands. Reverting
the forwarding branch only inside the offline fixture produces 65 failures;
the fixed version passes all 263 cases. This covers the missed decoder path,
but does not simulate GPU sampling or establish visual acceptance.

The production build, sky queue fixture and reverse dependency-patch check pass.
No new game was launched after this correction. Await the user's ready signal
for visual confirmation; the failed executable is preserved for comparison.

## Cloud coverage revision

The user's subsequent screenshot shows blue sky instead of corrupted black
pixels, but the user reports too few clouds. V2 concentrated cloud detail in
the upper third of the master. V3 distributes cloud banks throughout its height
using the same generated pixel-art style. The encoder now checks cloud coverage
in each vertical quarter as well as exact wrap-edge colors and strip packing.
All 64,000 texels round-trip and the wrap has zero differing rows. No geometry,
HUD, camera, gameplay or addressing changes accompany this artwork revision.
The running test remains untouched; this revision awaits a new ready signal.

## Continuous procedural sky prototype

The user rejected V3 as more distracting than the previous version, and asked
for a skybox-like approach instead of visible pixel texture. The private next
candidate evaluates a static, periodic atmosphere in the raster pixel shader.
It uses broad low-contrast cream cloud banks over a periwinkle gradient, without
sampling the generated CI8 pixel/palette data for marked sky draws.

This is **not a new cube-map skybox or one merged draw call**. Native rectangles
still provide sky coverage, camera scrolling, split-view scissor and ordering
behind terrain. The change is continuous shading across that coverage. Each
owned sky strip carries its panorama tile/row-end in unused RDPHALF_1 word0 bits.
Only complete E4/E1/F1 (or E5/E1/F1) triplets are annotated. The decoder scopes
metadata to one sky draw and restores it, and the RDP converts bottom-up strip
UVs to global panorama coordinates in floating-point vertex-color payloads.
Negative alpha marks that private payload; normal guest colors are nonnegative.
The raster shader substitutes atmosphere color only for marked rectangles,
leaving native blending/coverage and ordinary texture paths in place.

The earlier bitmap versions and executables remain available for comparison.
Tests cover all 64 tile/strip annotations, unchanged UV/derivative command words,
continuity at horizontal and vertical joins, and untagged HUD records. Both
DXIL and SPIR-V shader variants compile with the production build. These checks
do not establish visual quality, runtime activation, or performance. No launch
was performed; a new ready signal is required for the user's visual test.

### First continuous-sky live result: failed

The authorized test launched PID 404 at 2026-09-19 09:25:37 Pacific, SHA256
`692AE64681056D57F27E1CAF65787A6206D5B6C19D8462756B06ADE294C823C3`.
The user reported a freeze or crash. The process was absent when inspected.
No runtime crash report or matching queried Windows application/driver-reset
event was found. No exit code was retained by the launch helper, and stdout/
stderr were not redirected for this run, so the cause is unconfirmed. Do not
infer a shader fault merely because it followed the shader change.

The failed prototype remains in `pending-continuous-sky`. The default private
test executable was restored to the earlier V2 address-route build (DAF09076...),
which the user preferred visually to V3. Source still contains the experimental
continuous-sky work; it is not release-ready. No additional launch was made.
Any next authorized prototype test must capture stderr, runtime diagnostics,
and the process exit code, including deliberate user closure versus spontaneous
termination. Offline compilation did not validate live stability.

The user clarified that they closed the game after it froze. Classify this as
a reported hang followed by manual closure, not a confirmed crash. A gated
`analysis/release-1.2-online/Start-SkyDiagnostic.ps1` helper now captures runtime
diagnostics, stdout/stderr and exit status for the same prototype, but has not
been run. It requires a new ready signal; do not present it as a repaired build.

## Final direction: remove clouds in all game types

The next diagnostic run did not crash according to the user, but they rejected
the cloud layer and requested its removal in every game type. The shared sky
producer's begin/end hook now removes only cloud IDs 0x2D..0x3C queued within
that call. All camera/state calculations still run. Preexisting records and
non-cloud sprites are preserved, and the native background clear remains.
There is no game-type condition, so the same behavior applies to Big Game,
local multiplayer, online cameras, and attract mode.

All RT64 sky-experiment changes were restored to the pre-experiment dependency
patch. Generated artwork is no longer included by the active sky implementation,
and the allocation/draw hooks, panorama shader, custom addressing and aspect
markers are not in the active path. Experimental source and patch snapshots
are preserved in `analysis/release-1.2-online/retired-cloud-prototype`.
The sky queue test now verifies removal of all 16 IDs in both queue buffers,
preservation of preceding HUD and mixed non-cloud records, and safe handling
of reused/invalid spans. Visual confirmation of cloud removal remains pending.
