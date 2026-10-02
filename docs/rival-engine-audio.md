# Positional rival engines

This feature covers AI opponents in solo and local multiplayer, and
nonlocal human and AI bikes online. It builds on the original ROM-loaded bike
sounds. No new sound assets or SDL_mixer dependency are distributed.

## Listening behavior

Audio > Game Audio > **Rival Engines** enables the managed engine sounds and
defaults to On. Off requests a stop for active rival loops at the next game
audio update, rejects further owned script-child voices, and skips subsequent
listener, bike-profile and rival sound processing. The choice is saved in
`sound.json` independently of **Rival Engine Volume**, which retains its 35%
default. Existing settings without the switch remain enabled.

The volume slider still fades sounds to silence at 0%; use the switch to skip
the rival update work as well. Main Volume still applies. The native local
player engine path remains intact, including each local split-screen player's
own engine. Online nonlocal native engines remain suppressed when Off so they
cannot return as unpositioned full-volume sounds. Crash, weapon, music and other
effects remain available.

Rival engines reuse the original shared engine sound bank. Off avoids rival
voice creation and updates; it does not unload samples needed by the player's
engine. There is no separate rival asset package to load or unload, and these
offline checks do not establish a measurable FPS or device-performance gain.

The manager selects up to three nearby riding bikes. Sound stays on each bike;
the listener follows the local rider's body when detached. The view direction
determines stereo left/right. Volume reaches the slider's full level within
12 world units, then fades smoothly to zero at 320 units. Changes in selection,
volume and pan are smoothed. Selection favors an
already playing bike slightly to avoid rapid switches between similarly distant
rivals. Original bike profiles and engine state determine sample and RPM pitch.
The native traffic Doppler calculation adds a bounded, smoothed passing pitch
offset to the continuous engine loop; short race-start cues keep their authored
pitch.

Online peers listen from their own rider. The original nonlocal-human engine
dispatcher is suppressed so it cannot produce a second centered engine. Local
split-screen shares one speaker mix: each AI bike uses its strongest local
listener, rather than creating a voice for every viewport.

## September 29 audibility follow-up

The 1.4.2 implementation incorrectly accepted only bike model indices below
17. The ROM sound table has 32 pointer entries, including later choppers,
Scooter, both Insanity models and cop variants. The same restriction also ran
while finding the local listener: selecting an excluded chopper could silence
all managed engines, even rivals whose own models were accepted.

Listener authentication now validates actor/bike/body ownership and position
independently of sound metadata. Sources accept all valid entries in the native
table, checking their four effect IDs against the loaded effect bank. Reserved
model 22 points to unrelated packed data and is rejected as a sound source.
The original engine producer still chooses the ROM-authored sound family and
throttle/RPM pitch. Some models intentionally share sound samples; the change
does not invent unique recordings for each model.

The old fixed gain of 98.56 also limited maximum rival volume to 70% of the
native local engine baseline of 140.8, before applying the short squared
distance fade. The follow-up reads that native baseline from the ROM, reaches
the slider's full gain within 12 world units and fades smoothly to zero over
320 world units. These are game coordinates, not a claim of physical metres.
The saved slider value and its 35% default remain unchanged. At maximum, a
nearby rival of the same model and throttle state receives the same input gain
as a local player's engine; actual perceived balance still needs listening.

The three-engine cap, four reserved effects rows, no-steal rule and bounded
per-frame scan remain in force. Larger audible range does not allocate a voice
for every distant racer. No performance claim for Steam Deck follows from the
offline producer benchmark.

Remote pitch uses the guest's native engine simulation after authority restores
the bike's movement. The native update still computes wheel/gear speed and the
smoothed engine state for active remote riders. Exact host throttle, gear and
RPM are not sent as audio fields, so their pitch can differ between peers. This
feature does not change packets or claim sample-identical engine audio online.

The passing effect uses the original traffic routine's horizontal relative
velocity calculation (`80057D48..80057DB4`) and ROM pitch constants. It reads
the source bike's velocity and the chosen listener's bike or detached body
velocity, rather than estimating motion from successive position corrections.
The same listener drives gain, pan and pitch. Listener changes, attachment
transitions and large teleports reset stale pitch offsets; invalid motion or
coincident positions retain the original RPM pitch. The scoped `8005753C` hook
adds the offset exactly once per native engine update, without changing local
player engines or unrelated effects.

## Native mixer protection

`rr64_rival_engine.cpp` calls original producer `800571DC` in a scoped context.
Its overrides do not apply to the local engine, attacks, weapon changes or other
effects. The stock `58600` gain threshold of 18 rejects quiet engine starts;
managed rivals permit gains above 1 so fade-in and a low volume setting work.

Admission examines the live native effects rows, not merely the racer cache.
It permits at most three owned rows and leaves four effects rows free when
admitting a voice. Added voices use priority zero and cannot steal another row.
Deferred releases remain occupied until the native audio worker frees them.
Original race effects may steal these low-priority engines when necessary.
Music's reserved rows are excluded from this budget.

Ownership uses both row address and native handle. A reused row cannot be stopped
as if it were an old rival sound. Script-child allocations share the same budget
and ownership safeguards; current engine scripts do not create children. The
manager uses fixed storage and no per-frame file output or heap allocation.

Recovery, invalid owners, dismounts, large teleports, pause, menu changes,
disconnect and highlight playback release the managed voices. Private prediction
does not emit, move or stop these sounds. Highlight audio remains governed by
its existing lifecycle; this feature does not replay recorded engine audio.

## Code and verification

- `native/src/rr64_rival_engine.cpp/.hpp`: source/listener selection, scoped
  native hooks, voice admission, ownership and lifecycle.
- `native/src/rr64_rival_engine_config.cpp/.hpp`: saved Audio option.
- `config/roadrash64.us.toml`: native dispatcher, producer, allocator and cleanup
  hook sites; `generate_prediction_frame.py` excludes live audio hooks.
- `native/tests/rr64_rival_engine_smoke.cpp`: production manager plus extracted
  original producer, dispatcher, allocator, volume, pitch and pan operations.
- `native/tests/rr64_rival_doppler_cases.hpp`: native traffic arithmetic oracle,
  approaching/receding motion, smoothing, fallen listeners, teleports and local
  and online listener mappings.
- `native/tests/rr64_rival_engine_config_smoke.cpp`: real Config save/load,
  compatibility with older settings, invalid values, callbacks and file backup.

Initial evidence is retained under root `analysis/rival-engine-20260927/`;
the audibility follow-up uses `analysis/rival-engine-followup-20260929/`. Offline
verification checks the real producer and native mixer contracts without opening
an audio device. It does not establish audible balance, physical stereo output,
Internet acceptance or Steam Deck frame times. Those require listening/runtime
confirmation. Windows and Linux build results are recorded there when complete.

The September 29 follow-up compiled the complete game on Windows and Linux.
Both passed eleven relevant suites, including 30,180 rival-engine checks,
45,525 online-audio checks and the retained campaign save/label tests. Separate
writer/reader processes also passed the save fixture. Isolated negative controls
restoring the old model guard, old gain ceiling or removing Doppler each failed
the corresponding new regression. The long bounded producer loop allocated no
host heap memory; this remains offline evidence, not a hardware performance or
listening result. No game or audio device was opened for these checks.

In 1.4.2, online peers must share protocol65. Listening and hardware coverage remain limited.
