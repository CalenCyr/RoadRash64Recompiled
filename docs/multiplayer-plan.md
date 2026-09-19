## Next candidate — connection and Controls changes, September 13

Body-motion trace completed: analysis/release-1.2-online/body-motion-field-trace.md.
Native34370 proves rider+B4 and bike+194 are integrated positions, despite C++
velocity names. Embedded bases rider+28/bike+108 are critical. Current velocities
are rider+98/bike+178; updated velocities rider+C0/bike+1A0. Campaign01 body_v*
must be interpreted as integrated position. Production named-field uses found
are raw synchronization/hash/validation, not speed arithmetic. No physics fix
or online-jitter root cause established; no runtime change made in this trace.

Campaign01 live capture reviewed: session20260917-073457-fc03d302 matches EXE,
exit0, 190030 rows, no loss/malformed rows. See analysis/release-1.2-online/
campaign01-20260917-review.md and JSON. Native rider+B4/B8/BC is labeled velocity
but appears position-like; do not infer rolling speed from that field without
tracing semantics. Ten rejected traffic snapshots are exclusively initialization
frame0/pre-update, not ongoing race failures. Preserve invalid vs empty distinction.

September17: user requested a whole-campaign logging package for later online
diagnosis. Campaign Logging01 is packaged under analysis/release-1.2-online.
Opt-in RR64_CAMPAIGN_LOG=1 admits offline live-race samples, all native racer slots,
at up to10Hz. Existing asynchronous sync writer now includes separate bike/body
positions, body velocity, attached/ejected flags and traffic count/hash. Campaign
row limit10million; ordinary online limit unchanged. No private replay enabled,
no native state writes, no automation. Production build, launcher syntax and ZIP
CRC/file hashes passed; live capture remains unverified. Not proof of online
correctness, collision/terrain reproduction or LOD rendering. Candidate22 preserved.

After Candidate21 feedback, simplified player cards: Input device, In-game name,
Customize Controls. Saved profile selection moved to a "Saved layout" dropdown
inside customization. Switching layouts still assigns the selected player's
device-specific profile and refreshes bindings. Production build passed; dependency
patch updated. This UI revision is not in the preserved Candidate21 ZIP and has
not been visually tested. Packaged September15 as Candidate22 on user request;
ZIP CRC and manifest file hashes verified. Prior packages preserved.
ZIP: analysis/release-1.2-online/RoadRash64-Online-Fix-Candidate-22-Win64.zip
EXE SHA256: 7126166624402c8beb70b5cfd94e086d4e7560d7fa6e2bcaa468c64698136e34

Candidate21 packaged on user request; ZIP CRC and every packaged file hash verified.
ZIP: analysis/release-1.2-online/RoadRash64-Online-Fix-Candidate-21-Win64.zip
EXE SHA256: 76f67a926b4b1bfde1d66673a1681eb5adadf6cd318fbfa60ae9afef893da2b6
Active production executable rebuilt, not launched. Unanswered
joins now time out after five seconds with "Failed to connect" and clear slot,
socket and player state. A non-host departure/timeout leaves stable actor slots
and neutral input instead of ending authority. Host departure behavior retained.
Departed riders are not converted to AI or automatically eliminated yet.
34 offline checks passed (protocol35-checks-1789355401671003600.json), including
loading/racing departures and unanswered join cleanup. This is not live acceptance.

Controls player cards now offer Mouse & Keyboard and connected pads in an Input
device dropdown. Occupied pads swap assignments; explicit keyboard assignment
survives controller refresh. Removed the Gameplay keyboard toggle and its unused
state. All binding rows display short descriptions without requiring hover.
Descriptions use the standard game actions; cop L tap/hold and dedicated actions
reflect current source. Reference for stock meanings:
https://gamefaqs.gamespot.com/n64/198491-road-rash-64/faqs/46566 (Controls).
Frontend edits are retained in the pinned dependency patch; reverse check passed
with Windows whitespace normalization. No UI/game launch performed.

Unresolved: client-local jitter (host-only log cannot measure it), pedestrian
distant pose distortion, and the reported early-join menu corruption beyond an
unanswered join. Do not claim these are fixed by the timeout message.

## Online Candidate20 — startup admission guard, live acceptance pending

Candidate19 paired logs 20260913-192326-51cb9a2c (host) and
20260913-192012-44f05ac3 (client) verify identical EXE identities, successful
handshake/selection and host-side "input admission failed" twice. Both processes
exit0; this is an intentional authority stop, not evidence of an OS crash or
basic reachability failure. Logs do NOT distinguish release/identity/queue/input
validation rejection. Do not claim the precise cause is proven by these logs.

Candidate20 additionally gates the actual native6AFFC entry on authority loading
release, returning before input/native work when waiting. The dispatcher barrier
remains. Source config and generated hooks agree. Admission failures now log their
specific branch and state, rather than only a generic disconnect. New negative
step fixture checks waiting consumes no input and raises no failure; all34 pass
in protocol35-checks-1789353174524146900.json. Production build succeeds. No game
launched; user will test Candidate20. All Candidate19 body voice settings retained.

## Online Candidate 19 — voice always uses rider body

User requires voices always originate from the rider body. Removed all bike
position fallback from both speaker and listener distance calculation. Invalid,
inactive or missing body positions silence playback and reset motion history.
Candidate19 compiled and packaged without launching game/microphone. Candidate18
and earlier packages preserved. Protocol35, previous voice settings retained.
ZIP: analysis/release-1.2-online/RoadRash64-Online-Fix-Candidate-19-Win64.zip
Live voice acceptance remains pending user testing.

## Online Candidate 18 — proximity voice, no in-game test

User requested proximity voice with mic settings, then crash fly-by effects,
without in-game testing. Candidate18 retains protocol35/authority behavior and
adds Audio settings: device dropdown (persisted by SDL name), mic mute, input
gain, voice activation threshold, voice output volume, fly-by strength.
SDL audio initializes before config enumeration; actual microphone capture
remains confined to enabled online races. Device changes reopen on the voice
worker; new devices require restart to refresh the dropdown. No mic was opened
or game launched during this work.

Spatial voice follows rider positions (including detached bodies), pans against
the listener bike heading and applies a bounded pitch shift from sampled radial
motion. Discontinuities/long gaps reject pitch changes. It is a presentation
effect, not altered crash physics. No wall/terrain obstruction muffling or echo
cancellation is implemented; no suitable ready native obstruction API was found.

Production build and offline proximity/pan/gate/pitch checks passed. Real mic
capture, directional perception, device UI and online sound require user testing.
ZIP: analysis/release-1.2-online/RoadRash64-Online-Fix-Candidate-18-Win64.zip
SHA256:55dcaf802a8aa21f1491c0920a049d23e6a5c432e932f9743f2d0c6f73b59c96
EXE:2938d4b765eda756158f5422d7f0af0a422d6ffc8811cd2674e93327c7f0b23f
No publication. Candidate17 preserved. All testers should use Candidate18.

## Online Candidate 17 — September 13, prior handoff

User requests ending the expanding demo-verification loop, finishing a playable
candidate, then stopping automated testing for their own online test. Candidate17
is packaged in analysis/release-1.2-online/RoadRash64-Online-Fix-Candidate-17-Win64.zip.
Protocol35 prevents earlier dormant-authority builds from joining this candidate.
The stock post-initialization race dispatcher now starts host authority and holds
guests until the round announcement and existing native roster/loading barrier.
Local/offline paths retain their earlier behavior. No publication authorized.

Production correction history now retains bounded ordered streaming inputs and
worker ownership evidence between updates. Replay runs those original loading
operations before the following order/update. Overlap, overflow or invalid capture
rejects the interval; no live payload pointers are copied. The cross-thread mailbox
is not held while obtaining the guest snapshot, avoiding a renderer/snapshot lock
cycle. This is conservative and can defer correction under unavailable history.

All34 checks pass in protocol35-checks-1789313717809030000.json; production build
succeeded. ZIP integrity/manifest checked. EXE SHA256:
0ad06fe52dbfbd3b7b8423a4bb5a939a8726a47e6c8205c8227790f46d60aaba.
This newly activated path has NOT been verified in a live two-PC race. Do not call
it stable or claim full multiplayer acceptance. User will test next; do not launch
additional automated demo/network sessions unless subsequently requested.

## V30 live automated two-mode checkpoint

V30 launched successfully under standing authorization; previous approval block
is historical. Session20260913-055617-35492fd2 verified Big Game and Thrash
sampled native replay:808 matching comparisons including64detached/32remounted,
336traffic comparisons, zero recorded RNG/motion/native failures or rejections.
Weapon swing and eject observed; no confirmed hit/finish/cop/WAN coverage.
See verification/reports/v30-two-mode-checkpoint.json and current handoff.
V30 remains paused in Thrash; online authority still dormant and incomplete.

## V29 live checkpoint; unattended V30 prepared

User authorizes all future private versions for automatic testing, including
close/relaunch, until revoked. Latest run392/392 sampled comparisons match,
272 with traffic,49 depth8, zero rejections/RNG differences. Mounted-only coverage
does not resolve all historical RNG/traffic failures. V30 developer controls add
the ordinary eject request once per serial; build/control checks and real saved
control replays pass. V30 remains unlaunched because automatic approval review
again rejected closing paused V29 despite direct authorization. See the newest
UNATTENDED-ONLINE-HANDOFF.md entry. No authority activation or online completion.

## V29 traffic constructor identity candidate; launch blocked by review

V28 closed: 904 matching comparisons, 224 with traffic, 113 depth-eight matches,
zero RNG differences, 21 ambiguous-model rejections. Actual saved-case tables
show graphics indices shared by distinct vehicle dimensions. V29 uses the exact
dimensions retained by the native constructor to filter model candidates;
unresolved ambiguity still rejects. Build and all34 rebuilt checks pass; saved
control replays pass. Independent package hashes/CRC/exclusions verified.
See UNATTENDED-ONLINE-HANDOFF.md latest section for identities and evidence.

Automatic approval review rejected V29 launch twice despite the current user's
direct standing ready override and fresh no-process/hash checks. No workaround
used; V29 remains unlaunched, game closed. Platform approval is the live-testing
block. Authority remains dormant and the full online candidate is unfinished.

## Standing ready override and V28 private testing

The user directly authorized unattended private launch/control/close/relaunch
until revoked. Prior ready blocks below are historical and superseded. See
UNATTENDED-ONLINE-HANDOFF.md V28 section for active sessions, source correction,
build identities and test limits. All 34 rebuilt checks pass. One real saved
control case matches under both offline baselines. V28 removes incorrectly
classified road-steering guards and has over 600 matching sampled comparisons
at the inspected checkpoint; off-road/no-traffic coverage is limited. V23 RNG
divergence remains unresolved. Authority remains dormant; no publication.

## Verification26 prepared; unattended continuation blocked by approval review

See UNATTENDED-ONLINE-HANDOFF.md continuation section for exact sessions and
build identities. V25 never reached a visible game or recorder output. The
approval reviewer rejected stopping/relaunching its windowless process because
it did not accept carried-over authorization over AGENTS.md's ready gate.
Further game control requires direct confirmation in this task.

Fixed saved-case FrameInput sequence capture to happen after continuation
depth assignment. Production builds; all34 rebuilt offline checks pass in
protocol34-checks-1789281260153665200.json. V26 archive and manifest validated,
no ROM/cases/logs included, not launched. This fixes diagnostic fidelity only.
No real saved cases, no RNG resolution, no authority activation or publication.

## Verification23 reviewed; hidden RNG divergence; V24 diagnostic awaits ready

Session20260912-230221-8f555b49 exit0 at2026-09-13T06:05:16Z.
464movement comparisons allmatch,58full8stepmotionmatches,384traffic,
406streamcompletions,zero rejections,allnativeequal. BUT65RNGdivergences.
Example34depth2 seed6443ccca equal before/afterorder andbeforeupdate, afterupdate
live5732d5a1/private922ffd67. Example106 live f7c8bdbb unchanged acrossupdate,
private4cec9da5. Extra private draws arise INSIDE sampled update. No reseeding
or tolerance applied. Earlier terrain fixes retained, driver detail acceptedV22.

V24 diagnostic adds59originalJAL callsite hooks to RNG distribution calls in
private closure, excluding nested wrappers. Fixed256entry live/private buffers,
firstunequal site/seed and counts/truncation printed onlyon disagreement.
No percallIO, no altered seeds. Live recording onlywhilepending, private only
inside replay_native_frame scope. No claim actual RNGbug fixed yet.
Generator analysis/release-1.2-online/add_rng_call_probes.py maintains configblock.
Activeconfig regenerated; protocol34-rng-callsite-build.log productionpassed.
All33 protocol34-checks-1789279716080005400.json passed. Parser fixtures pass;
report now preserves RNGdivergences/call differences separately from motion.
ReparsedV23 results includes65rngdivergentattempts; not a complete replaypass.
Skill updated. Authority dormant; no production historyjournal integration yet.

V24 package archiveCRCverified, no launch. Awaitready.
ZIP SHA256 cc33cfd43ba1516d01b722be064726da600e92a0d65e0a773f14502968bcec7f
EXE SHA256 2ce9a9da68a35d419b5082d37fa5997a2631dae26d413e450ea245261f55c70e
Next run same Big Game traffic/combat/crash/eject/recovery. Afterdone find FIRST
callsite discrepancy when beforeupdate seedsmatch; trace that routine and fix
without askingcontinue. Do not chase laterseed mismatches as independentcauses.
Historical rider-links/traffic-model rejections not reproduced here remain noted.
## Verification23 launched after ready

Session20260912-230221-8f555b49 started2026-09-13T06:02:21.7597507Z.
PID29120 responding. Identity matches694362f795eff659992bf4b8c91e18ea29e9dea5d870542c2aa8d9c70abd761d.
Consolidated/native replay enabled. Runtime results pending; ready consumed.
On done review RNG boundary logs and specific rider rejection reasons, then
apply supported fixes autonomously. Do not relaunch without another ready.

## Verification22 reviewed; driver detail accepted; Verification23 awaits ready

User confirms driver detail correct. V22 session20260912-225413-4470d5a8
exit0 at2026-09-13T05:57:14Z.372comparisons371matches45full8-stepmatches,
307trafficcomparisons,326streamingcompletions,0streamingfailures. Allnativeequal.
16rejections:12rider-links and4guardedraceboundaries; no traffic-model ambiguity
in this run (does not resolve earlier ambiguity universally).
Singlefailure106 attempt108 depth8 mounted:slot6 rooterror0.00146484,
velocitydifference~0.094 and rotationdifference~0.0036. Terrain89queries0different.
Dependencyfirsthit RNG8009DC30 at1A508, plus other differing reads. Root cause not
proven. No tolerance or live seed overwrite added. Source native1A500 uses LCG.

