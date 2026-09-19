## Synchronization expansion in progress — September 11, 2026

Continue from Candidate15 source/fixes; last distributed ZIP remains Candidate15/protocol14. Working source now protocol15 and must not be packaged as completed world synchronization. Production compilation passes. Implemented existing host AI racer root updates (canonical roster, identity guard, no remote writes on host), native float bike durability, selected weapon and 15 inventory halfwords, round-scoped rider packets/state reset, reserved player ownership mask, and corresponding log fields. Offline 2/4/14 peers, native memory ownership/identity/equipment checks and 37-column bounded CSV checks pass. These are state replication improvements, not authoritative hit/damage arbitration or complete AI lifecycle handling.

Still required: traffic/pedestrian lifecycle and shared state, host-authoritative interactions/results, complete animation/body synchronization, fourteen-player Custom Cop behavior. User additionally requires a full producer-to-draw trace of every rider/bike connection facet after the additions; verify mounted/crashed/ejected/recovered states, weapons, animation clocks, roots and all player mappings. Do not infer completeness from transmitted root positions or packet placeholders.

Traffic trace: 6BFF4 checks 20-car cap (A6528), resolves route via6B9E8, allocates via46488, initializes via77498, chooses model/builds scene node via47668, marks334 active/336 zero and places via6BA88. 47668 includes RNG model selection and render asset allocations. A coordinates-only update must not substitute for canonical lifecycle and collision geometry. Further tracing needed before activating traffic replication.

Full progress and hashes: analysis/release-1.2-online/sync-expansion-progress.json. No game launch or new ZIP created. Preserve Candidate15 as crash-fix test package.

# Online synchronization coverage audit

September 11, 2026. Audited clean release1.2 source, Candidate15/protocol14. Read-only code audit plus prior offline results; no live game launched. This supersedes any implication that the present rider transport is complete online gameplay synchronization.

| System | Actual coverage | Remaining gap |
| --- | --- | --- |
| Setup and selections | Host setup words, race options, seed, character/bike confirmations, names and start/load barriers are transported and applied. | Shared starting seed does not guarantee subsequent deterministic simulation. |
| Connected riders and bikes | Each owner captures body/wheels, rider position, movement sources, rotations, detailed anchor, attachment/eject and recovery gate. Host relays; recipients apply to connected remote actors. All14 slot mappings tested offline. | Live alignment/smoothing still pending. Host relays client proposals rather than running a single authoritative physics simulation. |
| Crash and body animation | Root motion, attachment/eject flags and recovery gate. | No complete animation state machine, animation clock, articulated body state or reliable crash/recovery event authority. Identical visible crashes are not guaranteed. |
| Health, damage, weapon, lean/suspension | Fields exist in RiderState and wire conversion. | capture_bike/apply_bike do not populate/apply those fields. Wire presence is not synchronization. No authoritative hit or inventory event stream. |
| AI racers / police | Setup options transferred. | Snapshot loop publishes only local connected player's actor and applies only connected slots. No host-owned AI snapshot channel. |
| Traffic vehicles | Local native traffic logic and distance extension remain. | No spawn/despawn IDs, vehicle transforms or collision authority in network protocol. Independent traffic can diverge. |
| Pedestrians / mutable world objects | Initial course/options locally loaded. | No dynamic object lifecycle/state channel. |
| Terrain and static scenery | Same selected track is loaded on each PC. | No continuous terrain packets are needed for immutable geometry. Protocol handshake does not establish content equivalence. Per-camera culling/LOD may legitimately differ; dynamic collision-affecting objects must agree. |
| HUD / race progress / results | Local HUD actor selection, online viewport mapping and names. | Rendering the local HUD is not synchronization of scores, progress, damage, finish order, timer or game-over events. No authoritative result/event packet. |
| Custom Cop | Selection/options ready checks, local gameplay code runs for non-replicated online path. | custom_cop.cpp local() explicitly excludes replicated_riders; begin requires <=4 humans. No fourteen-player parity, authoritative bust tally/elimination/win stream. Siren/shout effects are not authoritative events. |
| Pause / disconnect / voice | Host pause input routing; host-loss terminal flow; voice relay. | Previous tests cover specific behaviors, not complete gameplay equivalence. |

## Evidence and limits

Sources: native/src/rr64_netplay.hpp (RiderState and setup); rr64_netplay.cpp (packet families, conversions, endpoint-owned proposals, relay); rr64_online_race_sync.cpp (capture_bike/apply_bike, connected-only loops); rr64_online_menu.cpp and rr64_online_guest_flow.cpp (setup application); rr64_custom_cop.cpp (replicated exclusion and four-human guard); rr64_traffic_distance.hpp (local spawn extension); rr64_sync_log.hpp (diagnostic scope).

Existing logs cover setup/RNG samples, inputs, root hashes/positions, actor pointers and presentation offsets. They do not record complete traffic, AI, pedestrian, damage, animation or outcome agreement. Matching root hashes cannot establish whole-world agreement. Existing 2/4/14-process tests establish transport/slot mapping, not fourteen-player gameplay parity. Candidate15 live start, movement and HUD checks remain pending.

## Required implementation sequence

1. Define canonical entity identities/generations and host-owned shared world state, independent of local camera/controller indices. Explicitly decide authority for player interactions; client-owned movement alone cannot settle shared collisions.
2. Replicate traffic, AI and pedestrians including lifecycle and movement; retain local static terrain/rendering. Add bounded packet scheduling, stale entity rejection and per-category telemetry.
3. Add ordered, deduplicated gameplay events and authoritative damage/inventory, crash/recovery, bust/elimination, progress and race results. Map animation state/clock only after tracing native semantics; never copy pointers or entire guest structs.
4. Extend Custom Cop logic beyond four controller slots using canonical network actors. Verify host/client/14-player outcomes, not only selected equipment.
5. Test under packet loss, delay/reordering, disconnect and entity reuse, then paired live tests comparing world/event state. Keep visual interpolation separate from authoritative gameplay state.

No new executable or network behavior was produced by this audit. Candidate15 remains the existing test candidate, not a complete-sync release.
