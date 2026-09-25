# Candidate 03: small-hitch corrections

Private candidate built on Windows and Linux; no visual launch or publication.
Windows executable SHA256:
`c004e6a92eca8cf0e531e0f7e169a2b34566ad9f72c126fdc2e0e9c2d1de2008`.
Folder: `analysis/frame-pacing-03-20260921/RoadRash64-Frame-Pacing-03-Win64`.

## Why these changes

The previous 17-minute Vulkan/VSync run was mostly smooth by user report, with
occasional hitches. Recorded longer intervals were predominantly startup, paused
or transition work; one moving-race interval measured 20.119 ms before the timer.
This does not establish a single cause for every reported hitch.

Further source inspection and offline reproduction found two independent defects:

1. The frame limiter rebased its absolute schedule on every positive timer or
   submission error, even one nanosecond. Repeated tiny errors therefore
   accumulated. The correction preserves phase for errors up to 1% of a period,
   capped at 250 microseconds; larger delays still reset to avoid catch-up bursts.
   At 60 FPS, recovery can shorten one interval by at most 166.666 microseconds.
2. The hidden online lobby queued 16 text changes on every presentation even
   during offline races. The frontend only drains updates for visible contexts,
   causing an unbounded backlog and eventual replay when the lobby opens.
   Only the DOM refresh is now visibility-gated. Requests, session transitions,
   teardown and focus handling continue; opening refreshes current labels in
   that same update before visible contexts render.

Source simulation, race rules, native rendering fidelity, audio clocks, queue
ownership, full-refresh FIFO policy and D3D12 VSync behavior are preserved.
These fixes remove demonstrated timer drift and unnecessary memory/work growth;
they do not establish the cause of the individual 20.119 ms event or quantify
the live smoothness benefit. The run's overall memory growth includes changing
game resources and cannot all be attributed to the lobby.

## Evidence

- Full Windows/Linux production builds and nine offline invocations per platform
  passed. Frame-pacing tests include existing 24-hour modeled stress plus
  432,012 varying-jitter checks at 30–1,000 Hz, real stalls, rate changes and resets.
- A ten-minute synthetic 60 Hz case with recurring 1-microsecond submission
  overhead drifts 36.001 ms using the saved old helper; the corrected helper's
  phase error remains 1 microsecond. A genuinely slower source remains limited
  by that source. This is a scheduler proof, not a measured FPS gain.
- UI regression compiles the complete production `update_ui()` function with
  deferred-queue/network doubles. It passes 500,066 checks including 100,000
  hidden updates, all 14 names, same-frame show, hidden game-setup entry,
  reset/local start and disconnected focus. The exact saved old function fails
  the hidden-backlog assertion. Actual Rml rendering/live networking are separate.
- RT64, Plume and RecompFrontend patches were reconstructed from their pinned
  commits; matching helper overrides and hashes exported. The public source
  mirror and published downloads are unchanged.
- The private staged inventory has 169 verified files. It derives from the prior
  candidate's explicit inventory, excluding live logs and ROM files. A standalone
  diagnostic PresentMon console, license and documentation are added for this
  private run; it is not a public runtime dependency.

Details: `analysis/frame-pacing-03-20260921/PACER-PHASE-AUDIT.md`,
`hidden-online-menu-audit.md`, `pacer-phase-proof.jsonl`,
`old-ui-negative-control.json`, `windows-checks.json`, `linux-checks.json`,
`dependency-export.json`, and `candidate.json`.

## Next authorized run

The launcher defaults to Vulkan and records the exact game/recorder hashes.
After **ready**, use 60 FPS with VSync on, VRR off, the same resolution and 50%
draw distance. Play normally for approximately ten minutes, with a race finish
and pause/resume if convenient. Briefly opening Online after the race checks
fresh labels without another participant. Close normally.

PresentMon 2.5.1 console is pinned to the official release digest. It starts hidden
for only the new game's PID, without input-event tracking or installing a service.
It writes display-event CSV alongside internal logs, uses a unique ETW session,
stops after game exit and has a one-hour bound. Launching and checking CSV growth
still require the next user-ready gate; ETW permissions, Vulkan coverage and event
loss have not yet been validated live. Do not stop another trace or game as a
fallback. Capture failure leaves the game/internal logs usable and is recorded.

Paired internal spikes now include their absolute steady-clock nanoseconds;
Windows QPC frequency/launch anchors are saved for alignment. Display-event
timing can separate CPU pacing from Windows' displayed/dropped-frame behavior,
but is not a physical sensor measurement and may omit earliest startup.

Primary background: [Khronos explains separate CPU, GPU and display timing](https://www.khronos.org/blog/vk-ext-present-timing-the-journey-to-state-of-the-art-frame-pacing-in-vulkan).
The new Vulkan timing extension was researched but not integrated or made a
required driver feature. The existing [Vulkan queue synchronization requirements](https://docs.vulkan.org/refpages/latest/refpages/source/vkQueuePresentKHR.html)
remain intact. [PresentMon console documentation](https://github.com/GameTechDev/PresentMon/blob/v2.5.1/README-ConsoleApplication.md)
defines the captured display-event metrics and limitations.