V23 adds bounded mismatch-only RNG pairs before/after privateorder and frame,
splits rider-links/model/character/duplicate rejection reasons. This is targeted
DIAGNOSTICS, not a claimed new movement fix. Preserve V22 fixes and visualacceptance.
Need runtime RNG boundary evidence before deciding how omitted external RNG calls
should join historical execution; do not mask internal divergence by reseeding.
All33 passed protocol34-checks-1789279197497684400.json. Production
protocol34-rng-boundary-build.log passed. Skill updated. Authority dormant.
V23 package archive CRC verified; no launch. Await ready.
ZIP SHA256 5c938dba89d1ac5c59e67b77c8a3673523f570b1f45a1e99381d7ceab2b3ab2e
EXE SHA256 694362f795eff659992bf4b8c91e18ea29e9dea5d870542c2aa8d9c70abd761d
User should do combined Big Game traffic/combat/crash/eject/recovery session.
On done inspect RNG lines and specificcapturefailures then apply supportedfixes
without waiting for continue. Do not claim full online integration/acceptance.
## Verification22 launched after ready

Session20260912-225413-4470d5a8 started2026-09-13T05:54:13.1772780Z.
PID20284 responding. Identity matches expectedc595deff25c2e1a560b4fe61b4f1d63c23580cd261eab373c800e3afc32787dd.
Consolidated/native replay enabled; runtime results pending. Ready consumed.
On done inspect schedule completion/rejection, native component logs and visual
feedback, then continue supported fixes. No relaunch without another ready.

## Verification21 reviewed; Verification22 prepared, await ready

Session20260912-223934-29db7eff exit0 at2026-09-13T05:45:15Z.
720comparisons,717matches,87full8-stepmatches,608withtraffic.
630streamingcompletions,zero streamingfailures: cross-worker handoff now active.
28rejections:25traffic-model-ambiguous,3guardedraceboundary. Not allnativeequal.
158depth6 trafficid6 model225/231 and motiondifferent, roadpositionequal.
375depth8 2actors tinyrooterror0.00233459 and native-equal0.
382depth7 2AIheight errors158.62,cell1016 live-resident/private3-null,
entry-different1 changed-during-replay0. Preserve all, no online acceptance.
User distant LOD confirmation requested asynchronously; no answer yet. Do not
claim visual fix held based only on replay logs or saved MAX LOD=true.

Implemented bounded background worker catch-up at streaming entry: capture live
ResourceInventory task masks, drain original private worker until same free/queued/
priority masks atC814 safe task boundary. No payload or completion copies. Held
jobs/unreachable inventory reject; nonblocking receive remains nonblocking.
Synthetic resource test verifies first task completes, second remains pending
with untouched payload, repeated target creates no duplicate completion, resume
finishes second, backward target rejects. Existing unlimited drains unchanged.
Production history journal is NOT integrated; this remains private verification.
Added native-state component/eject mismatch logs and schedule rejection masks.
Traffic definition ambiguity remains guarded, not silently accepted.

All33 tests passed protocol34-checks-1789278570630089300.json (exit0).
Production protocol34-observed-resource-schedule-final-build.log passed.
Skill updated with runtime handoff evidence and offline schedule evidence/limits.
Verification22 package archive CRC verified; no launch, ready required.
ZIP SHA256 16391726cb528629494c5409dfe6f065af2a2d786cafe8a3446bd17bacdd5e92
EXE SHA256 c595deff25c2e1a560b4fe61b4f1d63c23580cd261eab373c800e3afc32787dd
Next consolidated Big Game run checks traffic/terrain,crash,eject,recovery.
Authority dormant; publication mirror untouched. On done diagnose AND fix without
waiting for continue. Do not treat sample rejection as a matching replay.
## Verification21 launched after ready

Session20260912-223934-29db7eff started2026-09-13T05:39:34.6286959Z.
PID27316 responding. Identity matches expected935f165c41be125501f0903d6dbf61d371666e2ff066b18d1002d23fb9c82500.
Consolidated/native replay enabled. Capture results pending. Ready consumed;
do not relaunch. On done review streaming completions and LOD feedback then
continue supported fixes autonomously.

## Verification20 reviewed; Verification21 ready, not launched

V20 session20260912-222635-d704940d exit0 at2026-09-13T05:31:37Z.
512 comparisons,510 matches,63 full eight-step matches,355 traffic comparisons.
Native shadows all equal. Two traffic-model-ambiguous rejected attempts.
Mismatch316 remounteddepth4 AI rooterror84.3359, cell677 private3/null;
mismatch400 detacheddepth4 extra private traffic13/14, cell823 private5/resident.
Both terrain entry-different1,changed-during-replay0. No streaming completion or
failure lines: V20 did not establish coverage of its new streaming stage.
Saved graphics.json has rr64_max_lod=true, drawdistance85. User reports distant
LOD regression. Private generated constructor11988 includes both LOD and world
allocation callbacks; their unguarded map() resets the live store on private
memory identity. Added prediction guards to these hooks and lifecycle invalidate/
pool/preparation hooks before touching bindings. Visual recovery needs acceptance.

Streaming callback runs in rendering, while continuation was thread-local on
simulation worker. Replaced with mutex-protected ownership; before takes it,
after publishes it, streaming holds lock while operating on private Resources.
Recursive private hook returns before lock. No live memory result installed.
This is verifier integration only, not production history journaling.
All33 offline checks pass protocol34-checks-1789277731014722300.json;
production protocol34-stream-handoff-build.log passed. Existing offline checks
are not a visual acceptance or live streaming coverage proof. Skill updated.

Verification21 ZIP packaged with archive CRC verified; no launch, await ready.
ZIP SHA256 2c8d01affac98179ea9a24c87e65117ea50a5b344bcac2bbebb111dad73cd33c
EXE SHA256 935f165c41be125501f0903d6dbf61d371666e2ff066b18d1002d23fb9c82500
One combined Big Game run: distant rider detail, traffic, crash/recovery,eject.
Watch streaming completions/rejections and subsequent comparison agreement.
Authority remains dormant. Do not publish or call this completed online sync.
## Verification20 running; reported distant LOD regression

Ready consumed: session20260912-222635-d704940d started2026-09-13T05:26:35Z.
Identity matches expected294a81ec84c20254922d68bbcc1a4871fb07494316b914c46ccaff35a5a9420e;
PID26220 responding. Consolidated/native replay enabled. Do not relaunch.
User reports distant LOD regressing during this run. Treat as unresolved visual
regression alongside replay acceptance, not acceptable diagnostic behavior.
Package wrapper sets replay options but does not explicitly disable MAX LOD.
UI default remains true; effective saved setting and live rendering path not yet
verified. MAX LOD changes after game start require application restart in current
code. Do not conclude a setting explains the symptom without evidence. Preserve
this session; after done review streaming side effects/render state isolation as
well as comparisons. No live settings/source changes applied during the run.
## Verification20: private native streaming stage integrated; await ready

Added StreamingInput and replay_native_streaming running original7B8D4 against
private Resources. Live entry hook only acts with verifier enabled and a retained
continuation. It captures CPU/FPU and six scalar inputs:771C/7720/7724 camera
selection,9DBAC/9DBCC map-range factors (1677C witha0=0),1830 clock. No resource
pointer, queue, allocator, descriptor or payload is imported. Native stage
retains its own triple cell lists and processes load/unload/fixup through private
queue services. Rejects unrepresented workers, bad contexts and bounded-step
failures; discards continuation on failure. Recursive private hook exits.

This is verifier continuation integration only. Historical online replay does
NOT yet journal streaming calls/scalars. Validate native streaming equivalence
before extending the production history contract. Authority remains dormant.
Current map-range setting hook is unchanged; historical settings transitions
are not claimed covered. Native streaming inputs may have additional dependencies;
this is a candidate, not proven resolution of V19 terrain mismatches.

All33 passed protocol34-checks-1789276964998243800.json. Streaming scalar tests
cover private restore, retained entry, invalid slot/NaN and atomic rejection.
Active config regenerated (v19-streaming-recompile.log); production builds
protocol34-streaming-build.log and protocol34-streaming-final-build.log passed.
Includes prior exact traffic descriptor alias fix. Package integrity/all manifest
hashes verified. No launch. Await explicit ready for consolidated Big Game run.
Package analysis/release-1.2-online/RoadRash64-Authority-Verification-20-Win64
ZIP SHA256 31784d957e8e15c55e90b086f9a8d6abb9aa8f6ad12038ff7675ffed4d133c53
EXE SHA256 294a81ec84c20254922d68bbcc1a4871fb07494316b914c46ccaff35a5a9420e
Watch RR64-REPLAY-STREAM completions plus streaming capture/replay failure lines;
normal after-update comparisons remain the acceptance evidence.

## Verification19 reviewed; exact-definition traffic aliases fixed

Session20260912-220815-00b65367 exited0 at2026-09-13T05:14:08.1304717Z.
679 completed comparisons,674 matches,83 complete eight-step sequences,
615 with traffic. Coverage528mounted/127detached/56remounted attempts.
No unregistered callback failures remain in this run.32 rejections:31 traffic
model ambiguity and one guarded race transition. Native shadow states equal.
Five differences:122/469/623 extra private traffic;317/339 AI contact/height.
All five have preexisting terrain residency disagreement.339 specifically
live5/resident vs private3/null, so an unload-only workaround is insufficient.
Do not claim a controlled improvement or complete online acceptance.

Audited constructor47668: dimensions and graphics reference same descriptor;
model index discarded at477FC, remote class supplied separately. Resolver now
canonicalizes only EXACT descriptor pointer aliases to lowest model ID within
one image. Distinct descriptors sharing graphics still reject. This differs
from the rejected class heuristic. Fixtures cover identical descriptor alias,
different-definition rejection, no output mutation and reason clearing.
All33 passed protocol34-checks-1789276546790997200.json; production passed
protocol34-v19-alias-build.log. Not yet proven that all31 runtime ambiguities
are identical-definition aliases. No new package or launch. Authority dormant.
Continue full streaming lifecycle work rather than blindly clearing cells or
copying live payload pointers. Existing capture retained as evidence.

## Verification19 launched after ready

Session20260912-220815-00b65367 started2026-09-13T05:08:15Z.
Game PID11948 responding. Wrapper identity confirms expected executable hash
and consolidated/native replay capture enabled. User testing Big Game.
Results pending; ready consumed. On done inspect capture and continue fixes;
do not relaunch without another ready.

## Verification19 prepared: observed callbacks and precise traffic rejection

V18 follow-ups compiled and all33 final checks passed:
protocol34-checks-1789269666552097500.json and protocol34-traffic-reasons-build.log.
Traffic resolver now reports unmapped, ambiguous, descriptor, table or group
failure rather than one opaque error. Retains strict atomic failure semantics.
Tested absent model, ambiguous descriptors and successful reason clearing.

Rejected attempt: using constructor vehicle class to disambiguate resource
aliases. OnlineTrafficSmoke exposed independent replicated model/class choices;
even a tie-break can select the wrong alias. Removed class logic entirely.
Do not reinstate without stronger identity evidence. Terrain audit located
7B8D4 eviction at7BE8C..7BF00 called6A74C/6AB30 between simulation updates.
Its render/streaming lists, queues and resource frees must be reproduced
coherently; no blind cell-state changes or live pointer import applied.

Verification19 packages observed callback fixes6BEE4/6BFF4/6C120 plus traffic
failure subtypes. Both remaining V18 terrain failures remain unresolved.
Archive integrity/all manifest hashes passed. No launch; await explicit ready.
ZIP SHA256 30e2790839f9f4af01472a1b30dbe9a89f325bbebedca41940967e67a6442e6c
EXE SHA256 d7246f99f03a9e6abfde152474e529ac6d41ddc6083eb83265611ffd3547cc48
Package analysis/release-1.2-online/RoadRash64-Authority-Verification-19-Win64.
Need live evidence for newly admitted callbacks and previously opaque model
failure; no claim the whole online model is ready. Authority remains dormant.

## Standing workflow: continue from verified findings into fixes

User explicitly requests autonomous continuation: after verifying evidence,
apply supported fixes and run appropriate checks without waiting for another
continue message. Give progress updates while working. Stop only for required
user input, a necessary live test, or completed work. Batch related fixes and
avoid repeated small live runs. Existing ready-only game launch rule remains.

## Verification18 reviewed; callback fix applied without another launch

Session20260912-200520-1f6528a2 exited0 at2026-09-13T03:08:23.9541323Z.
417 comparisons,415 matches,49 complete eight-step matches,334 with traffic.
Two mismatches:88 extra private carid1/model246;299 AI slot4 vertical/contact
state differs. Eleven rejected executions:four calls across missing callbacks
6C120/6BEE4/6BFF4,six traffic-model-resolution,one guarded race transition.
Native shadow comparisons all equal. Sampling spans153seconds; scenes/length
are not controlled against V17 so do not claim a quantitative improvement.

Audited and admitted the three observed callbacks (cop activation/roadside
traffic events) and their private dependency closure. Existing queue services
suffice; no expanded OS allowlist. Production passed
protocol34-v18-callback-build.log. Unknown callbacks still reject. Not live-tested.

Terrain phase records confirm both failing cells already differ at entry and
remain unchanged during replay:1020 and609, live unloaded/private resident.
This narrows the missing inter-update lifecycle; it does not authorize copying
live pointers or indiscriminate cell invalidation. Report parser now retains
TERRAIN-PHASE; report fixture tests passed. Traffic model resolution remains
unexplained; no relaxed validation. Authority dormant. No new package or launch.

## Verification18 launched after ready

Session20260912-200520-1f6528a2 started2026-09-13T03:05:20Z.
Game PID20388 responding. Wrapper identity confirms expected executable hash
and consolidated/native replay capture enabled. User running broad Big Game
check. Results pending; ready consumed. Do not relaunch on done.

## Verification18 batched fixes prepared; await ready

Private order explicitly seeds historical CopRulesState. Audited direct closure:
6E5E0 ->63B50 ->636B0 ->rr64_custom_cop_notification. Without a seed,
isolated_state cloned live race state on first use. Notification reads native
cop state and writes private guest HUD; no shadow output transfer needed here.
Achievement game-event already exits during replay. Regression exercises
historical cop notification while live mode disabled, then disabled historical
mode with no guest changes.

All33 offline checks passed protocol34-checks-1789268529600474400.json;
production passed protocol34-order-cop-seed-build.log. Verification18 combines
this with full native order, both timing restorations, worker initialization,
sprintf callback, traffic identity comparison and phased terrain evidence.
ZIP integrity and every manifest hash checked. No game launched.

