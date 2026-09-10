# Road Rash 64 Recompiled 1.2

- Added local multiplayer AI-count and bike-level options, plus Custom Cop Mode with cop selection, bust scoring and manual siren/shout controls.
- Improved geometry processing, launcher startup and controller connection handling; fixed floating weapons and per-player eject controls.
- Fixed ultrawide sizing and FPS selection. Traffic now appears farther ahead with Draw Distance, reducing approaching-car pop-in.
- Cleaned up obsolete diagnostics, organized source code and added contributor notes. Dependency patches and locked hashes were rechecked.
- Added a separate **experimental native Linux AppImage ZIP**, based on [CalenCyr's Linux contribution](https://github.com/linkssy2/RoadRash64Recompiled/pull/4), with bundled custom-music decoding.

**Downloads:** Windows users want `RoadRash64Recompiled-v1.2.0-Win64.zip`. Linux testers want `RoadRash64Recompiled-v1.2.0-Linux-Experimental.zip`; it includes the AppImage, source and redistribution materials. Linux requires x86-64, Vulkan and GLIBC 2.35+/GLIBCXX 3.4.30+. It passed build and offline checks but this integrated version has not been gameplay-tested on Linux. Online multiplayer remains early and very untested. High draw distances can still be demanding.

**Bring your own supported Road Rash 64 USA v1.0 ROM.** No ROM or soundtrack is supplied. The Windows download retains the optional RT64 conversion of **Crisaty (Cristian A.T.)'s Road Rash 64 Remastered Edition** texture pack; all credit for the original mod and artwork belongs to its creator. [Original mod](https://crisaty.tumblr.com/post/113116999840/road-rash-64-remastered-edition).

Thanks to the original game team, N64Recomp, N64ModernRuntime, RT64, RecompFrontend and all upstream contributors. See [full credits](https://github.com/linkssy2/RoadRash64Recompiled/blob/v1.2.0/CREDITS.md). Development used AI assistance, including Codex and Claude-assisted Linux contribution work.
