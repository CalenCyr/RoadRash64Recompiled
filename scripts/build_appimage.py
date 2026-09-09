"""Package a built Linux binary into a portable AppImage.

Run this after a normal native/build (see BUILDING.md / the Linux build
steps) has produced native/build/bin/RoadRash64Recompiled. Produces
native/build/RoadRash64Recompiled-x86_64.AppImage.

This does its own dependency bundling pass rather than trusting
linuxdeploy's bundled patchelf for the final library copies: on newer
toolchains (glibc/binutils that emit DT_RELR compressed relative
relocations, e.g. Fedora 40+), linuxdeploy's older internal patchelf
corrupts those relocations when it rewrites RPATH, producing shared
libraries that segfault during their own ELF constructors at load time.
linuxdeploy is still used for what it does well: discovering the full
dependency closure via ldd and applying its maintained exclude-list (so
things like glibc, libstdc++, and Mesa/GL stay off the AppImage and are
loaded from the host, as is standard AppImage practice). After it runs,
every bundled library is replaced with a pristine, unmodified copy from
the host, and a small custom AppRun sets LD_LIBRARY_PATH instead of
relying on per-library RPATH patches - sidestepping the corruption
without giving up linuxdeploy's dependency discovery.

Deliberately does NOT add extra exclusions beyond linuxdeploy's own
exclude-list (earlier revisions of this script excluded libsystemd/
libselinux/libmount/libblkid too, as a blanket "these segfaulted once"
precaution taken before the pristine-copy fix above existed). Once every
library is copied pristine, that crash risk is gone for all of them
equally, and excluding a library instead means silently depending on the
host having it - which broke the AppImage on SteamOS (Steam Deck), which
has no libselinux at all. Bundle everything linuxdeploy finds; don't
assume any of it is safe to leave off the host's word alone.
"""
import os
import shutil
import stat
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
NATIVE_BUILD = ROOT / "native" / "build"
BINARY = NATIVE_BUILD / "bin" / "RoadRash64Recompiled"
ASSETS = NATIVE_BUILD / "bin" / "assets"
ICON_SOURCE = ROOT / "native" / "assets" / "RoadRashIcon.png"
APPDIR = NATIVE_BUILD / "AppDir"
TOOLS_DIR = NATIVE_BUILD / "appimage-tools"
OUTPUT = NATIVE_BUILD / "RoadRash64Recompiled-x86_64.AppImage"

LINUXDEPLOY_URL = "https://github.com/linuxdeploy/linuxdeploy/releases/download/1-alpha-20251107-1/linuxdeploy-x86_64.AppImage"
APPIMAGETOOL_URL = "https://github.com/AppImage/appimagetool/releases/download/continuous/appimagetool-x86_64.AppImage"

SEARCH_DIRS = ["/lib64", "/usr/lib64", "/lib", "/usr/lib", "/lib/x86_64-linux-gnu", "/usr/lib/x86_64-linux-gnu"]

APPRUN = """#!/bin/sh
HERE="$(dirname "$(readlink -f "${0}")")"
export LD_LIBRARY_PATH="${HERE}/usr/lib${LD_LIBRARY_PATH:+:${LD_LIBRARY_PATH}}"
cd "${HERE}/usr/bin"
exec ./RoadRash64Recompiled "$@"
"""

DESKTOP_FILE = """[Desktop Entry]
Type=Application
Name=Road Rash 64 Recompiled
Comment=Native PC port of Road Rash 64, built from the recompiled source
Exec=RoadRash64Recompiled
Icon=roadrash64recompiled
Categories=Game;
Terminal=false
"""


def download(url: str, dest: Path) -> None:
    if dest.exists():
        return
    dest.parent.mkdir(parents=True, exist_ok=True)
    print(f"Downloading {dest.name}...")
    subprocess.run(["curl", "-sL", "-o", str(dest), url], check=True)
    dest.chmod(dest.stat().st_mode | stat.S_IEXEC)


def find_pristine(basename: str) -> Path | None:
    for d in SEARCH_DIRS:
        candidate = Path(d) / basename
        if candidate.exists():
            return candidate.resolve()
    return None


def main() -> None:
    if not BINARY.exists():
        sys.exit(f"Build the project first: {BINARY} does not exist.")

    linuxdeploy = TOOLS_DIR / "linuxdeploy"
    appimagetool = TOOLS_DIR / "appimagetool"
    download(LINUXDEPLOY_URL, linuxdeploy)
    download(APPIMAGETOOL_URL, appimagetool)

    if APPDIR.exists():
        shutil.rmtree(APPDIR)
    (APPDIR / "usr" / "bin").mkdir(parents=True)
    shutil.copy2(BINARY, APPDIR / "usr" / "bin" / BINARY.name)
    shutil.copytree(ASSETS, APPDIR / "usr" / "bin" / "assets")

    icon_path = APPDIR / "roadrash64recompiled.png"
    subprocess.run(["convert", str(ICON_SOURCE), "-resize", "512x512", str(icon_path)], check=True)
    desktop_path = APPDIR / "roadrash64recompiled.desktop"
    desktop_path.write_text(DESKTOP_FILE)

    print("Running linuxdeploy for dependency discovery...")
    env = dict(os.environ, NO_STRIP="1")
    cmd = [
        str(linuxdeploy),
        "--appdir", str(APPDIR),
        "--executable", str(APPDIR / "usr" / "bin" / BINARY.name),
        "--desktop-file", str(desktop_path),
        "--icon-file", str(icon_path),
    ]
    subprocess.run(cmd, check=True, env=env)

    print("Replacing patchelf-modified libraries with pristine copies...")
    lib_dir = APPDIR / "usr" / "lib"
    missing = []
    for so_file in sorted(lib_dir.glob("*.so*")):
        pristine = find_pristine(so_file.name)
        if pristine is None:
            missing.append(so_file.name)
            continue
        shutil.copy2(pristine, so_file)
    if missing:
        sys.exit("Could not find a pristine source for: " + ", ".join(missing))

    # Also restore the pristine executable: avoid whatever patchelf did to it too.
    shutil.copy2(BINARY, APPDIR / "usr" / "bin" / BINARY.name)

    apprun_path = APPDIR / "AppRun"
    apprun_path.unlink(missing_ok=True)
    apprun_path.write_text(APPRUN)
    apprun_path.chmod(apprun_path.stat().st_mode | stat.S_IEXEC)

    print("Packaging AppImage...")
    OUTPUT.unlink(missing_ok=True)
    subprocess.run([str(appimagetool), str(APPDIR), str(OUTPUT)], check=True)

    print(f"\nDone: {OUTPUT} ({OUTPUT.stat().st_size / (1024 * 1024):.1f} MiB)")
    print("Test it before releasing: ./native/build/RoadRash64Recompiled-x86_64.AppImage")


if __name__ == "__main__":
    main()
