# Native build map

`native/CMakeLists.txt` is the build-order map. Modules are included in the same
directory scope, so source paths and target names retain their existing meaning.

| Module | Owns |
| --- | --- |
| `cmake/ProjectOptions.cmake` | Language standards, platform flags, output and dependency paths. |
| `cmake/Dependencies.cmake` | Importer helpers, required-file checks, renderer/runtime/frontend libraries, generated guest library, SDL, Opus and media dependencies. |
| `cmake/ReplayWorkers.cmake` | Generated replay frame/resource workers and their source dependencies. |
| `cmake/Application.cmake` | Game sources, include order, linker settings, platform media libraries, asset copies and Windows DirectStart. |
| `cmake/OfflineTests.cmake` | Ordered inclusion of the offline fixture catalog. |
| `cmake/tests/*.cmake` | Network/replay, timing/input, rendering and gameplay/service test groups. |

Keep platform audio libraries and DirectStart in `Application.cmake`; a player
build must not depend on enabling tests. Keep test include order: some later
fixtures deliberately reuse earlier targets' include paths.

## Configure and build

Use the project's existing toolchain/dependency setup. The following options
apply to an already prepared native tree; they do not replace the ROM import or
guest-code generation steps.

```sh
cmake -S native -B native/build -DRR64_BUILD_TESTS=OFF
cmake --build native/build --config Release --target RoadRash64Recompiled
```

`RR64_BUILD_TESTS` defaults to `ON` for existing verification-script compatibility.
Test targets remain `EXCLUDE_FROM_ALL` in either the old or reorganized catalog;
turning the option off additionally skips configuring the offline fixtures.
Re-enable it when building named checks:

```sh
cmake -S native -B native/build -DRR64_BUILD_TESTS=ON
cmake --build native/build --config Release --target RR64Mk64ItemAudioSmoke RR64SchedulerQueueSmoke
```

Fixtures have different arguments and some use private ROM input. Read the
relevant fixture before running it; do not run every executable with arbitrary
defaults. The legacy SI queue target is an intentional failing negative control.
None of these commands authorizes a live game launch.

## Ownership boundaries

- Edit maintained native code and hook configuration; regenerate guest output
  instead of editing `build/RecompiledFuncs` by hand.
- A dependency fix must be exported in `dependency-patches/` or the appropriate
  `dependency-overrides/` entry and match `dependencies.lock.json`.
- Keep active dependency copies byte-identical to their exported overrides;
  line-ending-only differences still break reproducible file identities.
- Keep optional importer support and diagnostic fixtures. Being absent from a
  default player build does not make a source file unused.

The September 2026 module split was checked through CMake's generated target
model with tests both enabled and disabled: game sources, compile definitions,
include/link order and production dependency edges matched the prior catalog.
The refactor does not change dependency versions, network protocol, game rules
or the saved-settings schema.
