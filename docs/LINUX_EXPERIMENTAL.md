# Road Rash 64 Recompiled 1.3 — Experimental Linux download

This package integrates CalenCyr's native Linux contribution with the RoadRash64Recompiled 1.3 source. It is a separate Linux build, not the
Windows executable running through Wine. No ROM is included; select your own
supported Road Rash 64 USA v1.0 ROM at startup.

## Running

Extract the ZIP into a writable folder. If your archive tool did not preserve
executable permissions, run `chmod +x RoadRash64Recompiled-x86_64.AppImage`.
Start the AppImage normally. The package targets x86-64 Linux with a working
Vulkan driver, GLIBC 2.35+ and GLIBCXX 3.4.30+ (GCC 12-era libstdc++).
FUSE support may be needed; where unavailable, try
`APPIMAGE_EXTRACT_AND_RUN=1 ./RoadRash64Recompiled-x86_64.AppImage`.

Place optional custom tracks in a `music` folder beside the AppImage. The mounted
AppImage itself is read-only. FFmpeg libraries are bundled; installing a system
FFmpeg player or codec package is not required to use this download.

This integration is intended for community testing. The contributor reported
Fedora and Steam Deck testing of their earlier fork; that does not establish
gameplay compatibility of this newer binary. Report your distribution, GPU,
driver, desktop/session type, player count and steps to reproduce any problem.
Do not attach ROMs or personal configuration/saves to a public issue.

## Building and validating

The initial packaging environment uses Ubuntu 22.04, GCC/G++ 12 and a locally built
SDL 2.30.3. SDL 2.26 or later is required; Ubuntu 22.04's original SDL is too old.
Use GCC 12 or newer with its matching C++20 standard library. Clang 14 with the
older/mixed libraries failed during validation and is not the documented recipe.

GCC 12 is the initial Linux packaging baseline, not a requirement to use only
that version, and it is not the Windows compiler. The current Windows candidate
build uses Visual Studio's ClangCL 19.1.5. GCC 16.2 is available as of August 2026,
but this project has not yet validated a GCC 16 package. Compiler age alone does
not establish the cause of a crash, network desynchronization or a performance
regression. Evaluate a newer compiler in a separate build directory, with a
matching standard library, then compare tests and representative race timings.
Recheck the AppImage's required GLIBC/GLIBCXX versions before changing the
distributed compiler baseline; preserve the existing package for comparison.
Upstream release status: https://gcc.gnu.org/gcc-16/.

Linux development packages include build-essential, g++-12, cmake, ninja-build,
pkg-config, git, Python 3.10+, libsdl2-dev (2.26+), libgtk-3-dev, libssl-dev,
libvulkan-dev and zlib1g-dev. AppImage packaging also uses curl, ImageMagick and
libfuse2. See BUILDING.md for ROM staging and code generation. For the native step:

```sh
cmake -S native -B native/build -G Ninja -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_C_COMPILER=gcc-12 -DCMAKE_CXX_COMPILER=g++-12
cmake --build native/build --target RoadRash64Recompiled --parallel 6
python3 scripts/build_appimage.py
```

`RR64MusicSmoke` uses generated Linux fixtures instead of the Windows Media
Foundation encoder. Developers can install an FFmpeg CLI solely to create fixtures
and run `scripts/test_linux_audio.sh`. This is not a player/runtime dependency.

The archive's redistribution materials include bundled-library identities, notices
and corresponding sources. FFmpeg is built as minimal shared libraries; replacing
them is possible by extracting the AppImage and rebuilding compatible libraries.
The exact FFmpeg configuration is in native/CMakeLists.txt. Do not publish a
repackaged binary without updating its source and license materials.

Online and proximity voice are experimental. Maximum Draw Distance can load all race areas on the map; start around 50-60%, particularly for split screen. This 1.3 integration has not been gameplay-tested on Linux.
