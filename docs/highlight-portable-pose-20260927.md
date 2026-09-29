# Guest replay model allocation correction

The Logging 02 paired capture narrows the missing guest replay actors to the pose compatibility check. The Windows host accepts 3,720 detailed actors. The Linux guest rejects the same 3,720 attempts; its four bounded first-rejection records report matching record/bone counts, LOD and source bank, with differing topology hashes. The guest had also completed an earlier local race in this same process. This is evidence of independent heap layouts, not a platform serialization failure.

The cause is in the previous pose identity: it reused the local graph hash, which includes every native record's signed `next_delta`. Native constructor `8001AD24` allocates these linked renderer records. Their byte spacing is allowed to differ between peers or after earlier local gameplay, even when the source model and ordered bones are identical. The earlier regression relocated only the scene node and therefore missed this case.

The correction changes only the portable highlight pose identity. It hashes ordered record types, native hierarchy flags, source-template offsets relative to the root, and transform ordinals/counts. It excludes runtime renderer-record and pose allocation spacing. Source assets may relocate together; exchanging source bones or hierarchy still changes the identity. Local renderer topology/cache checks retain their original stricter hash. Existing pose pointer, overlap, graph-cycle, source-bank, finite-value and write-restoration checks remain enforced.

Replay codec format RHL3 rejects older blobs whose pose identities had the old meaning. The combined private candidate uses protocol 60; both peers must match. No gameplay, local multiplayer, camera behavior or asset layout is changed by this repair.

Offline evidence is under root `analysis/online-followup-log02-20260927/highlights/`:

- The preserved pre-fix helpers reject all 20 actors when 60 real saved-scene model graphs are independently reallocated with different, including negative, link distances.
- Corrected Windows and Linux helpers accept all 20 actors at the same boundary and restore the entire guest memory image after playback. Windows passes 8,784 checks; Linux passes 8,786 while decoding the exact Windows-produced compressed playlist.
- The ROM-free `RR64HighlightPoseSmoke` passes 542 checks across both actor types, all LODs and source banks, graph/pose/resource relocation, hierarchy/source-order mutations, invalid/overlapping pointers, cyclic links, real codec transport, and obsolete RHL2 rejection with a valid compression checksum.

These fixtures reproduce the logged failure mechanism. They use an older retained ten-racer scene, not RDRAM from the failing two-player session, and do not establish live visual acceptance. No game was launched during this investigation.
