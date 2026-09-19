# Optional paired sync capture

Set `RR64_SYNC_LOG` to a new CSV file path before launching each test process.
Parent directories must already exist. Use different filenames on host/client;
existing files are never overwritten. Unset it for normal release play.
The test package will need a launcher that selects these paths automatically.

Submit both CSVs from the same session plus the candidate identity and a short
description of when the mismatch occurred. No usernames, IP addresses, ROM data
or authentication tokens are written to this CSV.

Capture records local race/frame, monotonic timestamp, host/slot identity, guest
setup fingerprint, RNG, controls, local simulated positions, and received sender
ticks/positions. It samples initial pre-update state and post-update frames.
Controls are the latest available network controls, not proof of exactly which
input a guest instruction consumed. Capturing adds guest reads and network locks;
performance overhead has not yet been measured.

A bounded single-producer queue moves file I/O to a background thread. It flushes
every approximately 100ms and caps output at 250,000 rider rows. Queue drops,
write/limit failures and missing end markers invalidate complete-capture claims.
Crashes can lose queued/unflushed tail records. Normal and quick exit drain the
writer. File-open failures are reported to stderr and disable capture.

Run `python scripts/compare_sync_logs.py host.csv client.csv` to check matching
sender-tick payloads and flag initial-state differences. Equal local frame numbers
are NOT shared simulation frames. Race ordinals must refer to corresponding races;
do not combine reconnects or unrelated sessions. Matching payloads prove neither
matching simulation nor synchronized race starts. Initial differences need timing
interpretation; missing origins do not prove packet loss.

Offline status: writer/integration compile, overflow accounting and no-overwrite
checks pass. Synthetic report tests cover matching/different/missing payloads and
incomplete capture. Full candidate build and live host/client acceptance pending.
