# Multiplayer catch-up removal

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
