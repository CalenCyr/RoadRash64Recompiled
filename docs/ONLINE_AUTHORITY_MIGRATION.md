## Native client history connected - September12

Production SHA256 aea981a96ca1340e668d2c99897b9245f25b97005ecc02ed89e8a72806808027. authority_queue_input_recorded returns the exact accepted command atomically. Dormant-authority client update hooks now capture baseline0 before the first input and completed guest/native state after each outer update. The runtime guard supplies the8MiB private copy; immutable journal pages retain it. Cop rules/posts and accepted command are retained alongside every guest frame. Missing/duplicate/out-of-order capture ends the session. This history does not yet retire/replay or apply corrected local state; authority must remain disabled until that integration and cadence are complete.

RR64PredictionCaptureSmoke passes baseline/completed-state separation, historical cop fields, duplicate/pending rejection and reset; snapshot call is substituted in that test, while RR64SnapshotGateSmoke separately checks concurrency. Step/Actor/Channel checks rebuilt and pass. Native production compiles. No game launch or candidate ZIP. User requests continuous work through the complete build; next is native replay/correction plus cadence/lifecycle, not distribution of this intermediate executable.

## Guarded baseline capture - September12

Production SHA256 dfeeb838435487d9614317180b4f129474ec2c9669df92e47f6d4031b9943d92. Runtime adds copy_guest_snapshot, restricted to a running emulated CPU thread and8MiB, with nonoverlapping caller-owned output. Graphics action processing and RSP task execution hold shared access; snapshot copy holds exclusive access without callbacks or guest scheduling. Runtime source trace: PI DMA is synchronous in the emulated caller; external messages enqueue native records and are consumed by the scheduled CPU thread; RT64 full-sync writes color/depth to RDRAM inside graphics processing. No whole-image restore into live memory is allowed. The helper is not yet wired to live prediction history.

RR64SnapshotGateSmoke passes1000 copies with concurrent disjoint writer threads, confirms shared workers can overlap and rejects overlapping output. This validates the lock primitive, not live performance or full replay. Production compiles. Updated pinned N64ModernRuntime events patch and hashed rr64_snapshot_gate.hpp override; independently reapplied the events patch to pinned upstream and matched the live dependency file. Checkpoint script now includes dependency patches/overrides/lockfile. Next: connect guarded baseline capture to prediction history, then the native executor and cadence/lifecycle. User requires continued work through finished build. No test package or launch.

## Protocol25 crash damping transport - September12

Production compiles, SHA256 154e7b0c5812c52fd79d8496da7107f3f71036a8f6893ac91fdba119b82715df. Added the three audited detached-body damping vectors at rider+98,+174,+180 to the authoritative frame, validated finite before capture/publication/assembly and restored only to reciprocal local rider/bike ownership. Roots and dynamics are read from one immutable host frame per remote application pass. Native local physics and legacy root schema are unchanged. Two riders per datagram/seven parts preserve the under1200-byte bound. A frame is published only after all seven parts agree. Native dynamics logging now ignores isolated replay before opening/writing the recorder or changing entry/exit pairing.

Seven affected checks pass: Step, Actor, Channel, PredictionSideEffect, PredictionCopRules, AuthorityCopUI, AuthoritativeClock. Capture covers shuffled14-slot identities; restoration verifies byte-exact untouched surrounding memory and rejects NaN/out-of-bounds atomically. Actual dispatcher covers out-of-order seven-part assembly and invalid dynamics. These do not prove full native crash replay; only the confirmed damping field gap is filled. Authority remains dormant. Native replay memory ownership/executor, cadence and lifecycle integration remain outstanding. No game launch or packaged candidate.

User explicitly asked to continue through the full build without incremental stopping. Keep working; do not mistake this checkpoint for a completed deliverable.

## Authority failure handling and replay-state gap � September 12

