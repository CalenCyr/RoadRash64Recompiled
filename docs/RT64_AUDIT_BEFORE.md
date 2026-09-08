# RT64 patch audit — pre-change assessment

Date: 2026-09-07. Local cleanup only; no publication authorized.

Baseline: the published patch and the working RT64 tracked diff are byte-identical: **5,359 lines, 282,934 bytes, 46 files**. Both patch copies and all affected working files are preserved under `analysis/rt64-cleanup` and `handoff/rt64-cleanup-before`. This audit compares against the pinned dependency revision, not an untested newer upstream release.

## Decision before editing

1. Remove the obsolete screen-space horizon blur and its depth-target association machinery. `threadPresent` explicitly sets its enable flag to false; no other caller enables it. Despite that, State scans projections and writes the association map, PresentQueue locks/looks it up, and VIRenderer binds a depth descriptor and writes unused constants. The 25-tap blur itself was **not executing**, so its removal is not a 25-sample-per-pixel runtime saving. Keep the active RSP vertex fog/haze option.
2. Remove RT64 startup stage tracing and whitespace-only patch hunks. Preserve the max-frame-latency change in Application and normal upstream error handling.
3. Make optional pipeline Scope clocks, authored observation hashes, and periodic resource inventory conditional on the existing exact `RR64_DIAGNOSTICS=1` setting. `rr64_record_authored_sample` only updates observation counters/logs; it never authorizes a render. Do not gate actual cadence measurement or geometry matching certificates.
4. Retain the other functional changes. Current call paths establish usage, but do not establish that every algorithm is minimal. Their removability is **uncertain without controlled runtime comparisons**, so removal is not justified. In particular, retain pacing/ownership, matching proof, native fallback, terrain batching, widescreen/HUD fixes, TMEM caches and optional texture pack support.

## Evidence and risks

The old blur's producer is `State`'s presentDepthTargetKeys assignment; its only consumer is PresentQueue's horizonDepthSource lookup. Both are disconnected from the active RSP userExtendedHorizonHaze flag. VI shader bindings and shared constants must be reverted together and both shader backends rebuilt.

Timing probes currently call steady_clock twice before their callback rejects disabled diagnostics. Resource diagnostics periodically scan targets and lock the texture/batch caches even with no report requested. Gating these preparations reduces identifiable work, but no FPS or stutter improvement is claimed without live before/after measurements.

Geometry-certificate construction, texture replacement hashing, presentation atomics and existing profilers remain possible CPU costs. Their current contracts differ: correctness certificates must not be removed as logging; optional texture decoding is required with the user's mod; further profiling cleanup should be measured separately. Frame pacing and high thread priority remain hardware-dependent and require live validation.

## Validation plan

Build after each logical cleanup group, exercise existing renderer/LOD/world/pacing tests, test diagnostics enabled and disabled, regenerate the candidate patch and apply it to a clean pinned RT64 tree. Keep the public release immutable. Create a separate local candidate. Do not launch the game until the user says **ready**. Live checks should cover Big Game with maximum distance, racers/crashes/finish line, paused/menu rendering, widescreen/high-resolution, texture mod, and split-screen lap maps. Contributor refactoring beyond this narrow cleanup is deferred until runtime stability is confirmed.

## Complete original-hunk inventory

Classification below applies to functional contents; end-of-file-only additions in otherwise active files are redundant. Mixed hunks identify the removable subset explicitly. “Retain” means an active code path or supported tool uses it, not a claim of new runtime verification.

### 01. `CMakeLists.txt`

Builds the Jabo texture hasher and MD5 implementation used by replacement textures.

- Hunk 1: `@@ -313,6 +313,7 @@ set (SOURCES` — retain active implementation/declaration; safe removability not established.
- Hunk 2: `@@ -412,6 +413,7 @@ set (SOURCES` — retain active implementation/declaration; safe removability not established.

### 02. `include/rt64_extended_gbi.h`

Registers the packed indexed-triangle command consumed by the terrain batching path.

- Hunk 1: `@@ -80,7 +80,8 @@` — retain active implementation/declaration; safe removability not established.

### 03. `src/common/rt64_replacement_database.cpp`

Jabo replacement lookup, serialization and path resolution; the texture packer also uses the hash getter.

- Hunk 1: `@@ -58,6 +58,15 @@ namespace RT64 {` — retain active implementation/declaration; safe removability not established.
- Hunk 2: `@@ -97,8 +106,19 @@ namespace RT64 {` — retain active implementation/declaration; safe removability not established.
- Hunk 3: `@@ -106,6 +126,10 @@ namespace RT64 {` — retain active implementation/declaration; safe removability not established.
- Hunk 4: `@@ -145,7 +169,7 @@ namespace RT64 {` — retain active implementation/declaration; safe removability not established.
- Hunk 5: `@@ -192,13 +216,35 @@ namespace RT64 {` — retain active implementation/declaration; safe removability not established.
- Hunk 6: `@@ -208,7 +254,12 @@ namespace RT64 {` — retain active implementation/declaration; safe removability not established.
- Hunk 7: `@@ -217,7 +268,7 @@ namespace RT64 {` — retain active implementation/declaration; safe removability not established.
- Hunk 8: `@@ -331,6 +382,9 @@ namespace RT64 {` — retain active implementation/declaration; safe removability not established.
- Hunk 9: `@@ -341,6 +395,7 @@ namespace RT64 {` — retain active implementation/declaration; safe removability not established.
- Hunk 10: `@@ -407,4 +462,4 @@ namespace RT64 {` — retain active implementation/declaration; safe removability not established.

