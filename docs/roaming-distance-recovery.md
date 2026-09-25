# Backtracking distance and nearby recovery — September 19

User reports: signed rider distance remains large after returning close; crashing
after backtracking returns the rider to old race progress. Separate from the HUD
placement and world-pickup fixes. No visual launch authorized for this revision.

Native trace:

- HUD 30220 reads actor+E8 (race state), target actor at +28 and signed gap +34.
  6E5E0 computes gaps from the ordered progress table 800D77A0. These are route
  distances, not straight-line separation, so nearby riders across a hairpin can
  legitimately have a larger route gap. Finished/busted racers also have native
  ranking rules; these are not changed into physical-distance HUD readings.
- Physical route search 507B0 advances segments with 66D70 but rejects a negative
  parameter at 508A4. Human point-to-point travel can now walk backwards using a
  bounded five-segment search. AI and circuit wrap behavior are retained.
- Progress accumulator 674C4 clamps backward point-to-point segments at 67518..
  67538. For valid human backward transitions it now uses native bidirectional
  6736C. This keeps +20/+8/+C consistent instead of freezing at the old segment.
- Recovery 68E20 calculates a forward catch-up offset from rank, speed and stored
  progress. The new human hook at 68EB4 runs after the bust/wreck exclusion and
  selects a nearby valid road curve into the existing sp+58/sp+5C outputs. It
  rejoins at 692BC, retaining native road-height sampling, bike/body recovery,
  camera reset and damage rules. It does not respawn busted racers.

Recovery searches the race road's quadratic curves (same evaluation as 17050),
using the bike's current +16C/+170 horizontal position. It excludes the native
blocked/non-spawnable road flags. This implements the user's nearest-road fallback,
not unrestricted off-road respawning. Invalid course/position data preserves the
original recovery path. No cached native side state is introduced. Authority and
prediction replay use their captured human-slot masks, including slots beyond four.
Protocol 37 prevents a peer using older recovery rules from joining this build.

Validation: production Release build, 24 roaming checks (including the actual
generated 6736C/66DA0/66D70 routines extracted at configure), 205 HUD checks and
the local roster/menu/Custom Cop tests pass. The native accumulator test performs
290 -> 25 -> 275 -> 25 on a straight-road fixture. Recovery checks cover returning
near, curved roads, blocked roads, AI exclusion, malformed data and replay rules.
These establish offline behavior, not a confirmed reproduction/fix of the user's
live screenshot. In-game backtracking, cop pursuit, nearby recovery and online
visual acceptance remain pending. No renderer/HUD anchoring changes were made.

September 19 live feedback: user reports 'it worked' after the prepared distance/recovery/footer test. Accepted for that session; online peer verification remains separate. A new cop down-attack/trick conflict was reported and is addressed independently.

## September 21 native caller correction

An audit found a defect outside the previously tested helper: the backward
retry at 508A4 jumped to 50840 without reloading t0 from sp+70. Native 5083C and
508E0 perform that reload, but 50844 and the intersection callee can overwrite
t0. The next intersection therefore received the wrong output-point pointer.
For caller 5203C, the two point components could overwrite a separate scalar
and the curve parameter. The hook now restores t0 before retrying.

An extracted complete before/after 507B0 caller, original 66D70 and production
roaming helper pass 84 scenarios with bounded intersection/math probes; 40 old
cases demonstrate the wrong argument. Forward/stationary and AI paths, all four
local slots, saved registers and an adjacent stack canary are checked. This
proves the caller repair, not full geometry/physics or live online correctness.
Private fixture and reviews: analysis/insanity-hang-20260921/roaming-*.

Candidate05 was reported as returning to the modern Play/Settings launcher
during a later Insanity race; the user then closed it. No exception stack or
Windows fault event was captured. The route bug is independently verified, but
is not established as the cause of that return. Optional UI-state transition
logs now distinguish running-game state, launcher, settings, prompts and input
capture; runtime error-callback text is recorded before its modal opens. No
launcher or input behavior is changed by those diagnostics.

## September 21 circuit recovery freeze

Candidate06 session `20260921-210115-800d1e9a` froze after player 1 ejected.
Native VI continued, but guest updates and display-list production stopped.
A private dump, two live Windows stack walks and a coherent guest-memory/context
capture resolve the game thread through recovery `68E20` to progress `674C4`.
The latter's `67574..675AC` loop repeatedly called `66D70` and curve-length math.

The captured circuit has 21 records and wrap index 16. `66D70` traverses
`2,4,...,16,2`; it cannot reach segment 0 from the current segment 16.
Nearest recovery selected 0 because records 0/1/2 exactly duplicate 16/17/18,
and equal-distance ties kept the first candidate. This proves this run's hang;
it is separate from the earlier optional combat-credit access violation and
does not establish the cause of Candidate05's reported launcher return.

Recovery now searches only `2..wrap` on circuits, inclusive, and rejects malformed
wrap metadata without writing recovery outputs. Point-to-point searches still
include `0..count-3`. The existing native recovery tail, progress accumulator,
lap rules, terrain sampling, camera, damage and ownership checks are retained.
In the captured failure, valid segment 16 represents the same physical curve
as the rejected duplicate 0. No timeout or arbitrary lap/progress reset masks
the loop. AI retains stock recovery; the same helper serves local human players
and authoritative online human slots.

