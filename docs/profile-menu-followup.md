# Profile names, offline opponents and ultrawide follow-up

Private follow-up to 1.4.0. No publication or visual acceptance is implied.

## Player names

Solo Thrash replaces its stock actor-zero name after the native `8006C4E0`
copy, before the rest of race initialization consumes it. The hook requires
the solo Thrash session; local and online rosters retain their own names.

New Big Game name entry seeds the Controls player-one profile after `7280C`
has cleared the field. The campaign name is `800D6A48`, eleven characters
plus a terminator. `8009EF9C` is the keyboard letter grid and must never be
used as a name buffer. Unused edit cells remain underscores until the native
confirmation strips them. The editor cursor remains within indices 0–10.
Loaded saves and subsequent manual edits are not reseeded. Existing profile
sanitization supports uppercase letters, digits and spaces; this change does
not add punctuation to the original on-screen keyboard.

`RR64ProfileNamesSmoke` runs the generated native initializer and Thrash copy
with the production hooks, checking names, bounds, keyboard preservation,
mode gating and preservation of existing campaign data.

## Prevent Opponents from Moving

The offline Cheats tab adds a toggle that holds mounted computer-controlled
racers. The common `6AFFC` update uses an audio-only callback while frozen,
and skips bike/rider integration together. No rank, finish flag, route
progress, saved control lock or actor callback pointer is rewritten.
Detached/crashed/terminal actors continue through native recovery.

The predicates reject human-owned actors, invalid reciprocal entity links,
online play, prediction, private render memory, attract mode, highlights and
menu transitions. Turning the toggle off resumes normal AI updates. As with
other cheats, achievements are disabled while a cheat is enabled. This is the
"prevent opponents from moving" alternative requested by the user, not a
forced first-place score override.

`RR64OfflineOpponentsSmoke` checks production guards and the exact allowed
memory-write set, including all live race modes and toggle-off behavior.

## Ultrawide and local menu input

The frontend already selected output-window expansion for ultrawide, but
RT64 capped that target at 16:9. Expansion now follows the actual window
aspect while fixed 16:9 remains separate. The existing menu framing policy
is unchanged. `RR64VideoModeSmoke` includes 3440×1440, 2560×1080, 5120×1440,
invalid/narrow dimensions, and ultrawide world-culling boundaries. The
maintained RT64 dependency patch, override and checksum include the fix.

Local Multiplayer retains shared port-one input when no player cards are
assigned, so a keyboard-only user can select a race or go Back. Explicit
controller/keyboard assignments still use independent ports. The native
ordinary multiplayer modes already support one human against AI; no forced
extra player is needed. See `local-multiplayer-keyboard-entry.md` for the
trace and limits. `RR64OnlineMenuRefreshSmoke` exercises the actual UI-entry,
device-query and profile-resolution functions with hardware/network doubles.

These offline checks cannot establish live controller behavior or visual
framing on the reporter's monitor. Await a fresh **ready** before launching.