Package: analysis/release-1.2-online/RoadRash64-Authority-Verification-18-Win64
ZIP SHA256 f983bf8cb1eba6055a99cbd744f5ff9c50612e9204505516de5283200de0f8b7
EXE SHA256 9bbed388ffb67cf396d7b413d78ecce34d7b49a49f2c43507e542e035b9d9dd0
One consolidated approximately8-minute local Big Game session: traffic,
combat, crash/recovery and eject. No second person. This tests the batched
changes against V17, not activated online authority or a release candidate.
Need explicit ready before launch. Use Start-Verification wrapper for capture.
Authority remains dormant; no proof yet that all nine V17 mismatches resolved.

## V17 follow-up: order timing, worker entry and terrain phase evidence

Full private order now restores the current entry timing before running, not
just before simulation. Native6EE64 reads physics delta8009CBA8; the preceding
private6AFFC may have left it halved. UpdateCounters also retains raw finite
float bits of order clock800A1818 (read6E84C). This is local history metadata;
protocol34 wire format unchanged. NaN restore rejects without mutation.

Private order initializes its own resource-worker CPU entry, using the same
stock stack setup as private frame. It no longer depends on a prior frame
transaction having populated thread-local worker_entry.

Terrain evidence retains live entry, private entry and private query descriptors.
New TERRAIN-PHASE lines distinguish preexisting descriptor disagreement from
changes during replay, ignoring epoch-only differences. This still is NOT a
live-query comparison and cannot prove which residency is correct. No live
terrain pointers or payloads are imported. Tests cover immutable entry state,
in-update changes and incompatible terrain layouts.

All33 rebuilt offline checks passed: protocol34-checks-1789268137147001700.json.
Production builds passed protocol34-order-clock-worker-build.log,
protocol34-terrain-phase-build.log and protocol34-order-entry-timing-build.log.
The final timing restore ordering was compiled after the suite; the suite
covers timing preservation, not execution equivalence of full native order.
No new package or launch. V17 remains the retained live baseline; authority
remains dormant. Streaming/lifecycle, correction and online activation gates
are still incomplete. Do not claim the nine V17 mismatches are resolved yet.

## Batched V17 follow-up: complete private race order and identity comparison

Private generator now retains the full native6E5E0 routine instead of stopping
at6E844 and reconstructing only neighbors. Omitted suffix includes recovery/
eligibility/callback changes, AI messages and RNG consumption. Calls6A254 and
6A380 reject the disposable transaction before race teardown/setup; those
transitions remain a distinct host-boundary integration task. Removed the
second refresh_order_neighbors application from production private order;
the original routine now performs that work. Generated dependency closure483,
OS allowlist unchanged; no unknown callback acceptance widened.

Verifier traffic comparison now matches active cars by race identity, not
compacted native roster slot. New rr64_traffic_comparison.hpp reports paired
indices, additions/removals and state differences. Duplicate identities reject.
Offline tests cover reordering to slot19, changed motion, identity replacement,
duplicate roster. Do not retroactively dismiss V17 comparison331: old logs lack
full matched-identity state needed to prove that was only compaction.

All33 existing rebuilt regressions passed after full-order change:
protocol34-checks-1789267464483923700.json. New traffic identity fixture then
rebuilt/passed separately (traffic-identity-check-build.log). Production passed
protocol34-full-order-build.log and protocol34-traffic-identity-compare-build.log.
These are compilation/offline gates, not proof all live V17 failures resolved.

Terrain probe detail: failures63/289/453/495/620 compare live ENTRY descriptors
state1/null against private QUERY descriptors state5/resident. Epoch-only
differences are excluded. This is not a same-instruction live/private trace:
do not assume private residency is wrong solely from that report or import live
payload pointers. Streaming lifecycle and inter-update AI state remain under
audit. No package, launch or new user test requested. Authority still dormant.

## Consolidated Verification17 reviewed; no new test requested

Session20260912-192538-1b0d3582 exited0.764 comparisons,652 with traffic,
90 complete matching sequences. Nine mismatches (four traffic, five AI rider),
three unregistered callback rejections. No traffic-capture rejection remains.
Mounted/detached/remounted and two eject triggers covered; other checklist
items are not independently proven by buttons alone. Sampling lasted267s;
do not ask user to repeat solely for not reaching approximate8-minute target.
Full evidence and remaining work: workspace analysis/release-1.2-online/
authority-migration/verification17-session-review.md and verification17-results.json.

Audited/fixed missing proutSprintf80099448 in private generator callback closure;
only private guest buffer writes. Unregistered callbacks now include addresses.
Production regenerated/compiled in protocol34-sprintf-callback-build.log.
Do not claim all three rejections solved without target evidence. Traffic
lifetime/scene-collision and AI state dependencies remain; no new package.
Use consolidated capture for continued batched fixes. Authority dormant.
Game closed, no new launch authorization.

## Consolidated Verification17 launched after ready

Session20260912-192538-1b0d3582 started September13 02:25:39 UTC.
Game PID12648 responding. Wrapper confirmed expected EXE hash and recorded
consolidated=true/native_replay_verification=true; startup stderr exists.
User performing approximately8-minute Big Game checklist. Results pending.
Ready consumed; no relaunch without a new ready.

## Consolidated Verification17 prepared; wait for ready

User requested one full diagnostic session with timing/instructions instead of
continued small tests. V16 was not launched. V17 includes its traffic correction
plus opt-in RR64_VERIFY_CONSOLIDATED sampling: independent budgets renewed each
minute, up to20 starts/category/minute, three-second spacing, eight-update
sequences and a ten-minute wall-clock cap from first eligible race. Normal
verification limits remain unchanged. Bounded coverage rows retain elapsed time,
controller buttons, mode, rider count and traffic roster even on capture failure.
Observed roster coverage does not mean successful traffic comparisons.

Plan: about8 minutes in Big Game with traffic. First2 normal riding near cars;
next2 punches/kicks/weapon attacks; next2 two crashes with full recovery and
two eject/remounts; last2 traffic again, pause/resume, finish if practical or
return to menu, close normally. Another race within the session is fine.
No second person needed for native verification. Authority remains dormant;
this does not verify actual network synchronization or every game mode.
Diagnostic hitches are not a performance verdict. No claim of full coverage
until session evidence reviewed. No launch authorized yet.

Production compiled (protocol34-consolidated-build.log); parser rejection and
coverage fixtures passed. Last all33 checks passed before verifier-only change.
Package17 EXE SHA256: 44694e6e14a34c763e7305512afb86fe068097244c2e3eefba4492f621b755ff
ZIP SHA256: 7038c99e534525eaaa1918973960c8c2ac7755e0d835b705627a12a133607714
README contains full checklist; wrapper records consolidated mode in identity.

## Verification15 reviewed; traffic model correction packaged as Verification16

Session20260912-190800-654b649d exited0 at September13 02:09:59 UTC.
57 completed comparisons match, seven full sequences, zero traffic comparisons.
16 rejected capture events all report traffic-invalid-state (one live entry,
15 private output). This excludes rider capture as the reported failure stage.

Source audit found a concrete schema defect:47668 passes model descriptor bytes
4/3 to7B220, which sums group counts800A7734 plus item. Result s4 is stored in
scene+40 at47874. Capture incorrectly treated that resource index as model ID.
It now resolves through model descriptors800DF210 and group counts; unresolved
or ambiguous resources still reject atomically. F8 is excluded because47734
normalizes it toF7 before construction. No native gameplay change/validation bypass.
Live verification is still required to establish that this removes all rejections.

Traffic capture and online bridge fixtures now model the real resource/model
distinction, with unknown/ambiguous rejection and unchanged output assertions.
The first suite failed the old bridge fixture, which also stored model in+40;
that fixture was corrected. Final all33 pass:
protocol34-checks-1789265593801995700.json. Production build:
protocol34-traffic-model-build.log. Historical logs/failed check retained.

Verification16 prepared, not launched. Authority remains dormant.
EXE SHA256: 94c4e0ebb5a07b2c63436f90720884d5a68221fa06ba297f6330732842f96af8
ZIP SHA256: e63d6a56ff41842e27f26e6c7a20edd65b75c17b89768e921600d204382f3f83
Next ready: Big Game near traffic, crash/recover. Check nonzero traffic comparison
coverage and all rejection reasons before calling the traffic path verified.

## Verification15 launched after ready

Session20260912-190800-654b649d started September13 02:08:00 UTC.
Game PID11932 responding; wrapper verified the expected executable hash.
Native replay verification enabled and startup stderr recording confirmed.
Results pending. Ready consumed; do not relaunch until a new ready.

## Verification14 reviewed; Verification15 prepared, not launched

Session20260912-185303-35453b50 exited normally (0). The72 completed
comparisons and nine eight-update sequences match, but active-traffic coverage
is zero and14 attempts failed output capture. This is incomplete evidence,
not a traffic pass. Canonical rider mapping copies the complete frame first,
so traffic is not lost during that mapping. The old log cannot distinguish
rider capture rejection from traffic rejection; scene-model linkage is only
a hypothesis, not a diagnosed cause.

Capture now reports the rejected state component (roster, timing, route,
outcomes, links, integrators, dynamics, presentation, or traffic substage).
Live/order capture rejections are also reported instead of silently dropped.
Report parser retains rejected attempts separately from comparison mismatches;
zero comparisons cannot report native equality. No validation was relaxed.
Missing-model fixture verifies previous traffic output remains intact on failure.
All33 checks passed: protocol34-checks-1789264856672990400.json.
Parser synthetic coverage/rejection fixtures passed. Production compilation
passed: protocol34-capture-reasons-build.log.

Verification15 is a diagnostic follow-up, not a new online gameplay fix.
EXE SHA256: 65afb23ae75531fe88e85f7526a08136a585aa7b01ed5bb58dbcae70c9011bfe
ZIP SHA256: a819baaa693bcbcf66dee8cecdfe9f8632b19a5cf93b6309c58225cc1ef389dc
Use Big Game with traffic, drive near cars and crash/recover/eject. Need precise
capture rejection evidence before changing the rejected state path. Authority
remains dormant. No game running; wait for a new ready before one launch.

## Verification14 launched after ready

Session20260912-185303-35453b50 started September13 01:53:03 UTC.
PID2500, game window responding. Wrapper verified expected executable hash;
native replay verification enabled and startup stderr recording. Results pending.
Do not relaunch; ready authorization consumed by this launch.

## Verification14 packaged; waiting for ready

Protocol34 private traffic comparison package is ready, not launched.
EXE SHA256: 1c63e780537bc733fe6d4a651cb82c9bea02a6551d37cd0fc1bbbe20fde99f77
ZIP SHA256: d77fe89d62bca5a02df5832f12472e44f73782778bc0d0c1fcef54574b97b719
Workspace analysis/release-1.2-online/RoadRash64-Authority-Verification-14-Win64.
Next run: Big Game or local race with traffic enabled, drive near cars then
crash/recover and eject. No second computer needed. Check active-traffic coverage
before calling traffic verified. Authority remains dormant. No launch signal yet.

## Protocol34 coherent rider/traffic integration verified offline

AuthorityFrame now includes the20-car roster captured at the same completed
native stamp as riders. Traffic travels in separate sub1200-byte fragments,
with a sparse occupied-batch mask (explicit empty batch for retirement). Both
rider and traffic masks must be complete with matching round/tick/ACK metadata
before any frame is published to consumers. Old standalone WorldSnapshot is
ignored while authoritative; old world state is cleared on activation.

Game-thread authority_pin_frame selects one complete frame at update start.
Rider/outcome/world getters and replay preparation use that frame throughout
an update, so concurrent reception cannot mix two host times. Local prediction
corrects matching traffic identities in its disposable baseline. New/removal/
model-change topology defers correction without ACK or live mutation; no native
allocator/resource pointers are copied into old history. Still not full worker
continuation or full actor callback/contact correction.

All33 rebuilt checks pass: protocol34-checks-1789263951827112600.json.
Tests include missing final traffic batch, traffic-first/rider-first ordering,
invalid duplicate car IDs, newer reception during pinned reads, model/identity
matching and deferred private correction. Production builds succeeded in
protocol34-coherent-traffic-build.log and protocol34-traffic-verifier-build.log.

Verifier now compares traffic state as well, reports active-traffic coverage,
and fails a sequence on a traffic-only mismatch. Parser fixture confirms old
logs still parse, a traffic-only mismatch cannot pass, and active traffic is
counted. Previous V13 was clean for riders/progress but did not compare cars.
Next live check should include visible traffic (Big Game or local traffic on).
Authority remains dormant; full lifecycle activation, contact/callback coverage,
network bandwidth optimization and two-computer acceptance are still outstanding.
No percentage gain or completed online model claimed.

## Verification13 reviewed: expanded progress comparison clean

Session20260912-183237-0647ca28 closed normally (exit0) September13
01:34:24 UTC. All96 comparisons match; all12 eight-update sequences complete,
four mounted/detached/remounted each, native shadows equal. Newly included
progress values also match. No game remains running and no launch authorized.
User said done; no separate visual verdict. Continue traffic snapshot coherence
and remaining authority integration; do not request an unchanged rerun.

## Verification13 launched after ready

Session 20260912-183237-0647ca28 started September13 01:32:37 UTC.
PID24756, window responding. Wrapper verified expected executable hash and
enabled native replay verification; startup stderr exists. Results pending.
Do not relaunch; ready authorization consumed by this launch.

## Verification13 packaged; waiting for ready

Expanded progress comparison requires initialized-race evidence. Private
Verification13 is packaged (protocol33, authority dormant), not launched.
EXE SHA256: 5eac6067afc0391b3e3579a49596959142e180f64c1eb3ca1d4c178d23181059
ZIP SHA256: 0b8318434ed0610ca78c56239766c61633e35de64a450afc7239e731a86b4ca5
Path: analysis/release-1.2-online/RoadRash64-Authority-Verification-13-Win64.zip
Archive integrity checked; no ROM/save/guest-memory dump. One local race with
riding, crash/recovery and eject is the next live check; no second player needed.
Do not call this completed online integration. No new launch authorization yet.

## Protocol33 progress coherence, local result ownership and resource admission

Confirmed race-progress schema omission: original68D28..68D50 consumes stats+8,
+C,+20 to compute order distance, and68D48 tests the full +4C word. Capture all
three scalar inputs plus lower +4E half alongside the existing +4C half. All
values come from the same host frame. Nonfinite progress is invalid. Protocol33
makes this schema incompatible with older candidates. This does not synchronize
all route cursor/contact state or prove complete callback/lifecycle coverage.

Remote application also excluded only the local pose, leaving outcome.valid
set. It could install local crash/results before reconciliation approval. Clear
both channels for the local slot, retaining outcome-only remote retirement.
Production-path fixture proves local bust count77 survives while remote count9
is installed; capture/correction covers mapped rosters through14 slots.

Held resource tasks now cause a deferred replay admission, not a fatal failure.
Commands are validated first; malformed history remains an error. Deferral
consumes no network ticket or ACK and changes no live/history state. Fully
acknowledged zero-step correction needs no worker and remains admissible.
This is not worker-continuation capture: sustained unavailability is still bounded
by existing256-command history, and unknown held tasks are never restarted.

