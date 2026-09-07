# Building Release 1.0 yourself

The game executable is built ahead of time. Players of the binary release only select a ROM; these instructions are for compiling from source.

## Prerequisites

- 64-bit Windows and **Visual Studio 2022**, with Desktop development with C++, the x64 Windows SDK, and the **C++ Clang tools for Windows** component (ClangCL toolset).
- Git, CMake 3.20 or newer, and Python 3.11 or newer available on PATH.
- Internet access to obtain pinned open-source dependencies, SDL2 and Opus.
- Your own supported Road Rash 64 USA v1.0 ROM. No ROM download is performed by these scripts.

Open a Visual Studio Developer PowerShell with the x64 environment initialized. Work from the source repository root. Commands below are PowerShell commands; adjust only your ROM path and build parallelism as needed.

## Obtain the source and dependencies

```powershell
git clone https://github.com/linkssy2/RoadRash64Recompiled.git
cd RoadRash64Recompiled
git checkout v1.0.0
python scripts/setup_dependencies.py
```

Until the repository/tag is published, extract the prepared Source ZIP and run the same setup command from its root. `dependencies.lock.json` records the exact upstream commits, including nested dependencies. The script applies the checked-in `dependency-patches` and `dependency-overrides`; cloning only current upstream branches does not reproduce this release. Existing conflicting folders/files cause an error rather than being erased. Use a fresh source folder for a clean build.

## Stage your ROM and build the translators

```powershell
python tools/stage_rom.py "C:\YourDumps\Road Rash 64 (USA).z64"
cmake -S tools/N64Recomp -B build/toolchain -G "Visual Studio 17 2022" -A x64 -T ClangCL
cmake --build build/toolchain --config Release --target N64RecompCLI RSPRecomp --parallel 2
```

The staging tool normalizes supported byte order and checks the ROM hash. It writes the working ROM to ignored `build/roadrash64.us.z64`. It does not modify the original input.

## Generate CPU and audio code locally

```powershell
$cpu = Get-ChildItem build/toolchain -Recurse -Filter N64Recomp.exe | Select-Object -First 1
$rsp = Get-ChildItem build/toolchain -Recurse -Filter RSPRecomp.exe | Select-Object -First 1
if (!$cpu -or !$rsp) { throw "Translator build did not produce both tools" }
& $cpu.FullName config/roadrash64.us.toml
if ($LASTEXITCODE -ne 0) { throw "CPU recompilation failed" }
& $rsp.FullName config/roadrash64.us.audio_rsp.toml
if ($LASTEXITCODE -ne 0) { throw "Audio recompilation failed" }
```

Generated code belongs under ignored `build/RecompiledFuncs`. It is excluded from the public source archive and must not be committed. The checked-in symbol map and hooks are the release inputs; do not replace them with an older generated symbol map.

## Compile the native application

```powershell
cmake -S native -B native/build -G "Visual Studio 17 2022" -A x64 -T ClangCL
cmake --build native/build --config Release --target RoadRash64Recompiled RoadRash64DirectStart --parallel 2
```

The executable, frontend assets and required graphics/SDL DLLs are placed under `native/build/bin`. Run `RoadRash64Recompiled.exe` there and select your supported ROM. Keep the DLLs and assets with the EXE. `RoadRash64DirectStart.exe` uses the same configured ROM and settings.

Build time and memory use depend on the machine. Start with two parallel jobs if compiler memory is limited. Do not copy a game's installed AppData/config folder into a public package; it can contain the locally stored ROM and saves.

## Validation and limitations

The release preparation used the existing Windows toolchain and regression suite. The four dependency patches were applied to clean archives of their pinned upstream revisions and compared against the working sources (ignoring line-ending differences). The release owner additionally reports a successful source build by another user on a different computer; that tester’s build log and archive checksum were not supplied. A successful compilation or offline input/rendering test does not prove the absence of runtime bugs; multiplayer and the latest menu changes still require broader live testing.

For an initial ROM-free check after configuring:

```powershell
cmake --build native/build --config Release --target RR64PopupInputSmoke RR64ActionBindingsSmoke RR64AchievementAudioSmoke --parallel 2
& native/build/bin/RR64PopupInputSmoke.exe
& native/build/bin/RR64ActionBindingsSmoke.exe
& native/build/bin/RR64AchievementAudioSmoke.exe
```

Some other developer tests require locally generated fixtures or the local ROM and are not part of the player release. Routine diagnostic reports can be explicitly enabled in a developer session with `$env:RR64_DIAGNOSTICS='1'`; leave this unset for normal play. Never publish ROM-containing fixtures or personal logs.

## Versioning and existing mods

The application version is **1.0.0**, package version **v1.0.0**, public designation **Release 1.0**. Internal development builds previously used 1.0.6. The mod loader retains an explicit compatibility floor of 1.0.6 because the interfaces were not downgraded when the public version was reset. This allows existing working mods to remain installed; it does not alter their files or pretend that the application version is 1.0.6. Mods requesting newer unsupported interfaces still fail their version check.
