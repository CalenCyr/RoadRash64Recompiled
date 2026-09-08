# Frontend audit result

Local cleanup only; not published. Audited all 69 original hunks in 18 files.
The patch decreased from **1,517 lines / 77,988 bytes** to
**1,140 lines / 55,579 bytes**. Line counts include diff context.

Removed old UI/renderer startup progress prints, their logging-only temporary
variables, routine focus-change logging and redundant trailing whitespace.
Retained actual exception/SDL failure diagnostics and rethrow behavior.
Corrected framerate help text so it no longer claims VSync is always enabled.

Retained active rumble coalescing/expiry, hotplug and focus handling, prompt-device
tracking, action/profile bindings, popup dismissal guards, post-input update
ordering, background artwork, mod format guidance, stylesheet read validation,
graphics conversion and presentation synchronization. These are used by the
current integration; safe removability was not established. The explicit
UI-hook troubleshooting switch remains supported. Default-title styling remains
as a harmless fallback, even though the game replaces its title artwork.

The [editing guide](FRONTEND_EDITING_GUIDE.md) maps the remaining changes and their
constraints. The [original inventory](FRONTEND_AUDIT_BEFORE.md) records classifications
before edits. No broad input, rendering or multiplayer behavior was changed.

Clean application to the pinned frontend revision reconstructed all 18 reviewed
files exactly after line-ending normalization. Build/test evidence is recorded
separately under `analysis/frontend-cleanup` in the development workspace.
No new runtime test was performed for this frontend cleanup. The earlier user
acceptance applies to the preceding RT64 cleanup build, not this candidate.

Removing startup logging is not a measured in-race performance improvement.
Per-frame focus tracking, rumble locks and update callbacks remain active and
would need profiling before optimization. The local-player setup is the next
separate implementation: expose existing controller assignment/profiles, then
add local in-game names without conflating them with online identity or binding
profile names. Preserve the original local multiplayer game behavior.
