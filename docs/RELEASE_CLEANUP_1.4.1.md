# 1.4.1 release cleanup

The accepted temporary Insanity selection probe was removed from native hooks,
shared recorder declarations and its dedicated smoke assertions. Selection
behavior is unchanged. The optional shared race recorder remains useful.

Player naming, campaign completion and offline opponent freeze use small,
separately tested helpers. Keyboard routing remains in the existing menu owner;
imported-course progress extends the signed route helper to eligible AI instead
of replacing stock-track logic. No broad unrelated refactor was performed.

The RT64 patch was regenerated against its exact pinned commit after finding an
invalid trailing hunk. Four dependency patches and 33 overrides reconstruct the
live dependency changes. Locked override bytes are preserved across Git exports.

Public packages omit private logs, profiles, ROMs, converted courses and obsolete
candidate artifacts. Historical evidence is preserved locally. Existing launcher
artwork, icons and all 52 achievement images are unchanged by explicit request.
The optional Crisaty texture conversion is provided as a separate release asset.

Windows/Linux checks and artifact hashes are retained privately with the release
manifest. Offline tests do not establish every game mode or Internet-play case.
