# One-batch unattended online verification

Run tools/run-online-verification.py with --session pointing to the current
private AI/demo session and --output pointing to a new report folder. It never
launches or controls a game, changes a router, or asks a user to drive. Start
one private demo through the established verification launcher first; an already
completed session can also be evaluated without replaying it manually.

The batch rebuilds/runs the 34 offline checks, rebuilds the production UDP test
harness, runs LAN/Internet/14-player/loss-outage profiles, and monitors the demo
capture until its sampling-end marker or the bounded wait expires. Each phase
writes progress to report.json and READ-RESULTS.md. Existing evidence is not
overwritten. Executable identity is checked against the session's hash.

Live coverage counts only matching completed comparisons. At least64 matches,
eight mounted, eight detached, eight remounted and eight traffic comparisons
are required. Any rejected execution, RNG difference or state difference fails
that section. Passing these minimums is sampled coverage, not an exhaustive
physics proof. Artificial packet tests measure transport, not rendered gameplay.

The report deliberately lists production integration blockers and untested
online gameplay. A green diagnostic batch does not switch on host authority or
claim the candidate is ready. Fix implementation blockers next; do not use missing
integration as a reason to ask for another identical demo capture.

Current full-race prerequisites still include the historical resource-event
journal and activation of the host-authoritative lifecycle/correction path.
Combat outcomes, finishes, host pause/disconnect, remote input/HUD and true
host/client smoothing need live connected coverage after that integration.

Reports contain text only. Never upload the separate private-replay-cases
folders, ROMs, saves or converted memory cases with these reports.
