# Networking research and application

2026-09-11. Primary sources reviewed for the online test-build work:

- [Glenn Fiedler: Snapshot Interpolation](https://gafferongames.com/post/snapshot_interpolation/): buffer uneven arrivals and interpolate remote display state; velocity-aware interpolation and quaternion slerp can improve motion. Packet rate alone does not remove jitter. Extrapolation is unreliable during interacting-body collisions.
- [Glenn Fiedler: State Synchronization](https://gafferongames.com/post/state_synchronization/): sequence timing, jitter buffering and valid physics state matter. Keep visual error smoothing separate from simulation correction. Prioritize state under a bandwidth budget.
- [Gabriel Gambetta: Prediction and Reconciliation](https://www.gabrielgambetta.com/client-side-prediction-server-reconciliation.html): local prediction requires numbered inputs, acknowledgment of the last processed input and replay of remaining inputs against authoritative state.
- [Gabriel Gambetta: Entity Interpolation](https://www.gabrielgambetta.com/entity-interpolation.html): remote entities can be displayed on a delayed timeline while local controls respond immediately.
- [Valve GameNetworkingSockets](https://github.com/ValveSoftware/GameNetworkingSockets): reliable/unreliable messaging, connection management and network simulation facilities; entity serialization/state synchronization remain application responsibilities. Some Steam services require separate access. No transport-library switch or dependency was made here.

## Application to this native runtime

Host authority must cover shared dynamic actors and outcomes. Owner-root snapshots currently do not resolve conflicting collision/damage decisions. Do not claim local replay is server reconciliation: the current protocol has no acknowledged input-history replay. Native replay also needs isolated physics/time/RNG/effect handling to avoid duplicate sounds, damage or allocations.

The existing visual helper recorded new samples at receiver observation times. Even steady25ms packets sampled by a10ms game loop distorted timing. Added owner monotonic microseconds to coherent rider state, wire conversion and host AI publication. Interpolation preserves relative owner spacing anchored in receiver time, without requiring synchronized wall clocks. Fixed a clock-domain bug in the first experiment: mapped sample times may briefly lead arrival observation, which must not be treated as the receiver clock moving backwards. Actual local-clock reversal is tracked separately. Histories retain up to500ms gaps before resetting; discrete attachment/recovery/teleport changes still reset. No simulation records receive the interpolated calculation.

Tests: deterministic10-second linear motion;40Hz packets;100Hz receiver sampling;50ms base delivery delay; uniform +/-20ms jitter, deterministic5% loss, and200ms outage. Baseline RMS step errors0.155919/0.455861/0.452838/0.702415. Revised0/0/0/0.565862. Normal expected step1. Revised outage still has16 held samples and17.5 maximum step (baseline14 held,16.847839 maximum); do not call outage behavior solved. First three scenarios have regression assertions for even steps and no stalls. This is synthetic translation evidence, not measured live gameplay, rotation, crash animation, bandwidth or end-to-end latency improvement.

Follow-up implementation requirements: common presentation timeline for all facets of an actor; validated rotation/animation and attachment-relative pose; measured jitter-buffer occupancy/underruns and clock drift; complete round-scoped traffic snapshots and lifecycle identities; reliable or repeatedly acknowledged critical outcomes; tests for multiple bursts/reordering/duplicates, race changes, different simulation rates and14-player bandwidth. Do not blindly install Hermite interpolation until native velocity units and discontinuities are established.

Traffic trace gained this session: free-list46488 uses +35C link;77498 stores native traffic ID at+4;6BA88 inserts into compact D76E0 roster and stores mutable index+28. Thus pool address and roster index cannot identify a vehicle across recycle/compaction. Native model creation47668 must be reproduced locally; pointers cannot be transmitted. Traffic channel is still pending, not falsely marked implemented.

No new game ZIP or game launch in this research pass. Working protocol15 differs from downloadable Candidate15/protocol14.

## Implemented follow-up: bounded playback recovery

See analysis/release-1.2-online/playback-reconcile-review.md for current measurements and limits. Sender timestamps now feed a bounded playback cursor with gradual occupancy corrections; local receipt time determines staleness. Ten-minute positive/negative clock drift cases are tested. Traffic host capture/transport is implemented, superseding the earlier pending-channel note; native client lifecycle execution is still pending. No live acceptance or new package.