Production build succeeds, SHA256 934c4bba06c963bb602b3d0d11e8202a735c5ca2adffe7c76ffa8dd4b3424325. The new model remains dormant; no game launch or playable authority package. Failed input admission, native step start, missing control translation, acknowledgement or capture now ends the authority session through the existing timed menu-return path. It retains race/camera identity during teardown and rejects subsequent input/step acknowledgement. Local simulation does not become a fallback. The local failure label is Online sync stopped; peers receive the existing disconnect notification. Actual dispatcher and native-step offline checks pass; live menu presentation remains untested.

The unused TickBudget primitive passes shared-step scheduling with different host/client frame-pump rates (60/60,59/60,30/60,60/30,24/144) without growing queues over60seconds. This is not native cadence integration. Evidence: clock-check.txt. Do not replace the game's variable timing with a nominal60Hz constant without preserving its timing semantics.

Confirmed crash replay schema gap: embedded rider physics starts at+28, and damping touches rider+98,+174,+180 vectors absent from RiderRootState. See crash-replay-field-gap.json. Full historical bike/rider byte traces do not capture all external/native replay dependencies. No physics constants changed.

Still required: complete safe movement/crash replay state and executor, shared native cadence integration, activation/lifecycle and full gameplay verification before packaging. This is a successful intermediate compilation, not the requested finished candidate.

## Protocol24 timing and cop outcomes — September 12

Production compiles: SHA256 219eae1fcacd273b54a9c4cdc0b91dab9aae23bbe2e224af8ea517c75c765682. Authority remains disabled; this is not a finished playable authority candidate. No launch or distributable test ZIP.

Authoritative frames now retain exact native delta, three derived timing fields, both game/race clocks and one/two-pass metadata. Every packet fragment must agree on timing and cop-result metadata before atomic commit. Invalid/nonfinite timing, siren flags, cue ages and conflicting fragments are rejected. Actor capture now includes pointer-free role, eligibility, busted flag and bust tally through the actor route-state pointer. Cop mode, victory age, siren and pursuit cue come from the host native state at the same capture boundary. These additions do not constitute a complete movement/crash replay schema or implement clock alignment.

Authority-host cop roles and post state support the canonical14-slot human mask, including gaps. AI roles and native local player limits stay unchanged. Client cop HUD uses its own canonical network outcome on the full320x240 canvas; it does not award local busts or infer them from the split-screen actor. Client siren reads host outcome via inverse guest/network mapping. Host-mode activation must occur early enough for native cop initialization; lifecycle is still pending. General client race-result transitions and complete crash/pose state remain pending.

Checks passed: authority channel/capture, actual generated actor loop for2/4/14 riders with one/two passes (physics/overrides mocked), historical cop rules/posts, newRR64AuthorityCopUISmoke (native drawing mocked), and originalRR64LocalRaceOptionsSmoke. The actor fixture now includes8006B16C..8006B2C4 so its per-pass counter reset matches native code. Tests prove a press happens once across two passes while held controls persist. HUD checks cover network slots0/3/13, yellow cue/win labels, no frame/no mode, and original local two-view layout. Production build log: analysis/release-1.2-online/authority-outcome-production-build.log.

Source checkpoint tooling now includes .inc implementation fragments; previous source-only checkpoint extension filters omitted those two files. Preserve older archives as partial historical artifacts. Use save_authority_checkpoint.py for new local recovery snapshots; no cloud upload is implied.

Remaining before requested build/package: complete native replay state and isolated executor, command/native-time alignment (the59/60 synthetic backlog finding remains unresolved), explicit lifecycle activation/fault recovery, remaining complete authoritative gameplay transitions and regression verification. Do not activate merely because transport/UI checks pass.

## Native timing trace — September 11

