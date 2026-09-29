# Input time and correction ownership

Logging02 acknowledged the newest contiguous input sample after a single native
host update. Reconciliation then applied a completed host state and replayed
every pending guest update in full. An acknowledgement could stay unchanged
while held host controls advanced the rider. Replaying the same entire tail
then counted part of that movement twice. Different update rates and packet
delivery bursts made sequence number an invalid substitute for simulated time.

Protocol60 adds a bounded1–250000microsecond native duration to each command
and a64-bit elapsed simulation cursor for each host-owned player. The cursor
starts with the first contiguous command, advances through held-input updates,
and is published only after native simulation completes. Acknowledgements retire
only complete input intervals. A separate entered-edge frontier preserves short
taps and eject requests once even when their interval is only partially complete.
Missing inputs expire after250ms, independent of host update rate.

Each guest journal entry retains its cumulative command endpoint. A replay plan
compares these endpoints with the authoritative cursor. An elapsed interval runs
no physics; a partly consumed interval runs only its remaining duration. Full
intervals keep their original timing bits. A partial interval scales delta,
delta-squared, scaled-delta and inverse-delta by their appropriate factors,
retaining the native pass count and historical end clocks. Ordered loading
events and journal entries remain present even when physics is skipped.

Native non-RDRAM state associated with an already-entered action must be carried
with validated ownership; suppressing its edge alone must not discard manual
eject protection. This state remains local to the predicted rider. Host-owned
outcomes and unrelated actors are not replaced by an old private replay.

This remains temporal resampling of the existing native updates. A host update
which spans more than one short input interval selects the latest entered
controls and preserves edges; it does not subdivide native physics at every
network input boundary. Native collision, timing precision and camera behavior
still need paired gameplay acceptance. The separate mounted render offset is
bounded and reset on crash/recovery/identity changes; it never changes physics.

Private replay cases are now ABI version5 because command layout changed.
Original saved cases and Logging02 binaries remain preserved. No old private
case should be silently interpreted with the new layout.