All33 rebuilt offline checks pass: protocol33-checks-1789262750922992200.json.
Production compilation succeeds: protocol33-progress-ownership-build.log.
Authority remains dormant; online integration is not complete. Expanded runtime
comparison now includes progress values previously absent from Verification12.

## Protocol32 loading admission and applied-finish barrier

Race inputs now require host release and local loading completion on both
client queue admission and host receive. Early authenticated input is ignored
without consuming sequence1. Socket regression injects a conflicting early
sequence1 and verifies the correct post-release command wins; readiness and
input datagrams may arrive in either order.

Confirmed finish-order flaw: client finish eligibility used the newest received
snapshot tick, although receipt does not commit native reconciliation. It now
uses ClientHistory's successfully reconciled tick; host uses its published tick.
A phase guard suppresses stale finish state outside Race. Tests receive a final
snapshot while an older replay is pending, verify neither receipt nor that older
commit releases finish, then verify the matching commit does. Protocol remains32;
no wire layout change in this fix.

All33 rebuilt checks pass in protocol32-checks-1789261913795124300.json.
Production build succeeds in protocol32-finish-application-build.log.
Authority remains dormant. No package or launch. This verifies ordering only,
not complete results-table coverage or full host finish lifecycle. Resource
worker continuation/admission, complete correction coverage and activation
remain required before an online candidate; no unsupported worker restart added.

## Protocol32 finish-state coverage repair

Confirmed schema omission after clean Verification12: original6EF78 sets
stats+52 on finish, while6EA8C/6EF50 test the entire stats+50 word. Outcome
carried only the upper recovery half. Added finished half to host capture,
packet outcome and selective correction, plus named verifier comparison.
Protocol bumped32 even though the field may occupy old struct padding; old
packets must not silently supply undefined finish semantics. Package manifest
now reads the source protocol rather than hard-coding31. Authority dormant.

Tests cover capture across14 slots, network outcome roundtrip, full-word
recovery/finish correction, and rejected-approval atomicity. All33 checks pass:
protocol32-checks-1789261322485200800.json. Production compilation succeeded
in protocol32-finish-state-build.log. No new package or launch.
Commit audit confirms roster count and actor ownership guards already exist;
no redundant implementation added. Further required work remains: resource
continuation/admission, correction coverage beyond result flags, lifecycle
activation and actual two-computer verification. Do not label this complete.

## Verification12 clean run reviewed

Session 20260912-175129-c2b336d2 exited normally, code0, September13
00:52:46 UTC. All96 comparisons match: twelve complete eight-update sequences,
four each mounted/detached/remounted, with all native shadows equal. No failure
or dependency records. This is the first clean bounded-continuation run after
the order-summary fix. User said finished; no separate visual verdict supplied.
No game running from this session. Parsed verification12-results.json retained.

Do not equate this with complete online acceptance: authority remains dormant,
the test covers short local sequences, and older intermittent height failures
are not proven eliminated across scenes. Next integration review must address
resource-worker admission/continuation, host correction state coverage and
race lifecycle activation before a two-computer candidate. No further random
field copying or gameplay changes are warranted by this passing sample.
Reconciliation does refresh resource inventory from rewritten journal images
before approval; audit confirmed it does not retain old inventory metadata.

## Verification11 reviewed; Verification12 shared order fix

Session 20260912-174509-67b05211 closed code0 at September13 00:46:21 UTC.
95 comparisons, 11/12 matching sequences. Comparison23, mounted depth7, still
differs on ten riders. Native shadows/outcomes equal. Counter read differences
are gone, but this is not a full fix. Terrain80 queries match except epochs.
Dependency candidates shrink to15, including D7668/D766C and per-object reads.

Confirmed missing producer: shared race order at 6EA3C..6EB18 and
6EFF0..6F034 writes D7664 leading eligible racer, D7668 leading eligible human,
D766C average human rank. Native 4F018 consumes the human index; 7A078 reads
average rank. Added derivation to refresh_order_neighbors from private roster;
preserves finished/eligible tests, zero fallback and native signed divisor rule.
No copying from live state, authority activation or protocol change.

Independent extracted-original comparison covers112 roster scenarios including
all AI, no eligible actors, completed actors, zero/nonpositive divisor and
reversed slot ordering. All33 rebuilt checks pass:
protocol31-checks-1789260511213256000.json. Production passes
protocol31-leader-order-build.log. Verification12 packaged in workspace-root
analysis/release-1.2-online. Launched once after ready: session
20260912-175129-c2b336d2, September13 00:51:29 UTC, PID30548, window responding.
Wrapper verified EXE 0c5decbacd04da749b1a838dc56c215a091632d4a0f98b407793852a4a81a52c,
enabled verification and captured startup stderr. Runtime results pending;
do not relaunch or force-close. Remaining per-object
read candidates and intermittent large height errors are not yet resolved.

## Verification10 reviewed; Verification11 counter fix

Session 20260912-173810-9d41d349 exited code 0 at September 13 00:39:40 UTC.
93 comparisons, 11 complete eight-step matches. One mounted sequence failed
at depth5/comparison21 across all ten riders; max root error 0.00012207, with
velocity differences too. All outcomes/native shadows matched. Terrain queries
80, only epoch differences. Dependency trace valid: 155087 entry words,
81 read hits, 31 retained. No large NPC9 height recurrence; still not resolved.

Confirmed omitted inputs: integer counters 800A182C/1830, advanced outside
private prefix at 4EA74/4EA84 and consumed by AI (4EC24 modulo64 scheduling,
57548/598B0/5385C/5411C and others). Added separate private UpdateCounters,
captured before updates, retained in history, restored in private order/frame.
No wire/protocol change or live clock writes. This fixes stale counter input;
causality for the observed frame error still requires runtime confirmation.
Other read candidates (D7668/D766C, D5C7C, D4Fxx, 784FC per-object reads)
remain unclassified, not automatically copied into history.

33 rebuilt checks passed: protocol31-checks-1789260107123321700.json.
Production build passed protocol31-update-counters-build.log. Verification11
packaged in workspace-root analysis/release-1.2-online. Launched once after ready:
session 20260912-174509-67b05211, September 13 00:45:09 UTC (September 12 local),
PID 30536. Window responding, EXE hash verified by wrapper, replay verification
enabled and startup stderr captured. Comparisons pending; do not relaunch or
force-close. EXE fb23efb2f80d43315178b44ab6102d777a5f7246734fdf95603c947e563b3aa6.
Authority dormant. Test single-player Thrash, pass AI, crash/recover/eject.

## Verification10 prepared: history preparation and dependency reads

Standing workflow clarification: after diagnosis, implement confirmed fixes and
run checks without waiting for another "continue". Stop for runtime input only
when necessary; game launch still requires ready.

Confirmed integration gap fixed: reconciliation now calls replay_native_order
before replaying from an acknowledged completion and between subsequent steps.
Sequence-zero baseline is already at update entry and skips the first prepare.
Tests cover ordering, preservation of live/plan/output on failure, and initial
versus completed baselines. This brings the dormant correction path in line
with the existing verifier; it is NOT the fix for Verification09's small error.

Added verifier-only generated scalar read probe. It records native instruction
addresses that read entry-differing words still holding their private entry
value. No guest values are logged or installed. Stack scratch excluded; capped
128 unique reads with total hit counts. Native-hook reads, write-then-restore,
partial-word differences and descriptor hash limitations remain caveats. Hits
are dependency candidates, not automatic corrections. Terrain probe retained.

33 rebuilt checks passed: protocol31-checks-1789259624751158200.json.
Production passed protocol31-history-dependency-build.log. Packaged workspace-root
analysis/release-1.2-online/RoadRash64-Authority-Verification-10-Win64.zip.
EXE 4f7098ade074820a0a11ba2abfe6cae55c529e743125b42b9b6f5da9053312d3;
ZIP 054b7b5117d63570e91a8682f92dfc0936fe46a062995e3c9c9663aebc9483b4.
Launched once after ready: session 20260912-173810-9d41d349, September 13
00:38:10 UTC (September 12 local), PID 29900. Window responding; wrapper
verified EXE identity, enabled replay verification and captured startup stderr.
Runtime comparisons pending. Do not relaunch or force-close. Test single-player
Thrash with AI/crash/recovery/eject, then close normally.
Authority remains dormant; not a completed online candidate. Next read actual
DEPENDENCY instruction sites from failures and implement confirmed omissions.

## Verification09 reviewed

Session 20260912-172401-c1423373 exited normally (code 0) at September 13
00:25:33 UTC. 96 comparisons: 11/12 sequences passed; comparison 56 failed on
NPC3 at depth 8, remounted category. Max root difference 0.000488281, with
velocity/rotation differences as well. All native shadows/outcomes matched.
Terrain probe: 82 queries, zero non-epoch differences, 82 epoch-only, no invalid
queries. This does not support residency flags as this failure's cause. Earlier
large NPC9 errors did not recur, but are NOT fixed by this diagnostic-only build.
Only local0 entry words differ: actor D4/D8, bike 328/3F0/4A0. Next trace
external interaction/collision dependencies and these fields' consumers.
Details: workspace-root analysis/release-1.2-online/authority-migration/
verification09-session-review.md and verification09-results.json.
No game running from this session; no relaunch. Authority remains dormant.

## Verification09 prepared - targeted terrain dependency probe

Verification08's two large NPC9 height errors remain unresolved. Original
146C8 checks cell+0C == 5 before querying resident terrain; 41090 uses that
query for bike body height (412C4/412D8). Terrain preparation also runs outside
the replay prefix. Residency divergence is a hypothesis, not a confirmed cause.

Added read-only, disabled-by-default verifier evidence: snapshot live terrain
descriptors at frame entry, then compare only cells actually queried in private
146C8. Uses original computed indices, records loaded state, payload address,
request epoch and a header fingerprint; fingerprints are not identity/proof of
complete payload equality. Separate epoch-only differences, bounded 64 records,
failure-only output; no live state copied into replay, no gameplay change.
41090 scopes bike ownership for relevant query reports. Original generated
game functions are untouched; probe hooks exist only in private generated code.

33 rebuilt offline checks passed: protocol31-checks-1789258829080850500.json.
Production passed protocol31-terrain-probe-build.log. Prepared workspace-root
analysis/release-1.2-online/RoadRash64-Authority-Verification-09-Win64.zip.
EXE fbe2aaf5e1e9dc92e7353f0b850d2ecd8842ed72c54413ecf6b0a836740ee18c;
ZIP 629a0c80467cbbc43f15e03edeab42b43cba1828e5e9486388b9b4802e491ac5.
Launched once after ready: session 20260912-172401-c1423373, September 13
00:24:01 UTC (September 12 local), PID 16904. Window responding; wrapper
verified the EXE hash and enabled replay verification; startup stderr present.
Runtime comparisons pending. Do not relaunch or force-close. Test: single-player
Thrash, pass AI, crash/recover/eject, then close normally.
Authority remains dormant. This is diagnostic evidence, not a completed online
candidate or a claimed synchronization fix. Next review TERRAIN/TERRAIN-CELL
records alongside failing actor fields; if only epochs differ, do not treat
that as evidence of missing collision geometry. Matching headers do not exclude
deeper payload differences, asynchronous changes or other dependencies.

## Verification08 reviewed - September 12 local

Session 20260912-170602-5166abf2 exited normally at September 13 00:10:03 UTC.
84 comparisons: ten complete eight-update matches, two failures at depth 2 on
NPC slot 9. Four mounted and four detached sequences passed; two remounted
sequences failed. All native shadows matched. No stats differences remain
after private order in this sample. No game is running from this test.

Remaining failures are large: root errors 226.089 and 248.602, bike translation
component 2 live about 0.52/0.54 versus replay about 226.61/249.12. Slot 9's
actor/bike/rider/stats entry masks are identical; only local slot 0 has entry
differences (actor D4/D8, sometimes bike 328/3F0). Next trace collision/terrain
height, spatial lookup/cache and external resource dependencies; do not assume
the cause or dismiss these as floating-point roundoff. Details and decoded
offsets: workspace-root analysis/release-1.2-online/authority-migration/
verification08-session-review.md and verification08-results.json.
No new game patch or launch during review. Authority remains dormant.

## Verification08 preparation history - September 12

Integrated private race-neighbor derivation (+28/+2C/+30 links, +34/+38/+3C
signed gaps and +40 rank) after native order. Completed-entry skips, ties and
untouched boundary fields match the original 6EB1C..6EBEC block. Original
55574/555E4 consume the nearest link via actor+E8. This fixes a confirmed missing
calculation, but runtime causality for Verification07 drift remains unverified.

All 32 rebuilt checks pass: protocol31-checks-1789255115454168700.json.
New independent native-block comparison covers 70 scenarios and invalid-input
rejection without writes. Production passed protocol31-neighbor-order-build.log.
Details: workspace-root analysis/release-1.2-online/authority-migration/verification08-neighbor-integration.md.
Prepared analysis/release-1.2-online/RoadRash64-Authority-Verification-08-Win64.zip;
EXE 30f06576013f6575e3b9a2392f02174714489bb41e4d52b603b89b6f9516cc42,
ZIP 0eda55f24f0bf60c61793a6a41267f8f74d808cd0e202c7c73d615250a53a979.
Launched once after ready: session 20260912-170602-5166abf2, September 13
00:06:02 UTC (September 12 local), PID 25168. Window responding; wrapper
verified EXE identity, enabled replay verification and captured startup stderr.
Race comparisons remain pending the user's test. Do not reopen or force-close.
Test instructions: single-player Thrash,
pass AI, crash, recover and eject for 1–2 minutes, then close normally.
Authority is still dormant; this is not the complete online candidate.
Stats+44/later finish effects and other entry differences remain pending.
Reusable neighbor-state lesson added to the N64 skill.

## Verification07 reviewed - September 12

Session 20260912-155538-20119fab closed normally (exit 0, 23:13:01 UTC).
78 comparisons across 12 sequences: eight matched through eight updates, four
failed on dynamics (two all-actor, two NPC-only). All native shadows matched.
Rounding isolation did not eliminate the failure classes. No game is running
from this test; historical launch descriptions below are superseded.

New entry masks expose stats+34/38/3C and other omitted fields. Original
6E5E0 writes those stats in 6EB2C..6EBE0 and stats+44 at 6EBF4; private order
currently stops at 6E844 and publishes only rank+40. Next: trace these later
calculations and their consumers, preserve required dependencies, and separate
them from notification/finish side effects. Do not blindly copy the diff masks.
Detailed review and decoded masks are in workspace-root
analysis/release-1.2-online/authority-migration/verification07-session-review.md
and verification07-results.json. No new build or launch during this review.
Full authority integration remains unfinished and dormant.

