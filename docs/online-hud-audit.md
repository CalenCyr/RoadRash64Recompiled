# Online HUD audit — 2026-09-11

Update: an online-only scoped override now selects the original one-player HUD
layout and local rider for this routine, restoring the original globals at its
epilogue. Synthetic restoration checks pass; visual correctness remains unverified.
The findings below describe the release baseline before this candidate correction.

Source audit against the restored v1.2.0 working copy; no live launch performed.

The online full-screen change is limited to geometry/camera presentation.
`make_viewport_render_plan` selects layout zero while retaining the network
player's guest index. The four render hooks in `config/roadrash64.us.toml`
target `func_8006A638`; they do not supply a corresponding HUD layout override.
For two through four online players, `requested_racer_count` intentionally
preserves multiple simulated controller slots. It must not be forced to one
merely to fix presentation.

Consequently full-screen geometry does not establish full-screen HUD behavior.
The user's report is consistent with this incomplete separation. The retained
generated guest reference also shows the original HUD routine (`func_80030220`)
branching on the human count at 0x80033240. That reference is read-only historical
evidence, not source to import into this clean working copy.

There is a second explicit problem to address when enabling online Custom Cop:
`rr64_custom_cop_ui.cpp` derives width/height and offsets from total humans and
iterates all human slots. It needs local-peer filtering and a full-screen region
online. In released 1.2, Custom Cop itself is gated off online, so this alone
does not explain the reported ordinary race HUD.

Required correction: separate displayed HUD region from simulated player count,
select the local peer's rider data, and retain original local split-screen rules.
Audit ordinary HUD, messages and cop tally together. Do not repeat the previous
global HUD/layout or sky changes. Live visual correctness remains unverified.