### 04. `src/common/rt64_replacement_database.h`

Declarations and storage for the same Jabo replacement database contract.

- Hunk 1: `@@ -66,6 +66,11 @@ namespace RT64 {` — retain active implementation/declaration; safe removability not established.
- Hunk 2: `@@ -75,7 +80,7 @@ namespace RT64 {` — retain active implementation/declaration; safe removability not established.
- Hunk 3: `@@ -118,15 +123,17 @@ namespace RT64 {` — retain active implementation/declaration; safe removability not established.
- Hunk 4: `@@ -150,4 +157,4 @@ namespace RT64 {` — retain active implementation/declaration; safe removability not established.

### 05. `src/common/rt64_timer.cpp`

Windows deadline wait implementation used by presentation pacing; hardware timing remains a live-test concern.

- Hunk 1: `@@ -2,12 +2,40 @@` — retain active implementation/declaration; safe removability not established.
- Hunk 2: `@@ -22,53 +50,62 @@ namespace RT64 {` — retain active implementation/declaration; safe removability not established.

### 06. `src/common/rt64_tmem_hasher.h`

Equivalent TMEM palette/hash fast paths; cache identity must remain exact.

- Hunk 1: `@@ -4,6 +4,8 @@` — retain active implementation/declaration; safe removability not established.
- Hunk 2: `@@ -72,10 +74,29 @@ namespace RT64 {` — retain active implementation/declaration; safe removability not established.
- Hunk 3: `@@ -206,4 +227,4 @@ namespace RT64 {` — retain active implementation/declaration; safe removability not established.

### 07. `src/common/rt64_user_configuration.h`

Stretch configuration and combined widescreen/high-resolution mode, populated by the frontend render context.

- Hunk 1: `@@ -52,6 +52,7 @@ namespace RT64 {` — retain active implementation/declaration; safe removability not established.
- Hunk 2: `@@ -84,6 +85,10 @@ namespace RT64 {` — retain active implementation/declaration; safe removability not established.
- Hunk 3: `@@ -130,7 +135,8 @@ namespace RT64 {` — retain active implementation/declaration; safe removability not established.
- Hunk 4: `@@ -174,4 +180,4 @@ namespace RT64 {` — retain active implementation/declaration; safe removability not established.

### 08. `src/gbi/rt64_gbi.cpp`

Unused cstdio include and end-of-file whitespace only: redundant.

- Hunk 1: `@@ -6,6 +6,7 @@` — redundant; restore pinned original.
- Hunk 2: `@@ -558,4 +559,4 @@ namespace RT64 {` — redundant; restore pinned original.

### 09. `src/gbi/rt64_gbi_extended.cpp`

Dispatch and validation for the packed terrain triangle command.

- Hunk 1: `@@ -326,6 +326,35 @@ namespace RT64 {` — retain active implementation/declaration; safe removability not established.
- Hunk 2: `@@ -439,6 +468,7 @@ namespace RT64 {` — retain active implementation/declaration; safe removability not established.

### 10. `src/gbi/rt64_gbi_s2dex.cpp`

Whitespace only: redundant.

- Hunk 1: `@@ -412,6 +412,7 @@ namespace RT64 {` — redundant; restore pinned original.
- Hunk 2: `@@ -661,4 +662,4 @@ namespace RT64 {` — redundant; restore pinned original.

### 11. `src/hle/rt64_application.cpp`

Retain maxFrameLatency=2 used with deadline pacing. All other changes are startup debug tracing or its formatting: experimental leftovers.

