# Split-screen triangle bookkeeping optimization

Measured change: replace packed bitsets with byte flags for per-packet visited
vertices and screen-bound readiness. Preserve visitation order, triangle order,
scissor/culling decisions, resource transitions and every output field. No geometry,
distance, texture, or camera policy changes. This is not a static-geometry cache.

Three paired warmed CPU benchmark runs (5000 packets, 40 triangles/packet):
baseline medians 2.5359, 2.5586, 2.5852 ms; candidate 2.2868, 2.2761, 2.5089 ms.
Median-of-runs improvement is approximately 10.6% for this triangle microbenchmark.
This is not a whole-game FPS gain; even a similar live improvement is unlikely
by itself to close the entire 60 FPS gap. Original and changed output dumps have
identical SHA256 5c075cd35332a3f0e0c4ffc6b29dcdad8ed836eb22f2669845161d6c20046aeb.
The parity suite covers 768 scenarios, 4674 checkpoints and 33 handler cases.

Detailed profiling remains opt-in. The existing live capture identified roughly
4.0 ms vertex handling and 5.3 ms inclusive batch work per display list during one
slow interval. These overlap other timings and profiling overhead is unmeasured.
Repeated static-world setup and actual course ownership still need deeper work;
do not claim a decoded-geometry cache or course-filter correction was implemented.

Game build and repeated parity/benchmark checks pass. Clean RT64 patch reconstruction
is verified during packaging. No game launched; no publication. The smooth rider-LOD
correction build and the full-distance baseline are preserved for comparisons.
