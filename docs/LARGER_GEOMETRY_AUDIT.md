# Larger static geometry audit

Headless audit of the user's installed ROM; no game launch or publication.
Results: analysis/geometry-audit/results.log. Original asset batching parity also
passes in analysis/geometry-audit/asset-parity.log. The reusable --geometry-audit
option lives in native/tests/rr64_world_asset_batch_smoke.cpp and outputs counts,
not game assets. No renderer behavior was changed by this audit.

One-time packet compilation is already active:
- Terrain: 16998 packets -> 10978; 346443 -> 326585 vertices; 213170 triangles retained.
- Unique scenery models: 1876 packets -> 833; 50391 -> 47341 vertices; 39925 triangles retained.
- Scenery: 373 models instantiated 4257 times; 1958 billboard placements.

Connected terrain components have 3258, 8, 8, 8, 4, 3 occupied cells. Therefore
selecting the largest component retains 3258/3289 = 99.06% of occupied terrain.
This exactly explains the recorded 31 excluded cells in the slow live capture.
It does NOT prove that all retained cells are unrelated to the selected course.
The filter is geometric connectivity, not authored course ownership. Derive
course bounds/route ownership from the original race data before removing cells.

RT64 vertex records currently couple static positions/colors to per-view matrix,
light/fog indices and CPU-projected positions. A cache of those complete records
cannot safely serve multiple cameras. A deeper reusable-mesh path must separate
immutable geometry from per-instance/per-view attributes and retain material,
transparency, texture animation, billboard and draw-order behavior. Blindly
reordering all geometry by material is not authorized by these findings.

Priority: resolve authored course ownership to avoid irrelevant work; then assess
persistent static vertex/index storage with per-view instance transforms. Keep
billboards and animated material bindings on correctly owned paths. Existing
packet compilation and triangle batching should be retained, not duplicated.
The earlier measured byte-flag triangle optimization remains intact. No larger
cache or course-ownership fix is claimed as implemented yet.