- Hunk 1: `@@ -3,6 +3,8 @@` — mixed: retain active contract; remove/gate only the named subset above where present.
- Hunk 2: `@@ -64,8 +66,18 @@ namespace RT64 {` — mixed: retain active contract; remove/gate only the named subset above where present.
- Hunk 3: `@@ -78,7 +90,10 @@ namespace RT64 {` — mixed: retain active contract; remove/gate only the named subset above where present.
- Hunk 4: `@@ -87,6 +102,9 @@ namespace RT64 {` — mixed: retain active contract; remove/gate only the named subset above where present.
- Hunk 5: `@@ -100,7 +118,9 @@ namespace RT64 {` — mixed: retain active contract; remove/gate only the named subset above where present.
- Hunk 6: `@@ -118,7 +138,9 @@ namespace RT64 {` — mixed: retain active contract; remove/gate only the named subset above where present.
- Hunk 7: `@@ -128,9 +150,14 @@ namespace RT64 {` — mixed: retain active contract; remove/gate only the named subset above where present.
- Hunk 8: `@@ -159,7 +186,10 @@ namespace RT64 {` — mixed: retain active contract; remove/gate only the named subset above where present.
- Hunk 9: `@@ -174,7 +204,10 @@ namespace RT64 {` — mixed: retain active contract; remove/gate only the named subset above where present.
- Hunk 10: `@@ -183,7 +216,10 @@ namespace RT64 {` — mixed: retain active contract; remove/gate only the named subset above where present.
- Hunk 11: `@@ -198,7 +234,11 @@ namespace RT64 {` — mixed: retain active contract; remove/gate only the named subset above where present.
- Hunk 12: `@@ -262,7 +302,10 @@ namespace RT64 {` — mixed: retain active contract; remove/gate only the named subset above where present.
- Hunk 13: `@@ -270,7 +313,10 @@ namespace RT64 {` — mixed: retain active contract; remove/gate only the named subset above where present.
- Hunk 14: `@@ -295,13 +341,21 @@ namespace RT64 {` — mixed: retain active contract; remove/gate only the named subset above where present.
- Hunk 15: `@@ -327,11 +381,17 @@ namespace RT64 {` — mixed: retain active contract; remove/gate only the named subset above where present.
- Hunk 16: `@@ -343,9 +403,13 @@ namespace RT64 {` — mixed: retain active contract; remove/gate only the named subset above where present.
- Hunk 17: `@@ -538,6 +602,8 @@ namespace RT64 {` — mixed: retain active contract; remove/gate only the named subset above where present.

### 12. `src/hle/rt64_game_frame.cpp`

Geometry certificates, scene membership, transform correspondence and ambiguity rejection are consumed by interpolation admission. Keep hashes used for correctness; these are not diagnostic hashes.

- Hunk 1: `@@ -6,10 +6,347 @@` — retain active implementation/declaration; safe removability not established.
- Hunk 2: `@@ -77,6 +414,8 @@ namespace RT64 {` — retain active implementation/declaration; safe removability not established.
- Hunk 3: `@@ -256,6 +595,35 @@ namespace RT64 {` — retain active implementation/declaration; safe removability not established.
- Hunk 4: `@@ -271,7 +639,7 @@ namespace RT64 {` — retain active implementation/declaration; safe removability not established.
- Hunk 5: `@@ -288,6 +656,13 @@ namespace RT64 {` — retain active implementation/declaration; safe removability not established.
- Hunk 6: `@@ -331,10 +706,26 @@ namespace RT64 {` — retain active implementation/declaration; safe removability not established.
- Hunk 7: `@@ -342,13 +733,16 @@ namespace RT64 {` — retain active implementation/declaration; safe removability not established.
- Hunk 8: `@@ -366,14 +760,34 @@ namespace RT64 {` — retain active implementation/declaration; safe removability not established.
- Hunk 9: `@@ -398,7 +812,7 @@ namespace RT64 {` — retain active implementation/declaration; safe removability not established.
- Hunk 10: `@@ -516,6 +930,12 @@ namespace RT64 {` — retain active implementation/declaration; safe removability not established.
- Hunk 11: `@@ -533,7 +953,19 @@ namespace RT64 {` — retain active implementation/declaration; safe removability not established.
- Hunk 12: `@@ -542,6 +974,11 @@ namespace RT64 {` — retain active implementation/declaration; safe removability not established.
- Hunk 13: `@@ -588,22 +1025,79 @@ namespace RT64 {` — retain active implementation/declaration; safe removability not established.
- Hunk 14: `@@ -1039,4 +1533,4 @@ namespace RT64 {` — retain active implementation/declaration; safe removability not established.

### 13. `src/hle/rt64_game_frame.h`

Storage and declarations for those frame-matching certificates.

- Hunk 1: `@@ -13,6 +13,7 @@` — retain active implementation/declaration; safe removability not established.
- Hunk 2: `@@ -121,16 +122,22 @@ namespace RT64 {` — retain active implementation/declaration; safe removability not established.

### 14. `src/hle/rt64_interpreter.cpp`

Display-list timing scope: optional diagnostics; currently clocks even when output is disabled.

- Hunk 1: `@@ -3,8 +3,10 @@` — optional diagnostic path; retain with disabled-mode timing bypass.
- Hunk 2: `@@ -154,6 +156,7 @@ namespace RT64 {` — optional diagnostic path; retain with disabled-mode timing bypass.

### 15. `src/hle/rt64_present.h`

Per-present scene, authored target and source cadence metadata: protects target identity across the queues.

- Hunk 1: `@@ -8,6 +8,8 @@` — retain active implementation/declaration; safe removability not established.
- Hunk 2: `@@ -23,5 +25,8 @@ namespace RT64 {` — retain active implementation/declaration; safe removability not established.

### 16. `src/hle/rt64_present_queue.cpp`

Retain metadata admission, owned/repeated images, queue wakeups and single-owner deadline pacing. Remove only horizon depth lookup and disabled blur preparation; diagnostic timing remains available.