## Verification07 preparation history - September 12

Fixed a replay isolation gap: recomp.h implements COP1 rounding with the host
thread floating-point environment, outside recomp_context. CpuContext now saves
that environment; private frame, order and resource-worker execution restore it
through an exception-safe scope that restores the caller afterward. Four rounding
modes, moved CPU entries and exception unwinding are covered by the capture test.
This is not yet proven to be the cause of Verification06's remaining mismatches.

Private verification now retains bounded actor/bike/rider/stats entry-difference
word masks and prints them only when an update fails comparison. These contain
offsets, not guest contents. They identify possible missing dependencies, not
fields authorized for live copying; rendering state can legitimately differ.

All 31 rebuilt offline checks passed: protocol31-checks-1789247092202099400.json.
Production compilation passed (protocol31-floating-context-build.log).
Prepared analysis/release-1.2-online/RoadRash64-Authority-Verification-07-Win64.zip
in the workspace root. ZIP SHA256 bc98e5e097ea96df2ba758b98b914ddf2b80ea58cf937d7af3cd9f68e50489ca;
EXE SHA256 ec5f35036014d4cf651d087e19c620023d9f1228de2cb4d159a59138b8304b64.
Launched once after ready at 22:55:38 UTC September 12; PID 29708, window
ROAD RASH 64 RECOMPILED responding. Session 20260912-155538-20119fab under
the Verification07 test-logs directory. Wrapper verified the EXE hash and enabled
native replay verification; startup stderr exists. Race comparisons still need
to be checked after the user finishes. Do not relaunch or force-close this run.
No second player needed: single-player Thrash,
drive past AI, crash, recover and eject for 1–2 minutes, then close normally.
Authority remains dormant and the complete online candidate is unfinished.
N64 skill reference updated with the bounded replay and floating-point lessons.

## Current verification status - September 12, after Verification06

Verification06 session `20260912-125430-ff702ce2` closed normally (exit 0).
It completed 88 comparisons in 12 consecutive replay sequences: 10 reached
eight updates with all checked values matching; two stopped on movement
differences. Private race-order reconstruction removed the observed rank
mismatches, including the earlier initial-race failure. Native shadow state
matched throughout. This is partial runtime evidence, not complete replay or
online acceptance. See `analysis/release-1.2-online/authority-migration/verification06-session-review.md`
in the workspace root for the recorded build identity and limits.

No verification game is currently running. Older running-session descriptions
below are historical and superseded by this status. A new launch requires ready.
Authority remains dormant. Continue diagnosing the remaining shared movement
dependency and NPC divergence, then integrate verified replay ordering and the
remaining state/lifecycle work before packaging a playable online candidate.
Do not dismiss velocity differences because the first detected position error
is small: sequences stop at their first mismatch.

## Latest online integration - September 12

Verification03 finished normally: four mounted, four detached, four remounted native comparisons matched. See analysis/release-1.2-online/authority-migration/verification03-session-review.md for limits. No game is running from that session. Fixed local reconciliation overwriting remote outcome records: inactive pose alone did not suppress Outcome.valid. Final local commit now excludes both remote channels; historical baseline still includes remote outcomes. Regression fixture preserves newer remote bust count88 over historical2. All31 rebuilt offline checks passed: protocol31-checks-1789240788167674100.json; production Release compiled. No new package or launch. Authority remains dormant; complete state coverage, resource continuation and lifecycle activation still pending.

## Active private native verification - September 12, 12:01 local

User approved a private verification build, then explicitly requested launch when ready. Authority Verification 02 is running (PID412 at launch verification); do not reopen or force-close it. Session: analysis/release-1.2-online/RoadRash64-Authority-Verification-02-Win64/test-logs/20260912-115936-d215f8c8. Executable SHA256 055d0fbe46f3220e40f578f34a84b24ce6bb9d3ad37d7baa09a53132329db117. ZIP SHA256 b918423924548a6b8e93d82ded612f6b7cb9e5060667da4a7a3d201f038d981e. This is NOT the completed online-authority candidate; authority remains dormant.

Latest31 offline checks: protocol31-checks-1789239377522397500.json. Production log protocol31-native-verification-detailed-build.log. Added disabled-by-default RR64_VERIFY_NATIVE_REPLAY: one disposable native update every3seconds, capped12 completed comparisons/60attempts. Captures actual CPU/timing/input/native shadows and checks resulting roots, integrators/outcomes and native state against the real update. It does not install private results or write guest-memory dumps. Bounded failure text is exposed by last_native_replay_error. All12 observed comparisons matched checked actors/dynamics/outcomes exactly, max-root-error0, native-equal1. Scene mode/attachment state were not logged, so this does NOT establish crash/eject coverage: two eject log lines appeared AFTER comparison12. Next verification should reserve samples for riding/detached/recovered stages instead of exhausting all12 early.

Package01 wrapper failed before creating a game: Windows PowerShell child could not resolve Get-FileHash under inherited module paths. Kept01 intact and prepared02 using .NET SHA256/ZipFile.02 started successfully, window title ROAD RASH 64 RECOMPILED, responding, logs growing. Restricted Get-Process showed handle0; same elevated desktop context found the actual window. No duplicate game launch. Start-Verification.vbs/ps1 and READ-ME-FIRST are in the02 folder; no ROM included. User was told no second player is needed, use single-player Thrash, drive/crash/recover/eject abouta minute, then close. They are currently testing. New N64 skill lessons were appended.

The full original task still remains: collision/route/controller-state coverage, worker-held jobs, live authority activation, complete round/restart verification and the actual online candidate ZIP. Continue after this capture; do not describe the private verifier as finished online implementation.

## Connected correction and race lifecycle - September 12

Protocol31 production builds; SHA256 deef81db3307ff7eec5af7090f4a751da6c4f18dd0c0f83ea43cc312cde6a7ee. All31 rebuilt offline checks pass (protocol31-checks-1789238762161081400.json); production log protocol31-retired-outcome-build.log. No game launch and no playable ZIP. Authority still has no live activation: host_release_selection does not enable it. Do not mistake the compiled reconciliation/lifecycle hooks for an activated new model.

Connected client correction after the completed local input: restore historical baseline, atomically apply host movement/results, replay privately, validate live ownership/native state, retire the network ticket and replace corrected history/live values. Native cop/eject commits reject calls inside private replay. A new synthetic-executor fixture covers failed replay, rejected tickets, duplicate retirement and guest/native/history commit. This is not full native race execution evidence.

Native loading gate now checks all human actor links before acknowledging loaded; its game dispatcher hook waits for every participant. Network begin packets retain released state monotonically. Leaving/timing out during loading or racing ends the authoritative session through the existing menu-return path, avoiding a permanently held input or barrier. Local/lobby removal remains unchanged. Host-selected results/main-menu modes are allowlisted and repeated in begin packets with a required final snapshot tick. Client dispatcher requests teardown once, never repeatedly resetting mode39. Activation and complete restart/round integration are still pending.

Movement correction also restores host-owned bust/eligibility/role and recovery values through locally resolved stats pointers. Added stats+40/+50 from66AC8/66B84. Reaction clocks rider+5C4/+5C8/+5CC affect native control (36D3C/36D6C;6B234), not just pose, and now correct with movement while leaving visual descriptor/resource pointers local. Stats writes use the same ownership/ticket transaction. Final outcomes survive actor retirement: Outcome.valid contributes to the snapshot payload mask independently of visible active state. Network and native-memory tests cover retired outcomes, final-snapshot ordering and2/4/14 rider mappings.

Remaining required work: complete collision/actor/race dependency coverage (including route progress/controller state), private full-native replay with valid race state, in-flight worker state support, activate authority and verify entire round/start/restart flow, then package. The source still deliberately rejects worker-held jobs; there is no valid full-race image test yet. The user's instruction is to finish the whole list and package, not stop at this checkpoint.

## Atomic movement correction - September 12

Protocol29 production compiles, SHA256 89f5c668c220e216b0cb55c274766faa7de736d79903599460d15ad9238a6d35. Thirty rebuilt offline checks pass: protocol29-checks-1789236225986364500.json. No game launch or playable candidate ZIP. Authority is still dormant; the live online model remains the earlier hybrid. The complete requested build is still unfinished; do not distribute this intermediate executable as the new authority candidate.

Added explicit value-only translation/rotation integrator state for bike+108 and rider+28 (37CDC/36B88). Original34594/348D8 use separate force, impulse, rotational basis/quaternion, angular motion and resting flags; root positions alone omit these. The native physics fixture now corrects divergent images in four resting/movement scenarios and compares40 subsequent original-native updates, in addition to the original120-step history test. This establishes integrator coverage, not collision/race replay. The first expanded fixture overflowed by repeatedly feeding34594 its already-scaled impulse; fixed the fixture to supply a fresh per-update impulse like its caller. No native physics formula changed.

MovementCorrection prepares all active pairs from one host frame, resolves local actor ownership, checks model identity and disjoint allocations, rejects contradictory overlapping pose/physics values, and checks the original values before ticket approval. Invalid state, changed ownership and rejected approval cause no partial writes. A successful commit allocates nothing, is single-use and writes no remote pointers. Twenty local-slot mappings across2/4/14-rider fixtures pass. Actual production capture also round-trips through this transaction. Connected this transaction to the dormant authority remote-rider path; the local rider stays reserved for reconciliation. Legacy race memory, guest menus, traffic, attack visuals and presentation checks pass.

Protocol29 packets carry the extra integrator values. Each packet is848bytes, below1200; the active mask permits skipping empty slots while retiring them atomically. Sparse2-rider frames use1696 payload bytes, full14 use11872; these are byte counts, NOT measured latency or bandwidth with all UDP/relay/voice/world traffic. Reordering, missing parts, duplicate parts, conflicting metadata, sparse frames and empty-roster retirement pass. Fourteen-player bandwidth still needs attention before activation.

Client cop movement/recovery prediction now uses the locally mapped human roster; bust awards and race wins remain host-owned. Historical replay cannot read current live siren/outcome data.

Remaining: complete host correction coverage for collision/actor/race state; connect private replay, corrected history and live native state with ticket retirement; valid full-race replay verification; authority race-start barrier and complete race lifecycle. Worker-held resource jobs are still rejected. Existing frame replay has compile/link coverage but no complete race execution evidence. Continue implementation rather than another intermediate package.

## Input resampling and corrected history - September 12

Production protocol28 compiles, SHA256 08e6f75adef4abeca41971438954cf929dee54e1b9af532dd036639f82925a49. All21 rebuilt offline checks pass: protocol28-checks-1789233043185678800.json. Authority is still dormant; live online remains the prior hybrid. No game launch and no playable candidate ZIP. The requested complete authoritative build is NOT finished.

Host input now consumes contiguous available samples per native update, preserving press/eject edges and acknowledging only after completion. Fifteen30/59/60/120/144 client-versus30/59/60 host cadence combinations remain bounded over60seconds; this is synthetic queue verification, not measured race latency. Missing sequence gaps are never skipped. Multiple same-button taps within one native update collapse to one native edge. The actual generated40664 comparison now includes a release/repress burst.

History records pre-update timing, native CPU entry and narrow session rules. Replay transports still see no live session; native cop rules retain historical ownership. Combat replay preserves client damage suppression, and siren replay cannot read a newer live outcome. First-step history reconstructs previous buttons from the native changed mask. Historical replay preparation validates every input against the journal. Evaluation carries corrected native state through each step and builds a replacement journal privately. Allocation and validation precede network-ticket approval; the final journal swap cannot fail, and epoch checks reject duplicate/stale commits. These transactions have synthetic executor tests; they are not connected to a complete live correction.

Confirmed schema additions: rider+D0 vector (36B88 plus37024/37040), effect value+5D4 and duration+5D8 (379A8/379C0 and37088..370B8), bike recovery clock+4CC (3FFA0..3FFC0). Capture and restoration validate finite values and reciprocal rider/bike ownership. Protocol changed because the authoritative payload changed. Packet-size assertions compile. These fields do not establish complete simulation-state coverage.

The earlier report protocol27-checks-1789231788579495500.json is NOT valid for the full changed source: the broader build had failed and two old executables ran. Fixed the compile error and outdated fixture; the runner now rebuilds every listed target and refuses to run tests after a build failure. Subsequent reports use this gate.

Remaining before activation/package: full valid-race private replay verification; complete typed host correction/live write ownership (roots and damping are insufficient); connect prepare/evaluate/commit with live correction and input retirement; authority lifecycle and complete race transitions. In-flight resource jobs remain explicitly unsupported. Client cop prediction currently excludes host-owned cop rules, which must be considered during integration. Do not label this compiled intermediate executable a playable new-authority build. Continue implementation; user explicitly requested the final test package rather than incremental handoffs.

## Isolated native update integrated - September 12

Production compiles, SHA256 e7715380af3e717e1a209681d6ede5c4d19e374eea84a40249f2900615c20356. Protocol remains27; authority is still dormant and the live model is unchanged. All20 offline checks pass (protocol27-checks-1789230357208187400.json); the full native frame has compile/link verification only, not a completed race replay test. No game launch or playable ZIP.

Implemented private resource queues/ROM copies and a generated copy of the original C7D0 worker with C++ linkage. The earlier C7B8 worker label was incorrect: C7B8 is an instruction inside free-slot helper C778. C7D0 handles requests, priority ring, chunked DMA and completion. The offline reference uses the original generated worker and compares all8MiB with the production private worker. Priority ordering, chunk boundaries, free-slot restoration, private completions, queue wrap/full behavior, malformed ranges and missing worker-owned tasks pass. ResourceInventory is captured beside each historical guest image. Unrepresented tasks are rejected, not assumed complete. A blocked worker result is not resumable by restarting it.

The private full6AFFC update prefix and474 dependencies (including eject) are generated separately from current native sources, with a bounded callback table, private queue wrappers, private error exits and a2million-block limit. Verify the generator's printed count if regeneration changes the closure. Native C++ cop/eject state can be exported only from the current replay epoch and carried into a later step; rule/post checks prove this does not replace live state. Input admission skips the native2192 suspended-update condition rather than queueing input that6B098 cannot execute. Windows /EHsc assumes C-linkage wrappers cannot throw: the first worker fixture aborted on its idle yield; /EHc- fixes that reference fixture, while production clones deliberately use C++ linkage.

Remaining before activation/package: exercise full native replay with valid race state; connect coherent host correction and history retirement without copying whole RDRAM; finish complete authoritative state/ownership coverage, native cadence and lifecycle. The worker-held/in-flight resource case remains explicitly unsupported. Do not treat20 checks or compilation as a full online candidate. User wants continuous work through the finished build, not incremental test ZIPs.

## Protocol27 regression check - September12

