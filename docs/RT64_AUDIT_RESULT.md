# RT64 cleanup audit result

Local candidate, 2026-09-07. **Not published. Runtime acceptance pending.**

The original patch's 223 textual hunks across 46 files were inventoried in
[the pre-change assessment](RT64_AUDIT_BEFORE.md). The remaining patch is
**4,574 lines / 248,972 bytes / 38 files**, down from **5,359 lines / 282,934 bytes / 46 files**.
This removes 785 patch lines (14.6%). Patch lines include context, not just code.

## Changes made

- Removed the explicitly disabled VI horizon blur, depth association scans/map/locks,
  unused depth-mask binding and constants. Restored the matching upstream shader and
  shared layout together. The active RSP vertex-fog/haze option is retained.
- Removed old RT64 startup-stage fprintf/fflush scaffolding and whitespace-only
  modifications. Retained the D3D12 compositor latency setting and upstream errors.
- Gated optional pipeline Scope clocks, authored observation hashes and periodic
  resource scans behind the existing process-start RR64_DIAGNOSTICS=1 opt-in.
  Required authored cadence and interpolation geometry certificates remain active.
- Added short comments explaining the diagnostics boundary and a
  [feature-to-file editing guide](RT64_EDITING_GUIDE.md). Broad structural changes
  are deferred until live testing confirms stability.
- Repaired one missing abort-on-use application callback in the existing renderer
  test fixture; no game callback behavior was changed.

## Remaining modifications and removal risk

| Group | Execution and purpose | Risk of removal |
| --- | --- | --- |
| Deadline pacing and queue synchronization | Active producer/present queues; absolute deadlines, wakeups and native fallback | Stalls, uneven cadence or recurring lower-rate output |
| Scene/target metadata and owned image batches | Active race presentation; ties images to writer, scene and resource generation | Reused buffers, flicker or wrong-frame presentation |
| Geometry certificates and matching preflight | Runs when interpolation/velocity matching is needed; rejects uncertain correspondence | Wrong model pairing or geometry interpolation artifacts |
| Framebuffer overwrite proof | Before retaining rendered images; rejects partial/composite cases | Incomplete or stale copied frames |
| Transform/projection endpoints | During interpolation; preserves exact authored endpoints/clipping | Drift, clipping or visible transitions |
| Widescreen/high-resolution/HUD | Configured viewport and race-HUD paths | Aspect ratio, split-screen or result-layout regressions |
| Packed triangles and bulk vertex streams | Terrain/object packet producers and RSP consumers | Loss of terrain performance or packet semantics |
| TMEM hash/load reuse | Texture tile processing and invalidation | Texture corruption or repeated texture work |
| Jabo replacement textures | Optional texture pack loading and offline packer | Converted mod incompatibility |
| Vertex fog/haze | User-configured RSP fog behavior | Removal of a supported graphics option |
| Optional observations | Enabled diagnostics; some existing counters/profilers remain | Loss of future diagnostic visibility; further overhead reduction needs separate evidence |

The editing guide and original per-file/hunk inventory provide the detailed source
mapping. Active usage does not establish that every retained implementation is
minimal. Uncertain removals were deliberately not attempted.

## Validation and limits

- Both cleanup groups built successfully, including DXIL and SPIR-V VI shaders.
- Eight renderer checks passed after the first group. Final suite: **39/39 passed**,
  covering image reuse (including GPU), matching, pacing, TMEM, vertex/triangle
  semantics, LOD/pose/world/video behavior, and diagnostics enabled/disabled.
- Four exported dependency patches applied to clean pinned trees; all 73 changed
  tracked files and 19 extra files matched intended sources (canonical line endings).
- All 19 override checksums matched staged Git objects and a checkout with Windows
  automatic line-ending conversion enabled.
- The corrected setup script fetched and patched RecompFrontend from a fresh
  download, then accepted a second unchanged setup run.
- The published binary and source ZIP hashes remain unchanged. No GitHub mutation.
- No game was launched. No new in-game FPS, frame-time or visual-success claim.
  Windows source setup/checkout checks do not establish Linux compilation.

The disabled blur's 25-tap body was already not running. The cleanup removes
identified CPU bookkeeping and optional clock/hash/scan work; its FPS impact is
unmeasured. Live tests should compare equivalent maps, camera directions, settings
and mod state, including long races and split-screen lap maps.

## Contributor source-build report

The two reported texture cache header hash mismatches were real. The old lock
hashed mixed Windows line endings; published Git objects held normalized LF.
The local candidate exports canonical LF bytes, updates their hashes and adds
.gitattributes rules preserving byte-locked overrides and patches.

RecompFrontend commit d0d90ba49f46f4896aaeda362056c21b1e342561 exists on GitHub but
is absent from a normal clone's current branch history. A fresh-clone check failed
to find it; an explicit fetch by SHA succeeded. The candidate setup script now
performs that fetch when required, preserving the pinned revision.

These fixes are local and unposted. Linux build verification and live gameplay
acceptance remain follow-up work. Any publication requires renewed authorization.