- Hunk 1: `@@ -4,11 +4,51 @@` — mixed: retain active contract; remove/gate only the named subset above where present.
- Hunk 2: `@@ -33,21 +73,22 @@ namespace RT64 {` — mixed: retain active contract; remove/gate only the named subset above where present.
- Hunk 3: `@@ -93,6 +134,8 @@ namespace RT64 {` — mixed: retain active contract; remove/gate only the named subset above where present.
- Hunk 4: `@@ -101,6 +144,7 @@ namespace RT64 {` — mixed: retain active contract; remove/gate only the named subset above where present.
- Hunk 5: `@@ -108,14 +152,23 @@ namespace RT64 {` — mixed: retain active contract; remove/gate only the named subset above where present.
- Hunk 6: `@@ -186,17 +239,114 @@ namespace RT64 {` — mixed: retain active contract; remove/gate only the named subset above where present.
- Hunk 7: `@@ -256,6 +406,16 @@ namespace RT64 {` — mixed: retain active contract; remove/gate only the named subset above where present.
- Hunk 8: `@@ -266,9 +426,15 @@ namespace RT64 {` — mixed: retain active contract; remove/gate only the named subset above where present.
- Hunk 9: `@@ -276,24 +442,35 @@ namespace RT64 {` — mixed: retain active contract; remove/gate only the named subset above where present.
- Hunk 10: `@@ -310,6 +487,38 @@ namespace RT64 {` — mixed: retain active contract; remove/gate only the named subset above where present.
- Hunk 11: `@@ -323,6 +532,13 @@ namespace RT64 {` — mixed: retain active contract; remove/gate only the named subset above where present.
- Hunk 12: `@@ -338,6 +554,33 @@ namespace RT64 {` — mixed: retain active contract; remove/gate only the named subset above where present.
- Hunk 13: `@@ -345,6 +588,13 @@ namespace RT64 {` — mixed: retain active contract; remove/gate only the named subset above where present.
- Hunk 14: `@@ -389,18 +639,99 @@ namespace RT64 {` — mixed: retain active contract; remove/gate only the named subset above where present.
- Hunk 15: `@@ -426,12 +757,19 @@ namespace RT64 {` — mixed: retain active contract; remove/gate only the named subset above where present.
- Hunk 16: `@@ -467,6 +805,7 @@ namespace RT64 {` — mixed: retain active contract; remove/gate only the named subset above where present.
- Hunk 17: `@@ -504,13 +843,22 @@ namespace RT64 {` — mixed: retain active contract; remove/gate only the named subset above where present.

### 17. `src/hle/rt64_present_queue.h`

Keep pacing/repeated-image members. Remove unused horizonDepthColorTarget.

- Hunk 1: `@@ -10,6 +10,7 @@` — mixed: retain active contract; remove/gate only the named subset above where present.
- Hunk 2: `@@ -52,9 +53,13 @@ namespace RT64 {` — mixed: retain active contract; remove/gate only the named subset above where present.
- Hunk 3: `@@ -73,4 +78,4 @@ namespace RT64 {` — mixed: retain active contract; remove/gate only the named subset above where present.

### 18. `src/hle/rt64_rdp.cpp`

TMEM loads preserve byte order/alignment/fallbacks; maximum primitive depth correction prevents strict LESS rejection.

- Hunk 1: `@@ -5,6 +5,12 @@` — retain active implementation/declaration; safe removability not established.
- Hunk 2: `@@ -367,6 +373,45 @@ namespace RT64 {` — retain active implementation/declaration; safe removability not established.
- Hunk 3: `@@ -461,6 +506,51 @@ namespace RT64 {` — retain active implementation/declaration; safe removability not established.
- Hunk 4: `@@ -962,7 +1052,13 @@ namespace RT64 {` — retain active implementation/declaration; safe removability not established.

### 19. `src/hle/rt64_rdp_tmem.cpp`

TMEM hash/load cache invalidation and reuse; active State tile processing.

- Hunk 1: `@@ -51,7 +51,9 @@ namespace RT64 {` — retain active implementation/declaration; safe removability not established.
- Hunk 2: `@@ -194,4 +196,4 @@ namespace RT64 {` — retain active implementation/declaration; safe removability not established.

### 20. `src/hle/rt64_rdp_tmem.h`

Storage for those live TMEM caches.

- Hunk 1: `@@ -8,11 +8,15 @@` — retain active implementation/declaration; safe removability not established.
- Hunk 2: `@@ -22,4 +26,4 @@ namespace RT64 {` — retain active implementation/declaration; safe removability not established.

### 21. `src/hle/rt64_rsp.cpp`

Packed triangles and vertex stream batching; active user vertex fog/haze controls. The vertex haze is distinct from the disabled VI blur and must remain.

- Hunk 1: `@@ -4,7 +4,9 @@` — retain active implementation/declaration; safe removability not established.
- Hunk 2: `@@ -18,6 +20,27 @@` — retain active implementation/declaration; safe removability not established.
- Hunk 3: `@@ -622,7 +645,12 @@ namespace RT64 {` — retain active implementation/declaration; safe removability not established.
- Hunk 4: `@@ -676,42 +704,71 @@ namespace RT64 {` — retain active implementation/declaration; safe removability not established.
- Hunk 5: `@@ -719,8 +776,8 @@ namespace RT64 {` — retain active implementation/declaration; safe removability not established.
- Hunk 6: `@@ -728,8 +785,8 @@ namespace RT64 {` — retain active implementation/declaration; safe removability not established.
- Hunk 7: `@@ -988,8 +1045,24 @@ namespace RT64 {` — retain active implementation/declaration; safe removability not established.
- Hunk 8: `@@ -1166,6 +1239,121 @@ namespace RT64 {` — retain active implementation/declaration; safe removability not established.
- Hunk 9: `@@ -1310,3 +1498,4 @@ namespace RT64 {` — retain active implementation/declaration; safe removability not established.

