# Frame-pacing candidate 02: VSync settings handoff

Implemented and built on Windows and Linux. The eight targeted offline test
invocations pass on both platforms. Private candidate only; no game launched and
no publication. See `analysis/frame-pacing-02-20260920/candidate.json` for the exact
Windows executable identity and file inventory, and `*-checks.json` for results.

## Evidence and scope

Candidate 01's 23.41 ms maximum was recorded in the paused window containing a
D3D12 VSync-on to VSync-off transition. All 600 camera/world comparisons in that
window were unchanged. The authored source maximum was 18.074 ms; display-list
processing peaked at 2.530 ms. The following resumed-gameplay window peaked at
20.24 ms. The old aggregate maxima did not identify the exact slow frame's
blocking stage, so they cannot prove the cause of the full 23.41 ms interval.

Source inspection did establish an avoidable dependency: changing only VSync
took the presentation thread mutex from the graphics producer. That mutex spans
presentation, driver waits, and pacing. A settings update could therefore stop
the producer while waiting for the previous presentation to complete.

## Change

- Runtime VSync edits now post a single atomic request. Repeated edits coalesce
  to the latest value. No settings-side wait for the presentation mutex is needed.
- The presentation thread consumes the request under its existing mutex before
  checking resize or acquiring an image. Vulkan's existing safe rebuild path
  remains responsible for mode changes. Fullscreen changes retain their lock.
- Every consumed request reaches the setter. Vulkan's getter describes the
  currently created chain, so comparing against it could incorrectly discard
  a request that cancels a previously deferred mode change.
- The software deadline resets on an applied request. Presentation timestamps
  and recorded intervals remain intact; delays are not hidden from the metrics.
- Opt-in slow-interval messages pair each recorded interval with its own
  present ID, authored writer, watermark, scene, policy and component timings.
  Separate budgets allow up to 32 menu and 32 race messages per process. These
  are threshold samples above 1.2 times the target period, not a full frame trace
  or physical display timing. `ready-us` includes all work since the previous
  Present call started, including that call; it is not pure geometry processing.

No source-rate assumption, geometry quality, guest clock, game rule, or existing
normal D3D12 pacing policy changed. The candidate retains candidate 01's Vulkan
buffer-count, presentation-ID lifetime and redundant-wait corrections.

## Verification and remaining limits

Offline checks include atomic off/on request delivery, coalescing, reset,
consumption, and two bounded concurrent 100,000-request sequences preserving the
final selection. Source contracts check the frontend posts outside the fullscreen
lock and the presenter consumes before the resize check. Frame pacing enabled and
disabled, authored cadence, metadata, pressure guard, Vulkan swapchain and
diagnostic checks all pass. Full Windows and Linux builds pass. Pinned RT64,
Plume and RecompFrontend dependency patches reconstruct cleanly; helper overrides
and hashes are exported. Existing unrelated Linux warnings remain unchanged.

This removes a verified source of possible settings-transition delay. It does
not establish that the full 23.41 ms sample is eliminated or that every future
frame meets 16.67 ms. The completed normal-play run is documented below and in
its session review. The private launcher explicitly forces Vulkan; the user's
normal-play capture superseded the original VSync-toggle test plan. Any further
launch requires fresh **ready**.

Vulkan buffering can help avoid a double-buffered FIFO stall after missing a
refresh. It cannot reduce arbitrary geometry cost or guarantee a flat frame time.
Primary reference: [Khronos swapchain buffering guidance](https://docs.vulkan.org/samples/latest/samples/performance/swapchain_images/README.html).
# Completed VSync session, September 21

User completed a 17-minute 25-second Vulkan run and reported mostly smooth with
occasional hitches. Exit code 0 and normal shutdown marker confirmed. VSync was
enabled before main races and retained, target 60 FPS on detected 120 Hz, draw
distance 50%. No 30 Hz authored-cadence classifications. Race/pause aggregation
averaged 16.674 ms over 55,827 reported intervals. Of 11 individually captured
race-tagged intervals over 20 ms, five were warmup/entry, five guest pause or
pause/resume, and one was a 20.119 ms moving-race interval shortly after entry.
Race sample budget did not exhaust; menu budget did. Physical scanout and driver
VRR state were not measured. Previous Present-path delays include queue mutex
and Vulkan API work, not isolated GPU time. No further safe patch is established
by these timings alone; no zero-hitch or comparative-gain claim is supported.

Full evidence: `analysis/frame-pacing-02-20260920/RoadRash64-Frame-Pacing-02-Win64/test-logs/20260921-183828-c01d13c3/REVIEW.md`
and `timing-review.json`; reproducible parser in the candidate analysis folder.
Game is closed. Publication unchanged. Further launches require fresh ready.