Source tracing found variable, smoothed/clamped physics_delta at0x8009CBA8 plus derived timing fields, and a two-substep branch inside6AFFC controlled by0x800A659A. Current authority commands count outer6AFFC calls, not fixed-duration physics ticks. Replay must preserve the substep schedule and coherent timing fields. Evidence and exact instruction ranges: analysis/release-1.2-online/authority-migration/native-timing-trace.json. No timing behavior was changed and no live race was measured. Authority remains dormant; replay/cadence integration remains unfinished.

## Simulation cadence audit — September 11

RR64AuthoritativeCadenceAudit exercises the actual HostRound and ClientHistory with immediate delivery/ACKs and hypothetical independent native simulation rates. In60seconds:60/60 host/client stays at1 pending;59/60 grows to61 pending and1000ms oldest input;30/60 exhausts256 history slots at8.55seconds;60/30 stays at1 pending. Results: analysis/release-1.2-online/authority-migration/cadence-audit.json. This is a negative architecture audit, not another passing sync test or measurement of the game's actual native rates. Do not confuse simulation cadence with the FPS slider/display rate.

Before authority activation, align prediction/input simulation time with the host or transmit bounded command durations and replay the exact consumed intervals. Merely increasing queue size, dropping old commands, or smoothing positions does not solve this mismatch while preserving input edges and gameplay. Audit actual6AFFC timing before selecting the native integration; no game launch is authorized by this continuation request. The existing model remains disabled. Latest production SHA35f5ae376fb34f3f9d024753cfa6ec2aa4dbc71d823e19cfdfae0ce8bad619d8.

## Historical native cop state — September 11

Added typed local capture/seed APIs for cop rules and four native cop posts. Capture is rejected during replay; seeding is rejected outside ReplayScope and only replaces the replay shadow. Actual-code checks verify old siren and enabled-mode states can be replayed after live state changes, without reverting the live siren or mode. Production compilation passes, SHA256 35f5ae376fb34f3f9d024753cfa6ec2aa4dbc71d823e19cfdfae0ce8bad619d8. These APIs are not yet attached to a native replay executor or journal timeline. Four-post layout remains unchanged; this does not solve 14-player cop parity.

Runtime inspection confirms events.cpp starts independent graphics and SP task threads using the same RDRAM pointer; task_thread_func calls rsp::run_task with it. A full-memory journal must not be connected to live capture merely because the game-thread hook is at a simulation boundary. Thread ownership/synchronization or a narrowed immutable dependency set still needs implementation. No runtime libraries were changed.

Authority remains disabled; native replay, complete host correction schema, cadence handling, full outcomes and activation are unfinished. No game launch or test ZIP. Latest local source checkpoint: analysis/release-1.2-online/authority-migration/protocol23-cop-history-checkpoint.json. Not uploaded.

## Prediction journal checkpoint — September 11

The dormant guest-memory journal now passes sparse retention, dense budget exhaustion, acknowledged-baseline replacement, wrong-round rejection and sequence overflow checks. Its immutable 4 KiB pages share unchanged data; it never silently discards pending input history. A separately constructed corrected branch can replace the live journal after a replay ticket commits. Synthetic sparse captures measured approximately 0.18–0.25 ms each on this host; these are not race performance measurements.

IMPORTANT: this journal is not connected to live gameplay. An 8 MiB guest-memory snapshot does not capture native C++ state and is not automatically coherent with other emulated/audio/render threads. Do not copy the complete journal back into live RDRAM: unrelated world, audio and resource state may have advanced. Resolve capture ownership, historical cop state, authoritative field coverage and selective correction before implementing native replay. The production executable remains protocol23 with authority disabled and the hybrid gameplay path unchanged. The native replay executor, cadence handling, complete outcomes/14-player cop parity and lifecycle activation are unfinished. No game launch or new test package.

Thirteen offline checks now pass; the journal adds to the twelve recorded checks. Production identity remains b32639dd50b01b0b9fff678291f07b0c517f634f2ebf170d8b6710e130332ab3; the unused journal header does not alter it. A local source checkpoint is recorded in analysis/release-1.2-online/authority-migration/protocol23-journal-checkpoint.json. This new checkpoint has not been backed up off this computer.