### 22. `src/hle/rt64_rsp.h`

Declarations for active triangle batching and fog controls.

- Hunk 1: `@@ -32,6 +32,17 @@ namespace RT64 {` — retain active implementation/declaration; safe removability not established.
- Hunk 2: `@@ -293,6 +304,7 @@ namespace RT64 {` — retain active implementation/declaration; safe removability not established.
- Hunk 3: `@@ -304,4 +316,4 @@ namespace RT64 {` — retain active implementation/declaration; safe removability not established.

### 23. `src/hle/rt64_shared_queue_resources.h`

Retain owned-batch cache, resource epochs and presented-target history. Remove depth association map/mutex used only by the disabled blur.

- Hunk 1: `@@ -5,6 +5,8 @@` — mixed: retain active contract; remove/gate only the named subset above where present.
- Hunk 2: `@@ -12,6 +14,9 @@` — mixed: retain active contract; remove/gate only the named subset above where present.
- Hunk 3: `@@ -19,6 +24,7 @@` — mixed: retain active contract; remove/gate only the named subset above where present.
- Hunk 4: `@@ -47,6 +53,12 @@ namespace RT64 {` — mixed: retain active contract; remove/gate only the named subset above where present.
- Hunk 5: `@@ -57,6 +69,50 @@ namespace RT64 {` — mixed: retain active contract; remove/gate only the named subset above where present.
- Hunk 6: `@@ -91,10 +147,17 @@ namespace RT64 {` — mixed: retain active contract; remove/gate only the named subset above where present.
- Hunk 7: `@@ -107,4 +170,4 @@ namespace RT64 {` — mixed: retain active contract; remove/gate only the named subset above where present.

### 24. `src/hle/rt64_state.cpp`

Retain scene/cadence payload, tile cache, race HUD state and pipeline probes. Remove depth association production. Gate only observation hashes sent to rr64_record_authored_sample, not hashes authorizing interpolation.

- Hunk 1: `@@ -3,9 +3,13 @@` — mixed: retain active contract; remove/gate only the named subset above where present.
- Hunk 2: `@@ -27,6 +31,20 @@` — mixed: retain active contract; remove/gate only the named subset above where present.
- Hunk 3: `@@ -80,6 +98,7 @@ namespace RT64 {` — mixed: retain active contract; remove/gate only the named subset above where present.
- Hunk 4: `@@ -576,6 +595,37 @@ namespace RT64 {` — mixed: retain active contract; remove/gate only the named subset above where present.
- Hunk 5: `@@ -642,23 +692,15 @@ namespace RT64 {` — mixed: retain active contract; remove/gate only the named subset above where present.
- Hunk 6: `@@ -790,6 +832,12 @@ namespace RT64 {` — mixed: retain active contract; remove/gate only the named subset above where present.
- Hunk 7: `@@ -940,6 +988,7 @@ namespace RT64 {` — mixed: retain active contract; remove/gate only the named subset above where present.
- Hunk 8: `@@ -1121,6 +1170,8 @@ namespace RT64 {` — mixed: retain active contract; remove/gate only the named subset above where present.
- Hunk 9: `@@ -1133,6 +1184,7 @@ namespace RT64 {` — mixed: retain active contract; remove/gate only the named subset above where present.
- Hunk 10: `@@ -1469,9 +1521,13 @@ namespace RT64 {` — mixed: retain active contract; remove/gate only the named subset above where present.
- Hunk 11: `@@ -1578,10 +1634,15 @@ namespace RT64 {` — mixed: retain active contract; remove/gate only the named subset above where present.
- Hunk 12: `@@ -1611,13 +1672,55 @@ namespace RT64 {` — mixed: retain active contract; remove/gate only the named subset above where present.
- Hunk 13: `@@ -1775,8 +1878,10 @@ namespace RT64 {` — mixed: retain active contract; remove/gate only the named subset above where present.
- Hunk 14: `@@ -2065,11 +2170,12 @@ namespace RT64 {` — mixed: retain active contract; remove/gate only the named subset above where present.
- Hunk 15: `@@ -2140,13 +2246,13 @@ namespace RT64 {` — mixed: retain active contract; remove/gate only the named subset above where present.
- Hunk 16: `@@ -2654,8 +2760,74 @@ namespace RT64 {` — mixed: retain active contract; remove/gate only the named subset above where present.
- Hunk 17: `@@ -2665,6 +2837,30 @@ namespace RT64 {` — mixed: retain active contract; remove/gate only the named subset above where present.
- Hunk 18: `@@ -2801,4 +2997,5 @@ namespace RT64 {` — mixed: retain active contract; remove/gate only the named subset above where present.

### 25. `src/hle/rt64_state.h`

Scene/cadence tracking state required by workload submission.

