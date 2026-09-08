# Shared terrain bounds candidate

Scenario: user reports Local Multiplayer, Thrash, Level 1 Race 1; captured two views.
Reference session: analysis/course-diagnostic/session-20260907-200641-480.

Implemented: compute immutable terrain center/extents once during asset loading,
reuse across all views, and specialize the six-plane test for the validated terrain
root transform (identity XY, half Z, XY translation). Create the full output matrix
only for retained cells. Each camera still supplies its own clip planes. No distance,
triangle, texture, stock ownership, billboard, or dynamic actor policy changes.
Bounds are cleared on mapping/heap replacement, alongside their asset owner.

Validation: 20,000 rotated/translated camera comparisons against the preceding
general-matrix test agree. Existing original-helper, four-view buffer ownership,
full 4900-cell capacity and enabled/disabled diagnostic tests pass. Game builds.

Six alternating-order benchmark pairs, 980,000 visibility tests per variant:
general median 17.410 ms; specialized median 7.159 ms, about 59% lower time.
Both paths report 385400 visible boxes in every run. This synthetic test uses one
camera and fixed box sizes; it is not a whole-game performance measurement.
Equivalent savings at 4900 tests are only about 0.051 ms per camera pass. The main
28.4 ms display-list bottleneck remains unresolved. Live visual testing is pending.

Larger work still needed: RT64 drawData vertex streams mix immutable positions and
colors with world/view/light/fog indices, projected CPU positions, texture transforms
and per-workload matching metadata. Reusing those complete records across views is
incorrect. A persistent mesh path would require separating source geometry storage
from per-draw instance attributes through upload, shaders, matching and CPU bounds.
That architecture has NOT been implemented by this small visibility optimization.
The existing packet compiler already merges adjacent geometry up to 127 vertices;
increasing the original 32-vertex packet limit is not an unused optimization here.

Diagnostic switches and session launcher are retained for an equivalent comparison.
No game launch until ready. No publication.