# Host-authoritative online migration

User approved September11. Keep Host/Join, local multiplayer, assets and renderer unchanged. Replace client-owned movement/contact proposals with host-owned simulation; never run competing authority paths. This is implementation work, not a published build.

## Implemented foundation

`rr64_authoritative_input.hpp`: one command per native simulation step; bounded256-entry history; sequence/round validation; host staging separated from post-simulation acknowledgement; retransmit oldest outstanding and newest commands together so brief taps survive loss without fresh controls waiting for an ACK; transactional generic reconciliation restores host state and replays outstanding commands. Overflow fails explicitly instead of erasing history. No arbitrary60Hz conversion.

Tests pass for reordered/missing input, contradictory duplicates, acknowledgements before simulation, stale races/snapshots, future ACKs, replay failure, history pressure and9999 ring-wrap steps. These use a synthetic state model, not the game's physics.

`rr64_authoritative_controller.hpp`: scoped slot0 adapter for native40664. Saves controller arrays and actor+4 controller index, supplies held/changed/pressed buttons and stick bytes, then restores byte-identically. All14 roster positions pass offline memory tests. This is not yet installed in generated hooks.

## Native trace and next integration

40664 reads actor+4 as controller index, actor+E0 as bike, CD98 changed buttons, CD90 held buttons and CDA0 stick input. 6AFFC calls it at6B1FC only on the human branch selected by actor+26; actor+108 callbacks run beforehand. Changing the controller lookup alone will not make the host drive the replicated riders beyond four. Trace/replace the human driving callback selection without changing canonical actor IDs, cameras or local gameplay.

Wire commands to authenticated slot ownership. Stage once per native step; acknowledge only after that step completes. Missing commands must not cause unlimited held acceleration or fake acknowledgement; define bounded waiting/recovery before activation. Include redundant oldest pending commands without reliable ordering of ordinary snapshots.

Host must capture all human and AI states and reject client position/health/hit claims. Clients receive their own authoritative state too (remove self-echo suppression in that model). Native hit handlers execute on the host only; remove protocol21 contact proposals when activating this path.

Prediction needs a complete replay-safe native movement state and suppressed external side effects. The generic reconcile helper is not such an implementation. Keep collision/world/RNG dependencies explicit; do not replay arbitrary full frames or merely restore position. Remote interpolation and local error correction remain presentation-only.

## Delivery gates

Helpers currently have offline coverage only. Production still uses protocol21 hybrid behavior. No new game build/ZIP or launch for this milestone. Required before player testing: command transport, all14 driving paths, host ownership assertions, replay isolation, native build, loss/reorder tests and crash/eject/collision regression checks. No claim of complete prediction or solved desync until live acceptance.

## Host round coordinator implemented

rr64_authoritative_round.hpp adds14 isolated queues, atomic batch validation, host-only pause masking, immutable staged inputs and post-step acknowledgement stamps. Missing controls hold for two native steps then become neutral; no missing command is acknowledged. It does not yet choose live simulation cadence or replay a native rider.

Three offline targets pass. Sustained in-process test generates600 commands for each of14 peers (8400 total), drops every seventh send, delays/reorders batches4-8 synthetic steps and delays ACKs6steps. All commands eventually commit once with empty client histories after draining. This is not a real UDP/native game test. The newest/oldest resend windows are both necessary; oldest-only retransmission can gate fresh input behind delayed ACKs.

Further native trace: actor+110 is initialized to4090C at6CDA8. A later transition at6ECC4 selects actor+108 as4EAD0 for the nonzero+26 branch or5264C for the human branch. Actor+10C also receives40558 for state-specific overrides. Do not permanently overwrite these callbacks to force human control; they include race-state transitions requiring further tracing. No generated hooks or live network switches have been installed for authority yet.

## Native control translation bridge

