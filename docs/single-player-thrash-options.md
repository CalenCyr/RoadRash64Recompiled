# Single-player Thrash options

Thrash uses its original single-player route, bike and character screens with
the same custom race choices as local multiplayer: exact AI racer count (0–10),
bike level (Match Track, levels 1–5, Scooter, Insanity), Custom Cop Mode and its
optional AI cops. Native difficulty, police, traffic and pedestrian choices
remain available. Raising one density no longer lowers an unrelated density.

Custom Cop requires the player to choose the cop bike and a cop rider. All five
cop profiles are available without changing unlock flags. Existing cop rules,
baton inventory, parked start, manual shout/siren controls, bust count and victory
handling are shared. With zero AI opponents this is free play, not an instant win.
AI Cops defaults off; when on, the Cops density selects their number (0–3).

## Ownership and persistence

`rr64_thrash_options.cpp` owns only the native solo menu bridge and its explicit
context. Entering offline `func_800256C0` activates it. Mode changes retain it
through Thrash loading/racing/results (17–21) and clear it outside that flow.
It does not enable `local_players::active`, split-screen or online input paths.
Big Game, demo and multiplayer retain their own menus and presets.

`rr64_local_race_options.cpp` owns the shared bike/roster rules and persistence.
The solo preset is `thrash-race-options.cfg`, separate from local multiplayer's
`local-race-options.cfg` and online session settings. Game-thread callbacks only
publish atomic preferences; the existing UI writer performs file operations.

## Native boundaries that matter

- Solo options table: `8009EB4C`, eight 36-byte records plus sentinel. The bridge
  allocates a separate twelve-record table rather than overwriting the sentinel.
- Solo stage: `8009ECA0`; stage 0 is route/options and stage 1 is selection.
- Solo bike cursor: `800A66B0`, not multiplayer's `8009EF3C` array. Tier overrides
  change bike-list reads only; selected route level remains native.
- The native text buffer is `sp+50..67`, so labels must fit 24 bytes including NUL.
- Apply exact roster before menu `26158` and initializer `6C488`, before total
  count and actor allocation at `6CB8C`. Set humans to one first; a previous
  multiplayer count must not select this race's human mask.
- At most eleven ordinary entrants plus three AI police fit fourteen actors.
  Custom Cop with AI cops off excludes the police allocation entirely.
- Enable race type 8 immediately before `6C6A8` after the stock cop-resource
  branch. Enabling it at initializer entry also alters the scenery allocation.
- Custom Cop bypasses the original all-police cheat redistribution; the explicit
  AI Cop option owns that choice. The cheat remains unchanged in ordinary Thrash.
- Normalize cop/rider pairing after the native default-rider assignment and
  before the committed rider/preview call at `26E38`. Validate at `26F1C` before
  cleanup or loading; do not touch multiplayer ready arrays when rejecting solo.

The detailed source trace and private verification reports are under
`analysis/single-player-thrash-options-20260921`. Those reports distinguish
offline coverage from the still-required visual/gameplay acceptance run.