Latest production SHA25690b39a24735aea841b5379a32523eaa43a99c12c4b390e0ca1b93ebc819028b7. All19 offline checks pass; report: analysis/release-1.2-online/authority-migration/protocol27-checks-1789219349333238400.json. Eject protection now resets for a new authority round; a failed predicted eject does not leave a pending capture.

The first suite run timed out in the snapshot test's startup barrier: exclusive copy could queue before its second shared reader, blocking that reader while the first waited. Fixed the fixture by making main participate in the startup rendezvous before requesting exclusive access. The runtime gate itself has no such rendezvous. The rerun passes all19 checks in about3seconds. run_authority_checks.py saves each result immediately to a unique file, including timeouts.

The alias-aware reproducible audit is now audit_replay_dependencies.py:474 generated functions, four functions with indirect calls and three OS entry paths. DMA worker0C7B8 performs ROM copies then signals1518/14C0; queued resource work is the remaining full-prefix replay isolation problem, not merely rumble. No full replay executor, correction/cadence or lifecycle activation yet. Authority remains disabled and no playable package was created. No game was launched. User still wants the complete implementation and ZIP, without incremental handoffs.

## Replay context and sequenced eject integration - September 12

Protocol27 production SHA25642a4e49d45745aca57986eec44e9dfbdc5e8b875bb9f5a3e62dce929424c97c2 compiles. Authority remains dormant; native replay/correction, cadence and lifecycle are not complete. No game launch or distributable ZIP. Continue through implementation; the user explicitly requested no incremental handoffs.

History now retains entry CPU registers with f_odd rebased during restore; both floating-point modes pass relocation/isolation checks. Replay Controls matches the actual40664 routine in84 comparisons, including two passes with one press. Native physics harness passes120 translation/rotation steps and byte-identical midpoint restore/replay, not a full race test.

Eject is a validated one-time authority command for all14 slots; missing/held commands clear the action. Host and prediction hooks call unchanged3F0E8 with reciprocal identity checks and durability protection. Legacy local behavior keeps its four-controller loop.14-slot helper checks (transition substituted),8400 delayed/reordered commands and actual packet dispatcher checks pass. Replay rumble is suppressed; manual-eject replay protection is thread-local.

The earlier call audit missed LOOKUP_FUNC and generated symbol aliases. Expanded callback traversal reaches3FE48 ->68E20 ->7C0F8 ->0CD34 -> message queues and a ROM-DMA worker. Private-image replay must not invoke the live scheduler. This resource dependency is unresolved. The resolved-callback JSON initially counts453 before alias expansion; later alias-aware traversal finds474. Passing control/physics checks do not prove full replay isolation. Do not enable authority yet.

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

## Client replay handoff and side-effect isolation — September 11

Protocol23 production SHA256 b32639dd50b01b0b9fff678291f07b0c517f634f2ebf170d8b6710e130332ab3 compiles successfully. Added authority_prepare_replay/authority_commit_replay: copy the host frame and exact outstanding inputs under the network lock, run native replay outside the lock in isolated memory, and retire history only after the issued ticket succeeds. New local input invalidates an older prepared replay; newer host frames may arrive without invalidating an otherwise coherent issued replay. Tickets are not reused across session resets. Dispatcher checks pass for concurrent input, newer frames, duplicate commit, old tickets, and round changes. No native caller invokes this replay yet.

rr64_prediction_replay.hpp supplies a non-nestable thread-local isolation scope. Custom cop rules and runtime posts use private per-replay copies; achievement game events are suppressed during replay. Actual cop runtime tests prove a replayed long-press can toggle only the shadow siren, while ordinary long-press still toggles live state. Actual cop rules tests prove replay cannot advance the real win timer or reset the real mode. This isolates registered C++ effects only: caller must supply isolated guest memory and the correct historical native state. It is not full prediction replay or a complete side-effect proof.

New passing targets RR64PredictionSideEffectSmoke and RR64PredictionCopRulesSmoke supplement the prior10 checks. Production compilation passes. Dependency audit additionally identifies sprintf_recomp, math, switch_error and do_break in direct closures; do not assume the graph proves replay safety.

Still outstanding before activation/package: complete native movement/crash state journal and replay executor, correct historical cop state for replay, input cadence/backlog reconciliation, complete remote pose/outcomes and14-player cop parity, and lifecycle activation. Native host step/routing/capture and remote frame channels from the preceding checkpoint remain implemented but dormant. No game launched; no downloadable test candidate created. User asked to continue until usage runs out; most recent limit reading was96% used, not exhausted.
## Native host step and capture checkpoint — September 11

Protocol23 now has generated6AFFC simulation-boundary hooks and a40664 input hook. Authority remains explicitly dormant (no live authority_start caller); local/hybrid gameplay remains enabled until client reconciliation is ready. Host commands are staged at the native update entry and acknowledged only at6B678 after every active human reached40664. Completed host capture follows canonical actor links for all14 records and rejects invalid counts, duplicate bikes/riders, broken reciprocal links or incomplete captures. Atomic authority frames now feed existing remote rider getters/presentation. The authority host no longer applies remote snapshots over its simulation. History resets on authority/round changes.

Native control routing redirects only call registers:4EB6C ->524CC and4EAD0 ->5264C at6B1C8;6B1D4 enters the existing human pipeline;515C4 start override ->40500 at6B224. These pairings are derived from native initialization6CC94/6CCC4 and transition6ECC0/6ECB8. Actor identity/controller/classification/callback fields are not permanently rewritten. Unknown callbacks or missing translated humans prevent acknowledgement. Further callbacks, full native crash lifecycle and all14 cop behavior still require validation.

Ten offline targets pass. New checks: RR64AuthoritativeStepSmoke (boundary/capture with native callee mocked), RR64AuthoritativeGeneratedSmoke (84 comparisons using actual generated40664/math helpers and synthetic coefficients), RR64AuthoritativeActorSmoke (actual generated6B17C..6B2C4 control-loop fragment and40664 for2/4/14 humans, with physics/override/reaction callees mocked). extract_control_fixture.py refreshes the copied exact generated fixtures after regeneration. These are NOT live race or full physics tests. Production compilation passes. EXE SHA256 acc4a46c5c4b95c6375e19cc055a0af8b7bbbdb54cd924c291c8af4318c907a5. No game launch or distributable package.

Client reconciliation remains unfinished: no input-history retirement or native prediction replay is connected, so do NOT activate authority yet. Current RiderState is not a complete replay-state schema. The direct-call audit (analysis/release-1.2-online/authority-migration/prediction-call-audit.json) finds57 functions reachable from4090C,23 from5980C,60 from45960,12 from36948. Cop posts/shout/siren/trick, bust/notification and achievement hooks are non-guest-state effects that replay must isolate. This audit does not resolve memory aliasing or prove all indirect paths. Preserve native world/collision dependencies; do not replay arbitrary full frames or position-only corrections.

Outstanding delivery: replay-safe local prediction/reconciliation with processed-input ACKs and cadence handling; complete authoritative outcomes/pose and14-player cop parity; lifecycle activation and offline connection/regression checks; then package for the user. User asked to continue until usage runs out; no ready was given, so keep the game closed.
## Coherent authority frame channel — September 11

Working protocol23 adds a dedicated host frame channel: all14 rider records plus one completed simulation tick and all14 processed-input acknowledgements. Five independently decodable bounded datagrams form a frame; an eight-frame bounded assembly commits only once all parts agree. Missing, reordered, duplicate, stale and conflicting-stamp parts are checked through the actual client dispatcher. Local player state is included, and receipt never retires predicted inputs. Host publication rejects premature ticks, fabricated acknowledgements, nonfinite active states and human/AI ownership conflicts. In authority mode, old client-rider sends, host incremental snapshots, client snapshot application and local/AI publication APIs are suppressed.

RR64AuthoritativeChannelSmoke and RR64HitChannelSmoke pass; RR64NetplaySmoke compiles. Production compilation passes. EXE SHA256 7bb90d8c33d954d81209271f4f2d9593d69ccb595680a65bf4b02eb8b1132799. Tests validate transport/assembly, not native gameplay or complete crash pose. No game launch or package.

Still outstanding: enable only after native all14 host driving, complete host frame capture at the post-simulation fence, and replay-safe client reconciliation are integrated. Authority remains dormant; ordinary gameplay remains hybrid. New frame transport is not a claim that the existing RiderState contains every replay dependency. User's completion-and-package request remains outstanding.
## Authority transport checkpoint — September 11

Protocol22 now includes authenticated sequenced input transport and an internal host-authority switch. No live caller enables that switch: gameplay still uses the hybrid model. Fixed host packet dispatch previously misplaced inside the Hello branch; normal authenticated input/hit packets now reach their handlers. Host-local commands use a separate sequence counter, avoiding a self-acknowledgement history overflow. Authority-mode combat runs native decisions on the host only and bypasses owner proposals/replay.

Seven offline checks pass (input history, controller scope, round coordinator, native translation bridge, real UDP input channel, combat ownership, hit channel). Channel checks cover foreign session/endpoint rejection, stale rounds, host-only pause masking and 600 host steps. Production compilation passes, EXE SHA256 c0bf3cbdf2f3700257a862befe776b49fe1e9d30b08640c96f0c0707b885ee7c.

NOT COMPLETE: live all14 host driving, authoritative snapshots including the local player's processed-input acknowledgement, replay-safe native prediction/reconciliation, and complete outcomes remain unintegrated. Do not label this an enabled host-authoritative candidate. No game launched or distributable package created. User requested completion and packaging; that request remains outstanding.
## Authority native control bridge — September11

Added native_controls around original40664, preserving real output pointers/register behavior and restoring the scratch controller. Four authority offline targets now exist; new native bridge test passes and production links. No gameplay hook activates the new model yet. Production protocol21/hybrid remains active. Complete host driving/transport/state publication and replay-safe client reconciliation are still required before packaging. See active docs/ONLINE_AUTHORITY_MIGRATION.md.

## Authority host-step coordinator — September 11, implementation ongoing

Added14-slot HostRound with atomic input batches, staged-step/post-step ACK separation, host-only pause masking and bounded missing-input hold. Resend includes newest plus oldest commands. Three offline tests pass, including8400 commands under simulated loss/reordering/delayed ACKs. Still not wired to production transport/native driving. Production remains protocol21 hybrid; do not package as the new model. Native callback trace and next steps are in active docs/ONLINE_AUTHORITY_MIGRATION.md.

## Active direction: host-authoritative migration — September 11

User explicitly approved replacing the hybrid model with host simulation, sequenced inputs, local prediction/reconciliation and remote interpolation while keeping Host/Join simple. Do not activate both authority models together. First implementation: rr64_authoritative_input.hpp and rr64_authoritative_controller.hpp, with two passing offline tests. These helpers are not yet wired into live simulation or transport. Production remains protocol21; do not call this a new playable model or package it. Source checkpoint: analysis/release-1.2-online/authority-migration/checkpoint.json. Next: host command transport/step fences, native human driving for all14, full host state capture, replay-safe local movement and host-only contacts. Plan: docs/ONLINE_AUTHORITY_MIGRATION.md in active tree.

## Protocol21 hybrid hit channel — September 11, experimental

Added owner-native contact proposals for61224/616BC, host endpoint/ownership/round/range checks, ordered reliable requests/commits, and victim-owner native replay before state publication. Eight offline checks and production build pass. This is NOT a server-authoritative simulation: contact detection remains attacker-owned, and separate collision impulses are not relayed by this channel. No live acceptance or ZIP. User is reviewing a server-authoritative/prediction/reconciliation migration; do not describe this interim channel as that architecture. See analysis/release-1.2-online/shared-hit-review.md.

## Protocol20 pose and impact-state gaps — September 11, work in progress

Added alternate bike display orientation (+21C/+4AC/+4B8, used by5E880) and rider impact reserve (+310, modified by616BC) to coherent owner capture/transport/application and root diagnostics. Seven offline regressions and production compilation pass. Fourteen-peer impaired UDP run passed, largest packet1188bytes. These close verified missing-state fields; no claim that final rider drift or authoritative hits are fixed. No launch/package/publication; downloadable Candidate16 unchanged. Review: analysis/release-1.2-online/display-basis-review.md.

## Protocol19 native traffic bridge — September 11, work in progress

Added host traffic motion/basis state, client native allocation/model/placement, original-loop retirement, round-safe update suppression, and bounded multi-frame world reassembly. Nine offline checks and production compilation pass; impaired14-peer transport run passed. Native bridge tests mock its generated callees, so actual resource cleanup and collision behavior still need live verification. Seven world batches increase bandwidth; no latency or visual-success claim. Attack visual work remains included. Combat authority, pedestrians and rider drift remain pending. No launch, ZIP or publication; downloadable Candidate16/protocol17 remains unchanged. See analysis/release-1.2-online/traffic-native-review.md.

## Protocol18 attack presentation — September 11, work in progress

Candidate16 paired test rejected: missing remote punches/kicks, rider drift and different traffic. Owner attack visual snapshots now drive the stock rider/weapon pose builders temporarily, using local descriptor/resource resolution and the movement playback timeline. Five offline checks and production compilation pass; no live test or ZIP. This does not complete authoritative combat, traffic lifecycle, or rider drift. See analysis/release-1.2-online/attack-visual-implementation-review.md. Last downloadable Candidate16/protocol17 remains unchanged; do not mix versions.
## Online Fix Candidate16 packaged — September 11

User requested the current test build. Candidate16/protocol17 ZIP is ready at analysis/release-1.2-online/RoadRash64-Online-Fix-Candidate-16-Win64.zip. All PCs must use this ZIP. Start-Online-Test.vbs enables bounded sync logging and packages logs on close. 159 manifest file hashes, ZIP integrity and PowerShell parser checks passed. Build verified; no game launched or release published. Package identity:analysis/release-1.2-online/candidate16-package.json.

This packages the current latency/presentation/state improvements; it does NOT complete the pending traffic/pedestrian native lifecycle, authoritative outcomes or full crash-animation integration. README states those limits. Launch only after READY. Older candidates preserved.
## Compact relay — September 11, protocol17 WIP

Host forwards only changed complete actor records per recipient, skips self-echo and padding, repairs final state every100ms, and checks for new movement every8ms. Owner updates now16ms, using measured bandwidth savings. Whole-world cadence remains25ms. Round ownership, per-actor tick and all-record validation retained. Production and eight offline tests pass; no game/ZIP/publication.

Against protocol16:four-peer typical age118.224->110.096ms; fourteen-peer p95 age234.789->220.736ms (repeat220.221ms), with total bytes down16%/20%. Worst isolated gaps worsened; do not claim all stutter resolved. Repeat14-peer run passed. Full evidence analysis/release-1.2-online/compact-relay-review.md. Native gameplay/world/crash integrations still pending. Last downloadable Candidate15/protocol14 unchanged.
## Delay reduction — September 11, protocol16 WIP