rr64_authoritative_native.cpp now calls original40664 with ControllerScope around that call only. The real ABI and caller-provided output pointers are retained. This translates controls; it does not run bike physics or acknowledge input. It is compiled into production but no generated hook activates it.

RR64AuthoritativeNativeSmoke passes for14 slots with a mocked40664, checking scratch controller selection, native output/register propagation, exact controller/actor restoration and wrong-round rejection. Production links against the actual generated40664 successfully. Neither proves live driving behavior.

Additional callback trace:5264C calls5980C(bike) and returns. 524CC starts with5980C and adds4F494 and further logic. Do not treat these as interchangeable full simulation steps. The active actor loop subsequently calls+10C overrides,404BC when rider+5C4 is active, then+110. Crash/recovery/override behavior must stay in that pipeline.
## Authority transport checkpoint — September 11

Protocol22 now includes authenticated sequenced input transport and an internal host-authority switch. No live caller enables that switch: gameplay still uses the hybrid model. Fixed host packet dispatch previously misplaced inside the Hello branch; normal authenticated input/hit packets now reach their handlers. Host-local commands use a separate sequence counter, avoiding a self-acknowledgement history overflow. Authority-mode combat runs native decisions on the host only and bypasses owner proposals/replay.

Seven offline checks pass (input history, controller scope, round coordinator, native translation bridge, real UDP input channel, combat ownership, hit channel). Channel checks cover foreign session/endpoint rejection, stale rounds, host-only pause masking and 600 host steps. Production compilation passes, EXE SHA256 c0bf3cbdf2f3700257a862befe776b49fe1e9d30b08640c96f0c0707b885ee7c.

NOT COMPLETE: live all14 host driving, authoritative snapshots including the local player's processed-input acknowledgement, replay-safe native prediction/reconciliation, and complete outcomes remain unintegrated. Do not label this an enabled host-authoritative candidate. No game launched or distributable package created. User requested completion and packaging; that request remains outstanding.

## Coherent authority frame channel — September 11

Working protocol23 adds a dedicated host frame channel: all14 rider records plus one completed simulation tick and all14 processed-input acknowledgements. Five independently decodable bounded datagrams form a frame; an eight-frame bounded assembly commits only once all parts agree. Missing, reordered, duplicate, stale and conflicting-stamp parts are checked through the actual client dispatcher. Local player state is included, and receipt never retires predicted inputs. Host publication rejects premature ticks, fabricated acknowledgements, nonfinite active states and human/AI ownership conflicts. In authority mode, old client-rider sends, host incremental snapshots, client snapshot application and local/AI publication APIs are suppressed.

RR64AuthoritativeChannelSmoke and RR64HitChannelSmoke pass; RR64NetplaySmoke compiles. Production compilation passes. EXE SHA256 7bb90d8c33d954d81209271f4f2d9593d69ccb595680a65bf4b02eb8b1132799. Tests validate transport/assembly, not native gameplay or complete crash pose. No game launch or package.

Still outstanding: enable only after native all14 host driving, complete host frame capture at the post-simulation fence, and replay-safe client reconciliation are integrated. Authority remains dormant; ordinary gameplay remains hybrid. New frame transport is not a claim that the existing RiderState contains every replay dependency. User's completion-and-package request remains outstanding.

## Native host step and capture checkpoint — September 11

Protocol23 now has generated6AFFC simulation-boundary hooks and a40664 input hook. Authority remains explicitly dormant (no live authority_start caller); local/hybrid gameplay remains enabled until client reconciliation is ready. Host commands are staged at the native update entry and acknowledged only at6B678 after every active human reached40664. Completed host capture follows canonical actor links for all14 records and rejects invalid counts, duplicate bikes/riders, broken reciprocal links or incomplete captures. Atomic authority frames now feed existing remote rider getters/presentation. The authority host no longer applies remote snapshots over its simulation. History resets on authority/round changes.