Windows and Linux production builds and 282 roaming checks pass, including the
actual native successor and circuit progress accumulator. Those checks cover
approach/tail exclusion, each legal circuit start, spatial samples, invalid wrap
metadata and retained open-road/ownership behavior. Captured-state reproduction
and detailed reports are kept privately under `analysis/insanity-hang-20260921`;
Candidate07 build evidence is in `analysis/circuit-recovery-fix-20260921`.
The private native-math/progress fixture also passes 400 cases on both platforms.
It preserves the captured progress state: the old target 0 triggers a bounded
negative control backed by the proven unreachable successor cycle, while target
16 completes with the same curve parameter. All 288 legal circuit combinations
match the full stock progress block/return delta/saved registers; 100 open-road
transitions are unchanged. This replays the affected route logic, not an entire
race. Live acceptance requires another owner-authorized test; offline checks
alone do not establish whole-game or online correctness.

## September 21 circuit recovery progress correction

The next report was an early multiplayer race end. The available log records
the results transition, but not the rider's final health/finish flags; it cannot
establish whether that specific end was a finish, wreck, bust, or another rule.
The native normal-race rule already waits for all human participants to finish
or retire. It can end while AI riders are still racing. A separate last-human
wreck path in `3FE48` can enter results directly, bypassing predicate `740A0`.
Do not identify either path as the cause of that session without captured state.

The audit did independently reproduce an incorrect lap award during recovery.
After selecting a valid nearby circuit curve, the old recovery tail called the
normal forward-only `674C4` accumulator. If the nearby point was on the preceding
curve, it treated the relocation as travel all the way around the circuit. On
the privately captured course, all seven preceding-curve cases incorrectly added
971–986 units on a 1002.889-unit lap and incremented the lap counter.

`rr64_roaming_recovery_progress` now handles only the circuit relocation call
inside `68E20` at `69464`. It uses the native `17DBC` curve-length calculation,
chooses the nearest signed displacement around the circuit, and applies that to
the actor's existing unwrapped progress. It rejoins at `6946C`, retaining the
native `68D0C` lap evaluator. The physical recovery target, terrain and body pose,
camera reset, damage, normal driving accumulator, AI, and open-road behavior are
unchanged. Bust/wreck state and invalid metadata fall back without changing the
actor's progress.

Completed laps remain intact. A backward crossing can lower the segment-base
distance below zero, which the native progress evaluator supports; it does not
turn the unsigned lap count negative. Returning across that line restores the
original distance instead of earning another lap. A real forward crossing during
a crash still receives the native lap award. The geometric route-array seam and
the actual lap threshold are tested separately because they need not coincide.

The private Windows and Linux native-math fixtures pass seven formerly failing
backward recovery cases and 607 seam/finish checks. These include real forward
crossings, repeated backwards recovery followed by ordinary driving, preservation
of several existing lap counts, and negative prestart distance. Actual native
`674C4`, `68D0C`, `68C70`, `1A250`, and curve math run against locally loaded private
course data; no game or renderer is launched. Source smoke tests also cover AI,
open roads, busted/wrecked state, malformed metadata, authoritative player slots,
and captured replay ownership. Private probes and outputs remain under
`analysis/insanity-hang-20260921/roaming-lap-*`; captured memory is never packaged.
This verifies the independent progress defect and its repair, not the unknown
terminal reason of the reported session or a complete online race.

## Three-lap follow-up and crossing diagnostics

The Candidate08 test completed normally at stored lap3. The owner reported four
laps but was unsure whether this included an initial short finish-line passage.
On the captured circuit, the grid projects before the finish marker on curve0,
which duplicates the final curve. Initial departure therefore passes the marker
before completing a circuit. The native menu maps 1/3/7 laps correctly, and full
native traversal fixtures on Windows and Linux match those targets with and
without recovery. No extra-lap defect has been established; finish rules remain
unchanged. See the private evidence in `analysis/lap-count-review-20260921`.

`rr64_race_end_trace` now adds circuit history when `RR64_DIAGNOSTICS` or
`RR64_RUNTIME_TRACE` is nonzero. The guest dispatcher calls the observer after
the update/synchronization boundary; recovery only sets a marker. Unchanged
observations emit nothing. Human route-segment, lap, identity, terminal-state,
and finish-parameter changes coalesce into one bounded snapshot per observation.
The existing terminal snapshot still includes every actor. Both paths share the
16-entry single-producer queue and UI drain; no guest-state writes, allocation,
or file I/O are performed by the producer. Replay is excluded.

The UI publishes the latest authoritative human mask so remote slots can be
traced without copying transport strings on the guest thread. That mask can lag
a membership transition. `observed_frame` counts dispatcher observations, which
may include online waiting frames; it is not a simulation tick. Marker-crossing
reason64 is a sampled parameter crossing on the finish curve, including a
verified duplicate starting curve. It does not itself award or prove a lap,
and identity changes suppress it. Skipped segments are still recorded as segment
changes, so interpret marker reasons together with progress and recovery data.
Guest `total_ticks` is recorded as raw bits plus its float interpretation, not
as a wall-clock duration. Queue overflow is reported explicitly.
