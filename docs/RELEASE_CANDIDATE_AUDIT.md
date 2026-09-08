# Release candidate cleanup audit

Baseline accepted by user: HUD-Rollback, SHA256
0e3195bd7c0479a391d6f7e0c4c11969688d2ced37d7a4b8af97319cb9549bc5.
Cleaned candidate: 8185fb0b7a885079e925666ece4eb16f2beb13f497b55869992b0d69a0884e62.

## Changes and classification

- Removed the obsolete frontend FPS preset enum, option vector and converter;
  the active numeric FPS slider remains unchanged. Removed a duplicate commented
  Manual aspect entry. Added a short note separating FPS from simulation.
- Confirmed removed split-Wide worker/hooks/workload fields and split-HUD helper
  experiments are absent from active source. Earlier HUD renderer is retained.
- Removed five superseded experiment documents from the isolated source export.
  Preserved historical candidates and analysis evidence outside the release.
- Updated local-player guidance and added a feature-to-file contributor map.
- Kept the platform-sensitive guest_pointer overload: one Windows warning alone
  does not establish that it is unused on all supported source configurations.
- Kept optional texture conversion/packers, diagnostics and renderer correctness
  checks. These are supported tools/contracts, not demonstrated dead code.
- Diagnostic logging remains off by default. Non-Windows startup no longer
  creates an empty log without opt-in. Error/crash reporting remains. The game
  package has no runtime logs, test-launch scripts or TEST_BUILD instructions.
  The separate internal ready-gated launcher clears inherited diagnostic flags.

## Dependency and source evidence

All four exported patches reconstructed at their pinned local Git revisions;
reconstructed changed files match intended source (canonical LF for text).
Every locked extra was exported from current source and its hash regenerated.
24 locked patch/override Git blobs retain exact bytes with core.autocrlf=true;
20 override hashes also pass a fresh checkout-index test.
Total patch bytes: 373507 before, 373027 after. This is not an FPS claim.
All dependency commit objects are present locally. Remote availability was not
re-fetched during this audit; Linux compilation was not performed here.

## Verification and limits

Release build passed. Video, weapon rendering, world terrain and world object
smoke tests passed. No gameplay launch was performed after cleanup. The user's
acceptance applies to the pre-cleanup baseline; this exact binary remains a
potential release candidate pending their test. Nothing published.

This was a conservative review of the retained changes and packaging, not a
proof that every branch in the entire upstream dependency graph is reachable.
Uncertain code was retained. Further broad removal would need focused evidence.