- Hunk 1: `@@ -118,6 +118,10 @@ namespace RT64 {` — retain active implementation/declaration; safe removability not established.
- Hunk 2: `@@ -152,6 +156,7 @@ namespace RT64 {` — retain active implementation/declaration; safe removability not established.
- Hunk 3: `@@ -174,4 +179,4 @@ namespace RT64 {` — retain active implementation/declaration; safe removability not established.

### 26. `src/hle/rt64_vi.cpp`

Strict logical VI source cadence fallback, used outside coordinated authored race timing.

- Hunk 1: `@@ -161,17 +161,24 @@ namespace RT64 {` — retain active implementation/declaration; safe removability not established.

### 27. `src/hle/rt64_vi.h`

Declarations for strict cadence/source factor.

- Hunk 1: `@@ -10,6 +10,8 @@` — retain active implementation/declaration; safe removability not established.
- Hunk 2: `@@ -167,7 +169,8 @@ namespace RT64 {` — retain active implementation/declaration; safe removability not established.

### 28. `src/hle/rt64_workload.cpp`

Reset/transfer of workload identity and rendering state.

- Hunk 1: `@@ -19,6 +19,11 @@ namespace RT64 {` — retain active implementation/declaration; safe removability not established.
- Hunk 2: `@@ -323,4 +328,4 @@ namespace RT64 {` — retain active implementation/declaration; safe removability not established.

### 29. `src/hle/rt64_workload.h`

Workload payload for exact scene, target, HUD and authored timing.

- Hunk 1: `@@ -21,6 +21,7 @@` — retain active implementation/declaration; safe removability not established.
- Hunk 2: `@@ -225,6 +226,11 @@ namespace RT64 {` — retain active implementation/declaration; safe removability not established.
- Hunk 3: `@@ -254,4 +260,4 @@ namespace RT64 {` — retain active implementation/declaration; safe removability not established.

### 30. `src/hle/rt64_workload_queue.cpp`

Retain matching preflight, cadence selection, bounded immutable image reuse, native fallback and queue synchronization. Gate optional resource inventory walking/locking when diagnostics are off.

- Hunk 1: `@@ -5,8 +5,25 @@` — mixed: retain active contract; remove/gate only the named subset above where present.
- Hunk 2: `@@ -51,15 +68,15 @@ namespace RT64 {` — mixed: retain active contract; remove/gate only the named subset above where present.
- Hunk 3: `@@ -131,14 +148,18 @@ namespace RT64 {` — mixed: retain active contract; remove/gate only the named subset above where present.
- Hunk 4: `@@ -148,6 +169,12 @@ namespace RT64 {` — mixed: retain active contract; remove/gate only the named subset above where present.
- Hunk 5: `@@ -271,6 +298,9 @@ namespace RT64 {` — mixed: retain active contract; remove/gate only the named subset above where present.
- Hunk 6: `@@ -289,28 +319,36 @@ namespace RT64 {` — mixed: retain active contract; remove/gate only the named subset above where present.
- Hunk 7: `@@ -319,7 +357,7 @@ namespace RT64 {` — mixed: retain active contract; remove/gate only the named subset above where present.
- Hunk 8: `@@ -387,7 +425,7 @@ namespace RT64 {` — mixed: retain active contract; remove/gate only the named subset above where present.
- Hunk 9: `@@ -697,7 +735,26 @@ namespace RT64 {` — mixed: retain active contract; remove/gate only the named subset above where present.
- Hunk 10: `@@ -839,34 +896,111 @@ namespace RT64 {` — mixed: retain active contract; remove/gate only the named subset above where present.
- Hunk 11: `@@ -880,14 +1014,20 @@ namespace RT64 {` — mixed: retain active contract; remove/gate only the named subset above where present.
- Hunk 12: `@@ -904,7 +1044,9 @@ namespace RT64 {` — mixed: retain active contract; remove/gate only the named subset above where present.
- Hunk 13: `@@ -914,6 +1056,43 @@ namespace RT64 {` — mixed: retain active contract; remove/gate only the named subset above where present.
- Hunk 14: `@@ -929,6 +1108,12 @@ namespace RT64 {` — mixed: retain active contract; remove/gate only the named subset above where present.
- Hunk 15: `@@ -955,46 +1140,145 @@ namespace RT64 {` — mixed: retain active contract; remove/gate only the named subset above where present.
- Hunk 16: `@@ -1004,12 +1288,6 @@ namespace RT64 {` — mixed: retain active contract; remove/gate only the named subset above where present.
- Hunk 17: `@@ -1019,6 +1297,29 @@ namespace RT64 {` — mixed: retain active contract; remove/gate only the named subset above where present.
- Hunk 18: `@@ -1029,7 +1330,7 @@ namespace RT64 {` — mixed: retain active contract; remove/gate only the named subset above where present.
- Hunk 19: `@@ -1040,13 +1341,22 @@ namespace RT64 {` — mixed: retain active contract; remove/gate only the named subset above where present.
- Hunk 20: `@@ -1055,28 +1365,11 @@ namespace RT64 {` — mixed: retain active contract; remove/gate only the named subset above where present.
- Hunk 21: `@@ -1085,7 +1378,7 @@ namespace RT64 {` — mixed: retain active contract; remove/gate only the named subset above where present.
- Hunk 22: `@@ -1093,7 +1386,6 @@ namespace RT64 {` — mixed: retain active contract; remove/gate only the named subset above where present.
- Hunk 23: `@@ -1116,23 +1408,40 @@ namespace RT64 {` — mixed: retain active contract; remove/gate only the named subset above where present.
- Hunk 24: `@@ -1143,7 +1452,8 @@ namespace RT64 {` — mixed: retain active contract; remove/gate only the named subset above where present.
- Hunk 25: `@@ -1152,16 +1462,27 @@ namespace RT64 {` — mixed: retain active contract; remove/gate only the named subset above where present.
- Hunk 26: `@@ -1221,4 +1542,4 @@ namespace RT64 {` — mixed: retain active contract; remove/gate only the named subset above where present.