Kept race deadline cadence correction, skipped entirely inactive rider batches, and combined full20-car roster into one1080-byte datagram. 2/4/14-peer and outage tests pass. Median peer state age:four-peer124.543->118.224ms; fourteen-peer202.405->188.546ms. Four-peer total bytes down about21%; fourteen-peer bytes UP about18% as intended40Hz cadence is restored. No claim for bandwidth-capped links. Voice device/codec work now has a separate worker; failed device opens retry once/second. Production and seven offline regressions pass; live audio/game acceptance pending.

See analysis/release-1.2-online/delay-reduction-review.md and delay-comparison.json. Native traffic execution and other sync integrations remain pending. No new ZIP/game launch/publication; last downloadable Candidate15 remains protocol14.
## Real UDP impairment tests — September 11

Offline local peers using production netplay passed 20-second moving-state checks: 2-player LAN, 4-player 100 ms nominal RTT with jitter/2% loss, 14-player 150 ms nominal RTT with jitter/5% loss, and 4-player 300 ms outage. Maximum observed update gaps:61/140/216/391 ms. These are transport/state coherence tests, NOT native races, full world synchronization, or visual acceptance. No game launched. See analysis/release-1.2-online/connection-simulation-review.md and results JSON. Remaining integration work in the next section is unchanged.
## Playback recovery and traffic reconciliation — September 11, WIP

Bounded visual playback now handles outage recovery and clock drift; seven offline targets and production compilation pass. The synthetic 200 ms outage maximum step changed from 17.5 to 1.100037; missing packets still cause a hold. Ten-minute +/-1000 ppm tests pass with jitter/loss. Small rate corrections introduce a measured tradeoff versus ideal constant-speed interpolation; no live smoothness claim.

Traffic ID reconciliation is tested, but the native lifecycle executor remains unfinished. Retirement must retain the native resource ownership/scene cleanup, not only call the pool free routine. Do not package this as full world synchronization. Continue traffic/pedestrian/outcome/pose integration, then further connection smoothing. No launch without READY. No package or publication this pass. See analysis/release-1.2-online/playback-reconcile-review.md and results JSON.
## Traffic snapshot channel — September11, work in progress

Host native capture and host-only UDP world snapshots are implemented. A full20-slot roster commits atomically after five bounded four-vehicle batches; old/foreign-round/incomplete snapshots cannot resurrect or partially remove vehicles. Snapshot fields: native traffic ID, active state, model/type, position, velocity, Euler angles. Native IDs are taken from entity+4, not pool addresses or compacted indices. Capture reads compact roster D76E0/countA6528, model scene nodes A145C; invalid lists preserve prior output. Native model selector includes D8..101 plus125/126; do not truncate the special branches.

2/4/14-peer host-only delivery tests pass. Tests cover reordering, loss of batches, duplicate identities, stale resurrection, malformed captures, round changes and native read-only capture. Production integration compiles. No live game launched or new ZIP.

Client lifecycle application is NOT yet implemented: received world state must still invoke local allocation/model setup/removal safely, then apply all native motion/collision dependencies. Traffic still simulates independently on clients until that bridge is completed. Do not call this complete traffic gameplay sync. Pedestrians, authoritative interactions/results and full rider/bike facet alignment remain pending.

Files: native/src/rr64_world_sync.hpp, rr64_traffic_sync_capture.hpp, rr64_netplay.cpp; evidence analysis/release-1.2-online/world-sync-results.json. Working protocol15; downloadable Candidate15 remains protocol14.

## Researched timing correction — September11

Working protocol15 adds owner monotonic sample timestamps to preserve movement spacing despite receiver-frame timing. Synthetic steady/jitter/5% loss regression assertions pass;200ms outage remains imperfect. Production and2/4/14 transport checks pass. No new ZIP/live test. See work/release-1.2-online/docs/ONLINE_NETWORKING_RESEARCH.md and analysis/release-1.2-online/network-timeline-results.json. Traffic lifecycle and full gameplay/pose authority remain pending as below.

## Synchronization expansion in progress — September 11, 2026

Continue from Candidate15 source/fixes; last distributed ZIP remains Candidate15/protocol14. Working source now protocol15 and must not be packaged as completed world synchronization. Production compilation passes. Implemented existing host AI racer root updates (canonical roster, identity guard, no remote writes on host), native float bike durability, selected weapon and 15 inventory halfwords, round-scoped rider packets/state reset, reserved player ownership mask, and corresponding log fields. Offline 2/4/14 peers, native memory ownership/identity/equipment checks and 37-column bounded CSV checks pass. These are state replication improvements, not authoritative hit/damage arbitration or complete AI lifecycle handling.

Still required: traffic/pedestrian lifecycle and shared state, host-authoritative interactions/results, complete animation/body synchronization, fourteen-player Custom Cop behavior. User additionally requires a full producer-to-draw trace of every rider/bike connection facet after the additions; verify mounted/crashed/ejected/recovered states, weapons, animation clocks, roots and all player mappings. Do not infer completeness from transmitted root positions or packet placeholders.

Traffic trace: 6BFF4 checks 20-car cap (A6528), resolves route via6B9E8, allocates via46488, initializes via77498, chooses model/builds scene node via47668, marks334 active/336 zero and places via6BA88. 47668 includes RNG model selection and render asset allocations. A coordinates-only update must not substitute for canonical lifecycle and collision geometry. Further tracing needed before activating traffic replication.

Full progress and hashes: analysis/release-1.2-online/sync-expansion-progress.json. No game launch or new ZIP created. Preserve Candidate15 as crash-fix test package.

## Synchronization audit: incomplete gameplay coverage — 2026-09-11

See [ONLINE_SYNC_AUDIT.md](ONLINE_SYNC_AUDIT.md). Candidate15 synchronizes connected player roots, not the complete dynamic world. Traffic/AI/pedestrians, authoritative gameplay events/results and full crash animation remain missing; Custom Cop replicated >4 path is excluded. Do not describe packet placeholders as implemented sync or transport tests as gameplay parity. Audit-only task made no game changes.

## Current: Online Fix Candidate15 — 2026-09-11

Clean release1.2 source: work/release-1.2-online. Protocol14. Corrects Candidate14 HUD pointer sign extension crash; all eight hooks and actual MEM_W pointer regression checked. Production build passes; live test pending. Package: analysis/release-1.2-online/RoadRash64-Online-Fix-Candidate-15-Win64.zip. See candidate15-review.md. No launch without READY. Supersedes older entries below.

## Current: Online Fix Candidate13 — 2026-09-11

Clean1.2 online source. Protocol13, both peers must update. Candidate12 reached
Race but user rejects smoothness and remote recovery pose. Candidate13 publishes
render offsets across threads and synchronizes bike+7F6 crash/riding pose gate.
Cross-thread draw entry, matrix isolation, recovery flag and2/4/14 peer checks
pass; production build and package checks pass. Live verification pending.
Package: `analysis/release-1.2-online/RoadRash64-Online-Fix-Candidate-13-Win64.zip`.
EXE SHA256: `a0c7f77495705823f428e9c73ea628163e01c1dec515bb0d4fd3a168d6c61328`.
No game launched; fresh READY required. See candidate13-review.md. Supersedes below.

## Current: Online Fix Candidate12 — 2026-09-11

Clean1.2 online source, protocol12. Both Candidate09 testers were stuck at Start Race.
Host request was logged; no race loading. Candidate12 retries a persistent host
start command as fresh button edges (120ms down,180ms up), preserving readiness
and loading barriers. Includes Candidate11 render-matrix smoothing; both changes
still need live acceptance. Build, delayed-edge/guest-flow checks and package checks pass.
Package: `analysis/release-1.2-online/RoadRash64-Online-Fix-Candidate-12-Win64.zip`.
EXE SHA256: `7bf7f03cd93af9bcb61967edbfb98480eba29a61be202af90e0c953d61e20fef`.
No game launched; fresh READY required. See candidate12-review.md. Supersedes below.

## Current: Online Fix Candidate11 — 2026-09-11

Clean source `work/release-1.2-online`, protocol12. Candidate10 remains rejected.
Candidate11 integrates translation smoothing ONLY in temporary draw matrices;
Candidate09 actor simulation records remain unbuffered. Covers rider/bike/weapon
roots, including independent MaxLOD graph selection. Offline matrix-memory
isolation, timeline, ownership and logger checks pass; production build passes.
Package: `analysis/release-1.2-online/RoadRash64-Online-Fix-Candidate-11-Win64.zip`.
EXE SHA256: `4e3df7f5b9549d648e9f0981d35a074bd7cfe71d79af8517ec44d7354be96aae`.
Live acceptance pending. No game launched; fresh READY required.
See candidate11-review.md for evidence and limitations. Supersedes entries below.

## Current: Candidate10 rejected; Candidate09 restored — 2026-09-11

User reported worse stutter and positions with Candidate10. Its guest-record smoothing
has been removed from clean source `work/release-1.2-online`; failed source preserved
under `analysis/release-1.2-online/candidate10-rejected-source`.
Production rebuild and guest-memory checks pass. For play use the existing Candidate09
ZIP (protocol12): `analysis/release-1.2-online/RoadRash64-Online-Fix-Candidate-09-Win64.zip`.
Candidate09 alignment is user accepted; remote visual stepping remains unresolved.
Rebuilt source executable SHA256: `fa36679b358c3cf8a1add696205a12711c727546c777f56967d41e5a5dd1b304`.
No game launched. See candidate10-user-capture/review.md. This supersedes prior entries.

## Current: Online Fix Candidate10 — 2026-09-11

Clean source: `work/release-1.2-online`. Protocol12 unchanged.
Candidate09 position alignment accepted by user; remote visual stepping remains.
Candidate10 adds 75ms mounted remote translation buffering before pose preparation.
Latest state still applies before simulation. Local owner and crash/eject motion bypass smoothing.
Package: `analysis/release-1.2-online/RoadRash64-Online-Fix-Candidate-10-Win64.zip`.
EXE SHA256: `8458b03fe7665639861d58cecd1e8a27b4efd04d3ff8e9136556d7ec17196bb4`.
Build, synthetic smoothing, 2/4/14-slot local-memory isolation and ZIP checks pass.
Live acceptance pending; no game launched. See candidate10-review.md for tradeoffs.
These current results supersede historical pending-test entries below.

## Online Fix Candidate 09 — 2026-09-11

Active clean source: `work/release-1.2-online`. Protocol 12.
Package: `analysis/release-1.2-online/RoadRash64-Online-Fix-Candidate-09-Win64.zip`.
EXE SHA256: `9d008a5c7df0ae0d54b17cb647d69c42205713a32c54246864684ae195ca4066`. ZIP SHA256: `664fd66dadc15940f64d39a38eff287cb03898bf60cf957b9d65916d9cfaf47b`.
Build, 2/4/14-peer checks, guest-memory isolation and package integrity pass.
Candidate08 main-menu return accepted by user; movement synchronization still failed.
Candidate09 adds primary bike origin/movement and rider velocity to coherent snapshots.
Live synchronization remains unverified. No game launched; fresh READY required.
See `analysis/release-1.2-online/candidate09-review.md` for evidence and limitations.
This current status supersedes historical entries below.

# Candidate08 live acceptance (2026-09-11)

Host-disconnect return to main menu is user-verified. Rider/bike synchronization
is still rejected. Physics origin and integrator state are absent from the
current presentation snapshots; do not certify full synchronization from
matching root/coordinate messages. Keep the accepted disconnect fix.

# Candidate08 implementation (2026-09-11)

Capture/correction now resolve racer-owned bike/rider allocations. Reciprocal
links persist through crash detach; do not infer ownership from attachment or
pool order. Host-loss menu transition is requested once and intermediate mode39
is allowed to update. Reordered-allocation and transition-progress checks pass;
visual validation remains pending. User additionally observed rider and bike
moving separately in Candidate07; do not report this as fixed without a test.

# Candidate07 implementation (2026-09-11)

Protocol11 coherently transports positions, root rotations, rendered heights and attachment/eject flags. Seven-rider batches preserve the 14-slot limit without IP fragmentation. Per-rider tick ordering and local-echo exclusion apply. Offline checks pass; visual acceptance pending. Full animation, collision/AI and result authority are not implemented by this root-state change. Clean 1.2 source only.

# Candidate06 live result (2026-09-11)

User confirmed synchronization still fails after driving; initial placement is
aligned. Paired coordinate transport agrees, but does not capture all rendered
root/height/animation state. Candidate06 is not an accepted online fix. See
`analysis/release-1.2-online/candidate06-user-capture/review.md` in the workspace
root for evidence and remaining authority/pose work. No new candidate packaged.

# Candidate 06 implementation status (2026-09-11)

Protocol 10 adds separate rider coordinates. Position corrections now precede
pose preparation for both the one-camera and multiple-camera branches. This
remains positional replication, not proven synchronized collision/AI/crash state.
Keep logical camera counts between online draws for Max LOD preparation.
Host disconnect now requests the original main-menu transition after its notice;
process exit is not the intended behavior. Offline checks pass; live validation
is pending. See the project candidate06 review for evidence and limitations.

# In-Game Direct-Connect Multiplayer Plan

## Decision

### Active online candidate flow (2026-09-11, protocol 8)

The clean v1.2 working copy now implements private stock character/bike selection
on each peer, using a temporary one-selector menu count. Confirmed selections are
transported as round-scoped state and committed to the canonical race slots only
after the host has everyone's confirmation. A second loaded acknowledgment gates
guest race updates. The host supplies an initial RNG seed. These are candidate
changes, not proof of deterministic simulation or internet synchronization.

Setup remains host-controlled. Non-host Start is filtered at packet receipt,
input retrieval and the local input return; the pause menu consumes the host's
stream. HUD drawing temporarily selects the original one-player layout and local
rider, then restores the globals before subsequent rendering/simulation. Custom
Cop HUD filters to the local peer, and the cop requirement checks the shared choices.

Offline two/four-peer transport and synthetic guest-memory restoration checks
pass. Live selection visuals, pause/resume, race loading, collision and drift
remain unverified. The five-to-fourteen-player replicated path remains experimental;
the four-controller Custom Cop/extended-options parity work does not establish
support for that larger path. No renderer experiments are included.

Replace the previously attempted online multiplayer implementation with one clean, in-game direct-connect system. The old launcher/overlay online flow, centralized relay service, matchmaking/public-lobby flow, lobby codes, and experimental Direct-IP path are legacy and should be removed before the replacement is integrated.

No multiplayer or game-code removal is part of this notes-only change.

## Current menu-entry update (2026-09-08)

The user explicitly requested separate original main-menu entries: **Multiplayer** for local play and **Online** for the existing Host/Join popup. This supersedes the nested Local/Online choice for this update. It does not authorize replacing the current network protocol as part of this menu change. The wider direct-connect redesign below remains future work.

