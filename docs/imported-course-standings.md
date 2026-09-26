# Imported-course standings after crash recovery

The September 26 Candidate01 owner run reported a persistently low race place
after an early on-road Rainbow Road crash, with the opponent-freeze cheat off.
That release-default run did not record individual route progress. Do not claim
its exact crash sequence has been reconstructed.

An independently reproducible defect exists in imported AI progress. Native
`8005A9DC` reacquisition may project a recovered AI racer onto an earlier curve.
`800674C4` advances only forward, so a short backwards transition becomes almost
a full circuit. The imported signed-progress adapter previously covered humans
only. AI then acquires unearned distance/lap credit, and the unchanged native
standings routine legitimately sorts the human behind that incorrect distance.

`rr64_experimental_course_route.cpp::progress_owner` now includes non-police
native AI for offline play and live authoritative-host races. Guest replicas and
private prediction do not gain AI ownership. Human handling, normal-course
behavior, physical recovery and the native standings/HUD routines are preserved.
Protocol58 prevents mixing peers using these simulation rules with protocol57.
Course assets and importer version1.0.1-c39 are unchanged; no reimport is required.

The native fixture in root `analysis/race-rank-20260926/rank/` executes the entire
original standings routine, distance/lap evaluator and the HUD rank argument
path. Over all16 packaged courses, a controlled backwards transition for eight
AI racers reproduced9/10 in the old source and yielded1/10 with the fix. It also
checks normal forward progress, terminal flags, ownership exclusions and an
unchanged stock-course whole-memory fingerprint. Both expected-result variants
pass224 scenarios. This tests route consequences, not impact physics or the
unrecorded owner's live sequence.

Optional existing race-end/lap diagnostics now report both `table_rank` and
`race_place` from their actual native fields. Capture remains disabled by
default. The recorder's enabled, disabled and diagnostics modes passed offline.
A ROM-free two-process loopback test passed protocol58 lobby/selection/snapshot
exchange. Neither offline suite establishes live gameplay or Internet quality.