### 31. `src/hle/rt64_workload_queue.h`

Render success contract prevents retaining a target that did not pass the self-contained overwrite proof.

- Hunk 1: `@@ -113,14 +113,15 @@ namespace RT64 {` — retain active implementation/declaration; safe removability not established.

### 32. `src/render/rt64_descriptor_sets.h`

Depth-mask binding exists solely for the disabled VI blur: experimental leftover.

- Hunk 1: `@@ -662,11 +662,13 @@ namespace RT64 {` — experimental leftover; remove with the complete disabled-effect chain.

### 33. `src/render/rt64_framebuffer_renderer.cpp`

Keep race HUD anchoring, split-screen/high-resolution rules and self-contained framebuffer proof. These protect both visual layout and safe image reuse.

- Hunk 1: `@@ -4,6 +4,8 @@` — retain active implementation/declaration; safe removability not established.
- Hunk 2: `@@ -15,6 +17,9 @@` — retain active implementation/declaration; safe removability not established.
- Hunk 3: `@@ -51,6 +56,244 @@ namespace interop {` — retain active implementation/declaration; safe removability not established.
- Hunk 4: `@@ -175,6 +418,8 @@ namespace RT64 {` — retain active implementation/declaration; safe removability not established.
- Hunk 5: `@@ -1318,9 +1563,32 @@ namespace RT64 {` — retain active implementation/declaration; safe removability not established.
- Hunk 6: `@@ -1433,7 +1701,17 @@ namespace RT64 {` — retain active implementation/declaration; safe removability not established.
- Hunk 7: `@@ -1447,15 +1725,32 @@ namespace RT64 {` — retain active implementation/declaration; safe removability not established.
- Hunk 8: `@@ -1508,6 +1803,12 @@ namespace RT64 {` — retain active implementation/declaration; safe removability not established.
- Hunk 9: `@@ -1625,6 +1926,34 @@ namespace RT64 {` — retain active implementation/declaration; safe removability not established.
- Hunk 10: `@@ -1637,6 +1966,41 @@ namespace RT64 {` — retain active implementation/declaration; safe removability not established.
- Hunk 11: `@@ -1645,14 +2009,16 @@ namespace RT64 {` — retain active implementation/declaration; safe removability not established.
- Hunk 12: `@@ -1676,7 +2042,13 @@ namespace RT64 {` — retain active implementation/declaration; safe removability not established.
- Hunk 13: `@@ -1760,6 +2132,85 @@ namespace RT64 {` — retain active implementation/declaration; safe removability not established.
- Hunk 14: `@@ -1917,4 +2368,4 @@ namespace RT64 {` — retain active implementation/declaration; safe removability not established.

### 34. `src/render/rt64_framebuffer_renderer.h`

Declarations and race-HUD state for the active renderer changes.

- Hunk 1: `@@ -123,6 +123,8 @@ namespace RT64 {` — retain active implementation/declaration; safe removability not established.
- Hunk 2: `@@ -166,6 +168,10 @@ namespace RT64 {` — retain active implementation/declaration; safe removability not established.
- Hunk 3: `@@ -179,4 +185,4 @@ namespace RT64 {` — retain active implementation/declaration; safe removability not established.

### 35. `src/render/rt64_projection_processor.cpp`

Preserves authored projection endpoints and near/far behavior during interpolation.

- Hunk 1: `@@ -14,7 +14,7 @@ namespace RT64 {` — retain active implementation/declaration; safe removability not established.
- Hunk 2: `@@ -126,7 +126,20 @@ namespace RT64 {` — retain active implementation/declaration; safe removability not established.
- Hunk 3: `@@ -160,4 +173,4 @@ namespace RT64 {` — retain active implementation/declaration; safe removability not established.

### 36. `src/render/rt64_projection_processor.h`

End-of-file whitespace only: redundant.

- Hunk 1: `@@ -30,4 +30,4 @@ namespace RT64 {` — redundant; restore pinned original.

### 37. `src/render/rt64_texture_cache.cpp`

Jabo replacement texture lookup/loading and reload cleanup. Optional pack path remains supported; do not remove because stock play does not use it.