## Required player flow

**Main Menu → Multiplayer → Online → Host / Join → Lobby → Host Game/Race Setup → Per-Player Character/Bike Select → Race**

Online setup and session progression belong inside the game, not in the launcher.

The host is authoritative while the stock game-type and race-option screens are active. Connected clients mirror those choices and cannot change them independently. Once the host confirms the setup, every peer regains control on the stock character/bike screen for its own rider.

## Network model

- Prefer a host/client direct-connect design over centralized matchmaking or relay servers.
- One player hosts; the others connect directly to that host.
- Keep the network lobby separate from the original local-player selection structures where practical.
- The lobby must show connected players, ping, and ready state.
- Keep the lobby UI/data model scalable beyond four players even though the first working target is four-player online.
- Expand the online player limit later only if the recompiled game's player, race, camera, UI, and related structures can safely support it.
- Preserve existing local multiplayer while replacing only the attempted online implementation.

## Implementation order for future work

1. Audit and remove the previous online multiplayer hooks, launcher/overlay UI, relay-server integration, matchmaking/lobby-code paths, and experimental Direct-IP path.
2. Verify the base recompilation still builds and runs normally, including local multiplayer.
3. Add the in-game Online menu and Host / Join flow.
4. Add the separate network lobby with connected-player, ping, and ready-state handling.
5. Integrate host-authoritative game/race setup, per-player Character/Bike Select, and Race synchronization for an initial four-player online session.
6. Evaluate higher player counts only after the four-player implementation is stable and the underlying game structures have been validated.

## Initial scope boundary

The first milestone is stable four-player, host/client direct-connect multiplayer. Centralized servers, public matchmaking, relay-backed lobbies, and a higher player limit are not part of that milestone.

## Implementation status (v0.8.0)

- The launcher and settings-overlay online entries have been removed.
- The relay server, lobby-code, public matchmaking, Quick Play, proximity-voice, 14-player override, and experimental remote-transform implementation have been removed from the active project.
- Selecting the original main-menu **Multiplayer** option now opens an in-game choice between **Local Multiplayer** and **Online**.
- **Local Multiplayer** resumes the original game mode and controller setup without network changes.
- **Online** provides direct **Host** and **Join** actions, followed by a separate four-slot network lobby with player names, ping, and ready state.
- When the host begins, each peer enters the original multiplayer character-selection, track-selection, and race path. Network-assigned controller slots carry each peer's input through those stock screens and gameplay.
- Protocol-v2 automated tests validate four simultaneous peers, slot assignment, ready/phase propagation, and all four synchronized controller streams without loading the game.

Runtime UI and gameplay validation is intentionally pending until the user asks to launch the build. Direct internet hosting may require the host's UDP port to be reachable; NAT traversal and relays remain outside this direct-connect milestone.

## Experimental 14-rider checkpoint (post-v0.8.0)

The higher-player-count evaluation requested after the four-player checkpoint is now implemented as a separate network mode, while the original local multiplayer path and the proven two-to-four-player controller-stream path remain intact.

- The in-game lobby and protocol now admit fourteen network peers. Network slots are separate from the N64's four controller ports.
- Sessions with two to four players retain protocol-v2's controller-stream behavior. A host that begins with five or more connected players latches replicated-rider mode for that session.
- Static analysis verified that Road Rash allocates fourteen native bike records and fourteen native rider records. Replicated mode uses those existing pools instead of inventing extra controller ports.
- Protocol v3 adds authenticated per-slot rider proposals and host-distributed canonical race snapshots. Snapshot sequencing, finite-value validation, interpolation history, disconnect cleanup, and a sub-fragmentation packet-size limit are in place.
- The game-side bridge keeps each machine's locally controlled rider in native entity zero and maps the other canonical network slots onto distinct native bike entities. It synchronizes the bike body and both wheel transforms before simulation and again before rendering.
- A headless host plus thirteen clients now passes admission, ready/phase propagation, and all-fourteen-rider snapshot validation. The normal two-peer compatibility test also passes.

This is an experimental gameplay checkpoint, not a stable 14-player release. Live testing still must verify stock character/track flow with more than four peers, rider/bike visual binding, collision and combat behavior, race results, cameras, late disconnects, and recovery. Direct-connect/NAT requirements are unchanged.

## Host-authoritative setup checkpoint (protocol v4)

- **Begin Game Setup** now enters the original multiplayer setup screens with only the host controlling game type, race, track, and related options.
- Every client mirrors the host's controller stream during setup. The host's finalized setup words are then captured, versioned, and distributed in the canonical lobby snapshot.
- The stock transition into character/bike selection is detected directly from the game's multiplayer stage. Clients apply the host's finalized settings before their own controls are enabled.
- After that boundary, each connected peer controls its own assigned stock selector in the two-to-four-player path. The experimental higher-player path maps each peer's local selector to its own network rider slot.
- Returning to the main menu and selecting Multiplayer again now tears down the previous online socket, protocol phase, and pending stock-menu transition before opening a fresh Local/Online choice.
- Live multiplayer rendering and race synchronization recognize all four gameplay-state pairs in the stock dispatcher, including the Thrash-specific path, so every game type receives the same widescreen projection handling.
- Original local multiplayer bypasses all of these online hooks and retains the game's normal controller and menu behavior.

## Race identity and proximity voice checkpoint (protocol v5)

- The display name entered in the in-game Host / Join screen is now carried into the stock multiplayer race presentation. The four stock `Player 1`-style HUD strings are replaced at their original initialization boundary; original local multiplayer names remain untouched.
- Online races publish the local bike position in both the two-to-four-player controller-stream mode and the experimental replicated-rider mode. This presentation-only snapshot supplies voice distance without changing physics, collision, or AI.
- Every direct-connect race renders one full-screen camera for the local peer. Two-to-four-player sessions retain every synchronized controller stream for simulation while selecting only the peer's assigned camera; replicated-rider sessions retain their local-rider-in-viewport-zero mapping. Stock split screen remains exclusive to Local Multiplayer.
- Race-only proximity voice uses 48 kHz mono Opus frames over the authenticated direct-connect session. Clients send only to the host; the host validates the assigned speaker slot and sequence, rate-limits packets, and relays them to the remaining peers.
- Nearby riders are heard at full volume and fade smoothly to silence at long range. Decoded audio is bounded and stale out-of-range speech is discarded.
- A **Proximity Voice Chat** switch is available in the recompilation Audio settings. The microphone is closed in lobbies, menus, single player, and whenever the setting is disabled.
- Protocol-v5 headless tests validate two-peer bidirectional voice relay alongside the original lobby/input/setup flow. The fourteen-peer admission and canonical-rider test continues to pass.

## R33 candidate corrections (2026-09-06)

The user requests finalizing music and online play, a selectable maximum of fourteen players, and reports host-only crash at online race finish with one joiner. The tested folder is unknown; retain this issue for the next test and preserve the host log.

Protocol 6 adds the original A6680/A6690 race-option words to finalized host setup. Setup controller presence no longer hides remote ports; only the host provides setup actions. Online Player Limit on the connect page cycles 2..14 and gates host admission. EF5C is the four-controller menu count and remains <=4 (one for replicated mode); A6574 is the actual rider pool and can reach fourteen. This separation replaces the previous unsafe experimental write of fourteen into the menu count. A6578 retains the physical human count. The larger roster still requires live gameplay validation.

The online camera plan now also checks actual live mode/pending mode, and transition logs capture settings, role, network slot and counts. This is a candidate correction, not proof that the reported host finish crash is solved. Preserve original local multiplayer behavior. Both peers must use protocol 6/R33 for validation.

## R38 connection candidate (2026-09-06)

The user reports R36 remaining on Connecting. R38 retains the direct-connect design and protocol 6 gameplay structures, adding a 15-second join deadline, a five-second host-loss deadline, explicit full/started/version rejection, trimmed IPv4/hostname:port input, exclusive host UDP binding, and sparse connection diagnostics. Initial welcome validation prevents delayed welcome packets from rewinding connected sessions. Receive processing is capped at 256 datagrams per update.

Local multiplayer, player limits, game setup and rider replication are preserved. No legacy relay, matchmaking or overlay lobby is restored. Loopback transport tests can verify these changes but cannot establish internet reachability. Test both peers on R38 and retain both runtime logs. The available R36 log contained no online attempt; the original WAN block and previously reported host finish crash remain unconfirmed.

## Local settings setup candidate (2026-09-07)

The user requested local controller/name setup in the launcher/settings overlay before choosing Local Multiplayer. The candidate adds four controller cards with saved in-game names, automatic connected-device assignment, optional keyboard enrollment and four-port limits. Local selection resumes the stock race menu directly. Online names/input remain separate. See docs/LOCAL_PLAYER_SETUP.md. Offline checks pass; live multi-controller validation is pending a new ready. No publication authorized.

## Pre-race ready roster (2026-09-11, candidate 03)

The original menu font now draws a non-interactive top-margin roster during
CharacterSelect and TrackSelect. Current-round character/bike confirmation
controls READY/WAIT; lobby readiness does not. Custom Cop labels require both
confirmed cop rider (40-44) and cop bike (31). Names are bounded to ten ASCII
characters for layout safety. Race/offline screens have no strip. Network
protocol and race-start behavior remain candidate 02/protocol 9. Build and
offline readiness checks pass; visual placement requires live verification.

## Candidate 04: race identities and guest camera banks

Paired Candidate 03 captures (candidate03-user-capture/transport-report.json in
the root analysis directory) contain 8,532 matched payload samples with no
mismatch. Both ran the identical executable and exited normally. The client
still simulated a different host position; matching transport is not physics
acceptance. Stock race creation consumes F400/F5D8, whereas the private selector
previously committed only F670/F660. The handoff now writes both pairs.

Within world drawing, full-screen hardware layout/viewport are separate from
logical player-camera slots. DB88 retains the logical count for pose/LOD banks;
DB84 hardware caching is invalidated before viewport setup and the logical
camera restored afterward. All viewport helper calls in the scope are covered;
only one world-loop iteration is permitted. The scope ends with the draw.
Offline/local paths bypass these overrides. No packet format or simulation
position-correction changes were made. Live appearance and drift remain unverified.

## Candidate 05: position application, host loss and fourteen riders

Candidate 04 captures agree on race-load identities and initial sampled states.
18,586 matched sender/receiver samples had no payload mismatches or unmatched
updates. Independent input replay still drifted: apply_remote_riders bypassed
the 2-4-player path. All connected remote slots now receive the existing
interpolated bike/body/wheel transforms before and after guest simulation.
The owning player's local position remains untouched. This is positional
reconciliation, not full deterministic physics/collision/crash replication.

Explicit host disconnect and five-second silence now enter a terminal state
that retains the prior player/camera identity instead of becoming split screen.
Input and simulation stop; yellow pulsing Host Disconnected is drawn, then
the client game exits after three seconds. Physical controller disconnection
does not trigger this state. Pause menus use host virtual port zero and the
menu direction filter is enabled while online racing is paused. Reconnect
behavior is a candidate fix pending live acceptance.

The online settings player-count number now shows the connected network count
(up to 14 including host), while the underlying controller arrays remain at
1 for the replicated >4 path. All 14 actor creations inject confirmed rider/
bike identity by canonical network mapping. Local split screen stays 1-4.
Delayed lobby snapshots previously overwrote local readiness: a 14-process
test found this and the same-phase local ready value is now preserved.

Verification: 2/4/14 headless peers pass selection, start barrier, input and
position transport checks. Graceful and abrupt host loss tests pass. Guest
memory checks cover remote application without local overwrite and viewport
indices for every local slot at 2/4/14 riders; offline paths are unchanged in
these checks. Fourteen-player gameplay, full Custom Cop parity beyond four,
crashes/collision agreement, message placement and physical hotplug remain
unverified live. No game was launched to produce this candidate.


## Candidate 14 — September 11, 2026

Active source remains work/release-1.2-online (published 1.2 base). Candidate14 protocol14 adds detailed rider anchor synchronization, online eject ownership, private HUD actor loads and 40Hz movement relay. Offline checks pass; live verification pending. See analysis/release-1.2-online/candidate14-review.md. Failed renderer source remains excluded.







## V36 automatic demo replay milestone

See UNATTENDED-ONLINE-HANDOFF.md V34-V36 sections for current work. Original
AI demo now exercises native AI without human control injection. A preserved
V33 random-state failure is fixed by supplying the historical per-object
visibility predicate at the private simulation read. Descriptor memory is never
rewritten; bike/object identity changes reject. Original-case positive replay
and deliberately stale-visibility negative control behave as expected.
All34 checks passed. V36 checkpoint312 matching comparisons includes traffic,
crashes and remount, with zero rejected comparisons or RNG differences.
This is native replay evidence, not completed host/client integration.

September19: next private test is pending-roaming-distance-recovery in
analysis/release-1.2-online/sky-review-20260919. Executable staged in the parent
folder, previous executable preserved by hash. Protocol37; SHA256
3C602076E4D7C7F5D857576C929407D28742CD3B48ECD01714284ADB77DAC689.
Includes cop selection reminder below the selection frame, bounded human reverse
route updates and nearest valid race-road recovery from current bike position.
Production build + 24 roaming/native-accumulator checks + 205 HUD checks + local
options/cop smoke pass. See docs/roaming-distance-recovery.md. No live launch;
user must say ready. Preserve separate visual acceptance and do not claim the
reported screenshot is confirmed fixed without that test.

September 19: roaming-distance-recovery user test accepted. Next private build pending-cop-backhand, protocol38, SHA256 274BE56D22BB648AA12018E40952A07C918AE40407F9F1F49FB7DA9B48AEC6AF. Dedicated RB trick now uses reserved button bit 0x40 transported in existing input snapshots and stripped at the native control adapter. C-Down/backhand and C-Down+C-Right jam remain intact for cop and ordinary rider. Production build and cop-input smoke pass; 24 roaming and 205 HUD checks pass. Not launched, await ready.

September19 release cleanup: Windows 1.3.0-rc1 package prepared, protocol38 remains unchanged. Online experimental. Source cleanup/recovery copies and eight passing smoke suites recorded in parent analysis/release-1.2-online/release-cleanup-20260919. ZIP RoadRash64Recompiled-v1.3.0-rc1-Win64.zip, EXE SHA256 daed6361dec89cf33cdc5e7cc7bd2619972ad5fa52def5c14d133c7d87939ccf. Await packaged user test; no launch/publication. Linux update and final source/dependency export remain pre-publication tasks.

September19: user played packaged Windows RC1 and reported it seems ready. Added requested full-map draw-distance guidance and recommended 50-60% starting range. Documentation-only ZIP refresh; executable unchanged. ZIP SHA256 34bf5766f4ad04869e74b4473ff90d89f4223de066b6ec750ea1ed3ae16bd882. Publication and Linux refresh remain pending.
