# Split-screen world rendering audit and candidate

The user accepted the correction build's performance and rider/bike MAX LOD,
but reported distant terrain and scenery pop-in. Its captured log contains sustained
active-race intervals near 60 updates/sec and presentation intervals near 16.67 ms.
Those are measured software rates, not physical scanout measurements. The log
snapshot is preserved in analysis/splitscreen-world/baseline-runtime.log; the user
feedback is the visual evidence. No new game launch is authorized until ready.

## Findings and implementation

The old distant-world drivers stored a single camera's command/matrix destinations
per graphics buffer, used camera-zero projection/view bank addresses, and refused
a second issue in the same graphics epoch. Their static geometry was already cached.
Simply enabling their scene gate cannot make split-screen correct or efficient.

The new terrain and scenery drivers keep the two existing asset copies (one per
original graphics buffer), shared across all local views. Each graphics buffer now
has four disjoint command/matrix regions and per-view issue guards. IDs incorporate
viewport identity so different cameras cannot match each other's transforms. Camera
projection/view and normalization addresses use the original per-view bank strides.
The far-plane readiness certificate is indexed by view, source bank and render slot.
No command/matrix buffer is allocated per frame or per racer.

Animated scenery texture bindings are frozen after the first view submits that
buffer in an epoch. Later views share those immutable bindings and do not rewrite
textures already referenced by an earlier screen. The shared animation clock advances
at most once per epoch. Course-island filtering and per-pass stock deduplication
remain in place, rather than enabling disconnected map islands by distance.

A six-plane culling object is computed once per camera pass. Model boxes are tested
against those planes instead of transforming eight corners through three matrices
for every cell/placement. A relative boundary tolerance retains marginal boxes.
This reduces CPU arithmetic without a polygon reduction or nearer distance cutoff.
It is not a measured FPS improvement until the user tests the candidate.

Scope: full cached terrain and placed scenery, including their cached detail and
billboard transforms. Dynamic traffic/pedestrian actor promotion still uses its
existing single-camera eligibility; extending that separate pose system is not
claimed here. Rider/bike MAX LOD and bumper fixes are retained.

## Verification and remaining work

Tests cover all four camera producers against original guPerspective calculations,
all four terrain/scenery submissions in one graphics epoch, unchanged earlier-view
command/matrix regions, distinct stable IDs, existing full-capacity buffers, slot
lifetimes, stock deduplication, disabled policy, and culling against the reference.
No original ROM-generated functions were changed. Exact dependency override hashes
and clean frontend-patch reconstruction are rechecked while packaging.

Live acceptance pending: same two-player course, same graphics settings, look toward
the previously expensive terrain, check both screens for scenery/distance, then
jumps, pause and finish. Capture diagnostics for comparison with the baseline.
If this remains expensive, use the new evidence to separate culling CPU work,
display-list decoding and GPU work before changing detail or visibility.
Nothing is published. Keep the accepted correction folder available for comparison.
