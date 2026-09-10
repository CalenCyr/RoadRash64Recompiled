# Multiplayer catch-up removal

## Local out-of-bounds timeout (2026-09-09)

The separate route timeout in 8003FE48 was still active for ordinary local
players. Its bike+858 timer also supplies the HUD countdown. The entry hook now
checks `rr64_local_player_roaming` for every populated local slot (one through
four), clears that timer and skips relocation while the rider is mounted with
finite, usable durability. This does not depend on choosing a cop or a specific
split-screen layout. AI and online ownership are excluded from this new guard.

Detached riders, active crashes, busted players and exhausted durability retain
the original recovery/death path. The accepted custom-cop roaming guard and
manual-eject health restoration remain in place. The shared relocation helper
is not removed because normal recovery still uses it.

Production build and local-options regression checks pass, including all four
slots, inactive slots, malformed player counts, crash/eject/bust/health cases,
countdown clearing and disabled/online paths. In-game off-route testing of this
new change is pending; the Performance-01 session predates it.

## Earlier catch-up change

The USA routine 8003FD60 is now stubbed in the recompilation configuration.
Its sole caller at 8006B128 is gated on the player count at 800A6578 being
at least two and the human-racer flag. The routine compares ranked route
progress, calls 80068E20 with a leader-relative destination, then charges
a penalty after successful relocation. Removing this dedicated routine
removes that forced catch-up and its associated penalty.

Ordinary recovery routine 8003FE48 and shared relocation routine 80068E20
are unchanged. Generated bodies were compared byte-for-byte against the
pre-change inspection copies. Single-player does not call this catch-up path.
This also affects online play when it uses the same original multiplayer path;
no network synchronization or replicated-rider logic was changed.

Full Release build passed. Runtime acceptance is pending: with two players,
leave one behind and drive beyond the old teleport threshold, then verify
crashing/off-road recovery still works. Test the retained shared HUD placement
correction too. No game launched and nothing published.
