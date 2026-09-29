# Online native sound ownership repair

## Logging02 weapon-switch follow-up

The owner's Logging02/protocol59 live test still hears the other rider's weapon
selection cue globally. This is a separate native producer missed by the first
repair: `56000` chooses effects `E8/ED/E9/EC/EB` from the selected weapon and
passes the fixed float at `80005978` directly to mixer gate `58600`. It never
calls any of the five positional gain routines below. Its only native callers
are the mounted weapon-cycle path at `4090C:40F80` and rider-control path at
`645E4:64E98`.

Three additional hooks bind the switching rider before either caller's delay
slot replaces the rider pointer with the weapon ID, then consume that binding
at the immediate `58600` entry. A binding requires the same guest mapping, CPU
context, expected stack and native cue effect; it is consumed even on rejection.
Only the volume argument changes. Native weapon selection, sound choice, pan,
priority and the original zero-volume allocation gate remain intact. The local
listener, offline/local split screen, menus and private replay keep stock gain.
Unscoped mixer calls retain their original arguments.

Remote cue volume follows the existing `1295C/59588` horizontal distance math
and the original ROM constants at `80005C90/94/98`. Both native caller bindings
and the mixer hook are explicitly omitted by the private prediction generator,
so historical replay does not touch this presentation state or consult live
network rules. This adds no positional AI engine feature or protocol change.

The expanded fixture executes fourteen complete original native routines plus
both original call-site instruction sequences, including their delay slots.
It checks every weapon ID1–14 through both callers for all twenty local slots
in 2/4/14-player representations, compares volume against the actual native
distance routine, preserves effect/pan, and checks unrelated/invalid/replay
calls. The native mixer gate now executes too; only its external mixer service
is modeled. Far cues allocate no voice. Evidence is preserved separately under
root `analysis/online-followup-log02-20260927/audio/`; the first investigation
below remains historical. Candidate injection is not regenerated-production
evidence. Audible acceptance still requires a new authorized paired run.

## Initial engine/attack investigation

September 27, 2026 follow-up to published 1.4.1/protocol 58. The owner reports
hearing the connected rider's engine and attacks at local volume while separated.
The paired Windows/Linux captures are preserved under root
`analysis/online-followup-20260927/`. Both identify source
`2ad4f79b9d91d92531c32d57289ce1a5e3bc9ee7`; their sync logs contain rider positions
but no native sound-gain trace or audible recording.

## Confirmed native ownership defect

Native gain routines `59324`, `593F0`, `594BC`, `59588` and `59648` treat a
nonnegative actor `+8` as a local listener. Pair routines grant this privilege
when either participant is human. This is correct for stock shared split-screen
audio, but online retains several human actors while each machine presents one
local rider. Every such remote human therefore receives the same 140.8 fixed
gain, regardless of separation.

The existing nonlocal branch measures horizontal distance from actor zero's
rider body (`800D8654`, actor `+E4`, position `+8C`). Its original ROM constants
give `clamp((1 - distance / 130) * 98.56, 0, 255)`. The stock bike engine
producer `571DC` obtains its gain through `59648`; attack swing `56208` obtains
gain through `59588`. No new distance curve or sound asset is needed.

## Candidate correction

`rr64_online_audio.cpp` changes only the branch register used by those five
gain routines and the listener register immediately before their original
distance call. Full local gain belongs to this machine's mapped native actor;
remote sounds use the existing native distance branch. Guests use their own
rider body as listener, including the local-to-zero mapping for sessions above
four players. Pair effects involving the listener keep native local gain.

The hook requires an active connected race and a valid reciprocal local
actor/body identity. Offline, local split screen, menus and isolated prediction
retain the original behavior. Actor fields, controller selection, sound-cache
ownership, native production gates, volume/pitch operations and expiry remain
unchanged. In particular, this does not add engine production for AI riders or
introduce the deferred positional AI engine feature. Proximity voice is separate
and unchanged. No packet format or importer change is needed by this repair.

Existing sustained engine handles receive their normal native volume updates;
the offline fixture exercises near → far → near on the same handle. No global
mute or mass handle cleanup is introduced.

## Verification

The initial `RR64OnlineAudioSmoke` executed twelve extracted native functions with private
supported-ROM constants, plus an unhooked negative control. It covers all five
gain routines, every local slot in 2/4/14 representations, nearby/distant
effects, local participants in paired effects, offline/menu/replay bypasses,
invalid listener identities, existing engine-handle volume updates and actual
native attack-swing dispatch. Only the external mixer/voice services are modeled.
Run it with the user's private `build/roadrash64.us.z64` as its sole argument.

Initial extraction required all thirteen hooks in generated production code;
the expanded weapon-switch fixture now requires sixteen.
The optional `--candidate-config` fixture mode inserts the exact configured
statements before CPU regeneration and is explicitly identified in its proof;
that mode alone is not production integration evidence. Detailed results and
source hashes are in root `analysis/online-followup-20260927/audio/`.

These checks do not establish audible or real Internet acceptance. Keep the
game closed until fresh **ready**. On the next authorized paired test, verify
local engine/attacks, nearby remote sounds, separated remote sounds, each guest's
listener, and unchanged local split screen. Nothing was launched or published
for this investigation.