Native control routing redirects only call registers:4EB6C ->524CC and4EAD0 ->5264C at6B1C8;6B1D4 enters the existing human pipeline;515C4 start override ->40500 at6B224. These pairings are derived from native initialization6CC94/6CCC4 and transition6ECC0/6ECB8. Actor identity/controller/classification/callback fields are not permanently rewritten. Unknown callbacks or missing translated humans prevent acknowledgement. Further callbacks, full native crash lifecycle and all14 cop behavior still require validation.

Ten offline targets pass. New checks: RR64AuthoritativeStepSmoke (boundary/capture with native callee mocked), RR64AuthoritativeGeneratedSmoke (84 comparisons using actual generated40664/math helpers and synthetic coefficients), RR64AuthoritativeActorSmoke (actual generated6B17C..6B2C4 control-loop fragment and40664 for2/4/14 humans, with physics/override/reaction callees mocked). extract_control_fixture.py refreshes the copied exact generated fixtures after regeneration. These are NOT live race or full physics tests. Production compilation passes. EXE SHA256 acc4a46c5c4b95c6375e19cc055a0af8b7bbbdb54cd924c291c8af4318c907a5. No game launch or distributable package.

Client reconciliation remains unfinished: no input-history retirement or native prediction replay is connected, so do NOT activate authority yet. Current RiderState is not a complete replay-state schema. The direct-call audit (analysis/release-1.2-online/authority-migration/prediction-call-audit.json) finds57 functions reachable from4090C,23 from5980C,60 from45960,12 from36948. Cop posts/shout/siren/trick, bust/notification and achievement hooks are non-guest-state effects that replay must isolate. This audit does not resolve memory aliasing or prove all indirect paths. Preserve native world/collision dependencies; do not replay arbitrary full frames or position-only corrections.

Outstanding delivery: replay-safe local prediction/reconciliation with processed-input ACKs and cadence handling; complete authoritative outcomes/pose and14-player cop parity; lifecycle activation and offline connection/regression checks; then package for the user. User asked to continue until usage runs out; no ready was given, so keep the game closed.

## Client replay handoff and side-effect isolation — September 11

Protocol23 production SHA256 b32639dd50b01b0b9fff678291f07b0c517f634f2ebf170d8b6710e130332ab3 compiles successfully. Added authority_prepare_replay/authority_commit_replay: copy the host frame and exact outstanding inputs under the network lock, run native replay outside the lock in isolated memory, and retire history only after the issued ticket succeeds. New local input invalidates an older prepared replay; newer host frames may arrive without invalidating an otherwise coherent issued replay. Tickets are not reused across session resets. Dispatcher checks pass for concurrent input, newer frames, duplicate commit, old tickets, and round changes. No native caller invokes this replay yet.

rr64_prediction_replay.hpp supplies a non-nestable thread-local isolation scope. Custom cop rules and runtime posts use private per-replay copies; achievement game events are suppressed during replay. Actual cop runtime tests prove a replayed long-press can toggle only the shadow siren, while ordinary long-press still toggles live state. Actual cop rules tests prove replay cannot advance the real win timer or reset the real mode. This isolates registered C++ effects only: caller must supply isolated guest memory and the correct historical native state. It is not full prediction replay or a complete side-effect proof.

New passing targets RR64PredictionSideEffectSmoke and RR64PredictionCopRulesSmoke supplement the prior10 checks. Production compilation passes. Dependency audit additionally identifies sprintf_recomp, math, switch_error and do_break in direct closures; do not assume the graph proves replay safety.

Still outstanding before activation/package: complete native movement/crash state journal and replay executor, correct historical cop state for replay, input cadence/backlog reconciliation, complete remote pose/outcomes and14-player cop parity, and lifecycle activation. Native host step/routing/capture and remote frame channels from the preceding checkpoint remain implemented but dormant. No game launched; no downloadable test candidate created. User asked to continue until usage runs out; most recent limit reading was96% used, not exhausted.