- Hunk 1: `@@ -13,6 +13,7 @@` — retain active implementation/declaration; safe removability not established.
- Hunk 2: `@@ -58,6 +59,9 @@ namespace RT64 {` — retain active implementation/declaration; safe removability not established.
- Hunk 3: `@@ -1306,7 +1310,28 @@ namespace RT64 {` — retain active implementation/declaration; safe removability not established.
- Hunk 4: `@@ -1467,6 +1492,7 @@ namespace RT64 {` — retain active implementation/declaration; safe removability not established.
- Hunk 5: `@@ -1479,6 +1505,7 @@ namespace RT64 {` — retain active implementation/declaration; safe removability not established.
- Hunk 6: `@@ -1493,6 +1520,7 @@ namespace RT64 {` — retain active implementation/declaration; safe removability not established.
- Hunk 7: `@@ -1518,7 +1546,7 @@ namespace RT64 {` — retain active implementation/declaration; safe removability not established.
- Hunk 8: `@@ -1560,6 +1588,7 @@ namespace RT64 {` — retain active implementation/declaration; safe removability not established.

### 38. `src/render/rt64_texture_cache.h`

Storage for those replacement lookup paths.

- Hunk 1: `@@ -83,6 +83,7 @@ namespace RT64 {` — retain active implementation/declaration; safe removability not established.
- Hunk 2: `@@ -265,4 +266,4 @@ namespace RT64 {` — retain active implementation/declaration; safe removability not established.

### 39. `src/render/rt64_transform_processor.cpp`

Exact transform endpoints avoid decomposition drift; intermediate interpolation remains.

- Hunk 1: `@@ -24,6 +24,8 @@ namespace RT64 {` — retain active implementation/declaration; safe removability not established.
- Hunk 2: `@@ -42,8 +44,13 @@ namespace RT64 {` — retain active implementation/declaration; safe removability not established.
- Hunk 3: `@@ -89,4 +96,4 @@ namespace RT64 {` — retain active implementation/declaration; safe removability not established.

### 40. `src/render/rt64_vi_renderer.cpp`

Keep stretch viewport/scissor support. Remove depth-mask descriptor update, haze constants and unused animation counter.

- Hunk 1: `@@ -8,6 +8,9 @@` — mixed: retain active contract; remove/gate only the named subset above where present.
- Hunk 2: `@@ -23,7 +26,14 @@ namespace RT64 {` — mixed: retain active contract; remove/gate only the named subset above where present.
- Hunk 3: `@@ -66,10 +76,14 @@ namespace RT64 {` — mixed: retain active contract; remove/gate only the named subset above where present.
- Hunk 4: `@@ -77,6 +91,13 @@ namespace RT64 {` — mixed: retain active contract; remove/gate only the named subset above where present.
- Hunk 5: `@@ -86,7 +107,7 @@ namespace RT64 {` — mixed: retain active contract; remove/gate only the named subset above where present.
- Hunk 6: `@@ -111,15 +132,15 @@ namespace RT64 {` — mixed: retain active contract; remove/gate only the named subset above where present.

### 41. `src/render/rt64_vi_renderer.h`

Keep stretch parameter/signature. Remove blur-only parameters and animation counter.

- Hunk 1: `@@ -14,11 +14,13 @@ namespace RT64 {` — mixed: retain active contract; remove/gate only the named subset above where present.
- Hunk 2: `@@ -29,11 +31,15 @@ namespace RT64 {` — mixed: retain active contract; remove/gate only the named subset above where present.

### 42. `src/shaders/RasterPS.hlsl`

End-of-file whitespace only: redundant.

- Hunk 1: `@@ -310,4 +310,4 @@ void PSMain(` — redundant; restore pinned original.

### 43. `src/shaders/VideoInterfacePS.hlsl`

Entire change is the disabled screen-space blur, depth texture and sample wrapper: compiled but unused effect.

- Hunk 1: `@@ -4,9 +4,12 @@` — experimental leftover; remove with the complete disabled-effect chain.
- Hunk 2: `@@ -32,10 +35,132 @@ float4 PixelAntialiasing(float2 uv) {` — experimental leftover; remove with the complete disabled-effect chain.

### 44. `src/shared/rt64_framebuffer_params.h`

End-of-file whitespace only: redundant.

- Hunk 1: `@@ -16,4 +16,4 @@ namespace interop {` — redundant; restore pinned original.

### 45. `src/shared/rt64_video_interface.h`

Entire change is blur-only VI constants/padding: experimental leftover.

- Hunk 1: `@@ -13,7 +13,14 @@ namespace interop {` — experimental leftover; remove with the complete disabled-effect chain.

### 46. `src/tools/texture_packer/texture_packer.cpp`

Offline texture-packer Jabo path matching/case validation. Required tooling, not a runtime hot path.

- Hunk 1: `@@ -341,8 +341,9 @@ int main(int argc, char *argv[]) {` — required supported offline tooling; not executed during gameplay.
- Hunk 2: `@@ -350,6 +351,12 @@ int main(int argc, char *argv[]) {` — required supported offline tooling; not executed during gameplay.
- Hunk 3: `@@ -429,6 +436,20 @@ int main(int argc, char *argv[]) {` — required supported offline tooling; not executed during gameplay.

