# Guest correction image-copy reduction

September 27 follow-up. Paired 1.4.1 logs show a guest update cadence lower than
the host's. They do not contain reconciliation stage timings, so they cannot
attribute that gap to this code. An isolated Windows benchmark measured two
specific avoidable costs in the existing correction transaction.

Before this repair, `evaluate_history_replay` copied its completed private
8 MiB resource image into `final_memory`. The commit stage then restored every
retained 8 MiB journal frame into scratch solely to inspect its small resource
queue inventory. The full restore was unnecessary because that exact private
image had already been available when each corrected frame was captured.

The candidate moves the completed private image into the result and records
each frame's `ResourceInventory` alongside its native metadata. Commit retains
the same round/sequence checks through `GuestJournal::contains`, validates all
metadata counts, and consumes the existing approval ticket only after replacement
history is prepared. No live guest image is replaced, no replay step is skipped,
and the page allocator, memory budget and history-retirement policy are unchanged.

The isolated benchmark runs the actual evaluator with a synthetic single-page
change per pending step; it separately measures the existing commit inventory
work. Twenty-four repetitions per row on this Windows machine gave:

| Pending steps | Before evaluate + inventory | After evaluate + inventory |
| --- | ---: | ---: |
| 0 | 2.878 ms | 1.948 ms |
| 4 | 3.947 ms | 2.810 ms |
| 16 | 7.267 ms | 4.410 ms |
| 64 | 20.956 ms | 12.285 ms |

Each evaluation allocates approximately 8 MiB fewer bytes. The new small
inventory vector replaces one allocation, so allocation counts remain unchanged.
This is a synthetic storage-cost measurement, not a native physics benchmark or
a claim of Linux frame-rate improvement. Preserved original source, baseline
benchmark executable/results, optimized results and offline checks are under
root `analysis/online-followup-20260927/`, in `before/replay-copy/` and `audio/`.

The journal, capture and connected reconciliation fixtures pass. New checks
compare every cached inventory against the independently restored corrected
frame, including changing worker ownership across steps; prove final-image
ownership transfer; reject missing inventory or wrong journal round before
approval; and retain existing replay-failure/ticket-rejection atomicity checks.
Real peer cadence and correction timing remain to be measured during the next
freshly authorized test. No game was launched for these checks.
