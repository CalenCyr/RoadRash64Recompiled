# Road Rash 64 Recompiled 1.4.1 Hotfix

Changes since **1.4.0**.

- **Player names:** solo Thrash and new Big Game profiles use the Player 1 name
  from Controls. Existing saves and manually entered names are preserved.
- **Keyboard multiplayer:** entering Multiplayer without assigned controllers
  keeps keyboard control available, including solo races against AI.
- **Ultrawide framing:** removed a 16:9 cap from the expanded output path so
  ultrawide displays can use their available width. Fixed 16:9 remains available.
- **Imported-course standings:** corrected AI progress after crash recovery on
  MK64 courses, which could incorrectly leave a leading player near last place.
- **Completed Big Game saves:** completing Level 5 now returns to Save Game after
  the ending. Save normally; after restarting, open/load that completed save to
  restore the special bike tier earned for its difficulty in Thrash/multiplayer.
- **Offline cheats:** added **Prevent Opponents from Moving**. Human players stay
  controllable; cheats remain disabled online and suppress achievement unlocks.
- **Release cleanup:** removed the temporary bike-selection probe and retained
  focused helpers and checks. Existing launcher artwork, app icons and all 52
  achievement images are unchanged.

## Downloads and updating

Windows x64, experimental Linux x86-64 and corresponding source are provided.
Download **RoadRash64Recompiled-Optional-Texture-Mod.zip** separately for the
optional Remastered Edition texture mod by **Crisaty (Cristian A.T.)**. Extract
its RTZ and install through **Settings > Mods**. Replace an older installed copy
instead of creating duplicates. This is the same texture conversion as before.

The main platform and source downloads exclude ROMs, extracted game textures,
course previews, maps and audio; the retained achievement images are the artwork
exception, alongside the unchanged custom launcher artwork and app icons.
Standard UI resources and library fonts/icons remain.
Supply your own supported **Road Rash 64 USA v1.0 ROM**. Importing MK64 courses
also requires your own supported **Mario Kart 64 USA ROM**; conversion happens
locally through Mods. Credits and third-party notices accompany every package.

Existing 1.4.0/RC4-and-later imported packs remain compatible. Converter:
`1.0.1-c39`. RC3-or-earlier packs still need reimporting.

**Online remains experimental. All peers must use 1.4.1 (protocol 58)** and the
same enabled imported pack. This update does not establish stutter-free Internet
play. Linux hardware coverage and imported-course terrain coverage remain limited.

The owner confirmed both solo and multiplayer Insanity bike selection works;
no speculative selection change was made. Completion-save behavior and restored
rewards passed generated-native offline tests; a full live final-campaign save
round trip has not been verified. The final package was not launched again.

**Draw Distance can load the entire Road Rash map. Start around 50–60%.** Distant
geometry can reduce performance, especially with multiple local players.
