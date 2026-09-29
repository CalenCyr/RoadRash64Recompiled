# Positional rival engines

This feature covers AI opponents in solo and local multiplayer, and
nonlocal human and AI bikes online. It builds on the original ROM-loaded bike
sounds. No new sound assets or SDL_mixer dependency are distributed.

## Listening behavior

Audio > **Rival Engine Volume** controls only the managed rival engines. It
defaults to 35%; 0 fades them out. Main Volume still applies. The native local
player engine path remains intact, including local split-screen players.

The manager selects up to three nearby riding bikes. Sound stays on each bike;
the listener follows the local rider's body when detached. The view direction
determines stereo left/right. Volume falls with distance over a 130-unit range,
and changes in selection, volume and pan are smoothed. Selection favors an
already playing bike slightly to avoid rapid switches between similarly distant
rivals. Original bike profiles and engine state determine sample and pitch;
this is positional stereo, without a new Doppler pitch simulation.

Online peers listen from their own rider. The original nonlocal-human engine
dispatcher is suppressed so it cannot produce a second centered engine. Local
split-screen shares one speaker mix: each AI bike uses its strongest local
listener, rather than creating a voice for every viewport.

Remote pitch uses the guest's native engine simulation after authority restores
the bike's movement. The native update still computes wheel/gear speed and the
smoothed engine state for active remote riders. Exact host throttle, gear and
RPM are not sent as audio fields, so their pitch can differ between peers. This
feature does not change packets or claim sample-identical engine audio online.

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
- `native/tests/rr64_rival_engine_config_smoke.cpp`: real Config save/load,
  compatibility with older settings, invalid values, callbacks and file backup.

Evidence is retained under root `analysis/rival-engine-20260927/`. Offline
verification checks the real producer and native mixer contracts without opening
an audio device. It does not establish audible balance, physical stereo output,
Internet acceptance or Steam Deck frame times. Those require listening/runtime
confirmation. Windows and Linux build results are recorded there when complete.

In 1.4.2, online peers must share protocol65. Listening and hardware coverage remain limited.
