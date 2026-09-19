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
