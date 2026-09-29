"""Carry local portable player data into a freshly packaged private candidate.

Run AFTER creating and auditing the clean download ZIP. This deliberately never
copies program files, course packs, logs or crash dumps. Existing destination
files are retained unless explicitly replacing verified clean packaged defaults.
ROMs and installed mods remain local to this machine;
do not create a distributable archive from the resulting personalized folder.
"""
from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path
import shutil
import tempfile


# Keep this list explicit: a new runtime state family needs a conscious addition,
# rather than accidentally collecting an executable, diagnostic or asset pack.
STATE_FILES = (
    "general.json", "graphics.json", "controls.json", "sound.json",
    "mods.json", "race_packs.json", "cheats.json", "character-preferences.cfg",
    "local-race-options.cfg", "thrash-race-options.cfg", "achievements.txt",
    "rr64.n64.us.1.0.z64",
)
STATE_DIRECTORIES = ("mods", "mod_config", "saves", "music")


def digest(path: Path) -> str:
    with path.open("rb") as stream:
        return hashlib.file_digest(stream, "sha256").hexdigest()


def reject_link(path: Path) -> None:
    # Refuse Windows junctions as well as symbolic links. The copy must stay
    # within the two explicitly selected candidate folders.
    if path.is_symlink() or (hasattr(path, "is_junction") and path.is_junction()):
        raise ValueError(f"Linked paths are not portable test data: {path}")


def reject_link_chain(path: Path) -> None:
    # Check before resolve(), which would hide a linked candidate root or parent.
    for part in (path.absolute(), *path.absolute().parents):
        reject_link(part)


def carry_forward(previous: Path, destination: Path, manifest: Path,
                  replace_packaged_defaults: bool = False) -> dict:
    reject_link_chain(previous)
    reject_link_chain(destination)
    previous = previous.resolve(strict=True)
    destination = destination.resolve(strict=True)
    if previous == destination or previous in destination.parents or destination in previous.parents:
        raise ValueError("Candidate folders must be separate, non-nested directories.")
    for folder in (previous, destination):
        if not (folder / "portable.txt").is_file():
            raise ValueError(f"Not a portable candidate folder: {folder}")
    package = json.loads(manifest.read_text(encoding="utf-8-sig"))
    if Path(package["directory"]).resolve() != destination:
        raise ValueError("Manifest belongs to a different destination.")
    archive = Path(package["archive"])
    if digest(archive) != package["archiveSHA256"]:
        raise ValueError("Clean download ZIP is missing or has changed.")
    for name, expected in package["files"].items():
        path = destination / name
        if not path.resolve().is_relative_to(destination):
            raise ValueError("Package manifest contains an escaping path.")
        if digest(path) != expected:
            raise ValueError(f"Destination package changed before state staging: {name}")

    state_names = (*STATE_FILES, *(name + ".bak" for name in STATE_FILES if name.endswith(".json")))
    paths = []
    for name in state_names:
        path = previous / name
        reject_link(path)
        if path.exists():
            if not path.is_file():
                raise ValueError(f"Expected a portable state file: {path}")
            paths.append(path)
    for name in STATE_DIRECTORIES:
        folder = previous / name
        reject_link(folder)
        if not folder.exists():
            continue
        if not folder.is_dir():
            raise ValueError(f"Expected a portable state directory: {folder}")
        for path in folder.rglob("*"):
            reject_link(path)
            if path.is_file():
                paths.append(path)

    # Validate the entire plan before copying; links or collisions cannot yield
    # a partially imported profile. Existing packaged README files are retained.
    plan, retained = [], []
    for origin in sorted(paths):
        reject_link(origin)
        name = origin.relative_to(previous).as_posix()
        target = destination / name
        if not target.resolve().is_relative_to(destination):
            raise ValueError(f"Destination escapes candidate folder: {name}")
        for parent in (target, *target.parents):
            if parent == destination:
                break
            reject_link(parent)
            if parent != target and parent.exists() and not parent.is_dir():
                raise ValueError(f"Destination parent is not a directory: {parent}")
        if target.exists():
            if not target.is_file():
                raise ValueError(f"Destination state path is not a file: {target}")
            if replace_packaged_defaults and name in state_names and name in package["files"]:
                # Only untouched root state shipped in this exact clean ZIP is
                # replaceable; installed mods/saves and user overrides are not.
                if digest(target) != package["files"][name]:
                    raise ValueError(f"Packaged default changed before replacement: {name}")
                plan.append((origin, target, name, digest(origin), True))
            else:
                retained.append(name)
        else:
            plan.append((origin, target, name, digest(origin), False))

    copied, replaced = {}, {}
    for origin, target, name, expected, replace in plan:
        target.parent.mkdir(parents=True, exist_ok=True)
        if replace:
            staging = None
            try:
                with origin.open("rb") as src, tempfile.NamedTemporaryFile(
                        mode="wb", dir=target.parent, prefix=".rr64-state-", delete=False) as dst:
                    staging = Path(dst.name)
                    shutil.copyfileobj(src, dst)
                if digest(staging) != expected:
                    raise RuntimeError(f"Replacement verification failed: {name}")
                if digest(target) != package["files"][name]:
                    raise RuntimeError(f"Packaged default changed during staging: {name}")
                staging.replace(target)
                replaced[name] = expected
            finally:
                if staging is not None and staging.exists():
                    staging.unlink()
        else:
            with origin.open("rb") as src, target.open("xb") as dst:
                shutil.copyfileobj(src, dst)
        if digest(target) != expected:
            raise RuntimeError(f"Copy verification failed: {name}")
        copied[name] = expected
    if digest(archive) != package["archiveSHA256"]:
        raise RuntimeError("Download ZIP changed during local state staging.")
    return dict(previous=str(previous), destination=str(destination), copied=copied,
                retained=retained, replacedPackagedDefaults=replaced,
                archiveUnchanged=True, localOnly=True,
                gameLaunched=False)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("previous", type=Path)
    parser.add_argument("destination", type=Path)
    parser.add_argument("manifest", type=Path)
    parser.add_argument("report", type=Path, help="Write outside the candidate folder.")
    parser.add_argument("--replace-packaged-defaults", action="store_true",
                        help="Replace only allowlisted root state still matching the clean package manifest.")
    args = parser.parse_args()
    if any(args.report.resolve().is_relative_to(folder.resolve())
           for folder in (args.previous, args.destination)):
        parser.error("The local staging report must be outside both candidate folders.")
    result = carry_forward(args.previous, args.destination, args.manifest,
                           replace_packaged_defaults=args.replace_packaged_defaults)
    with args.report.open("x", encoding="utf-8") as stream:
        json.dump(result, stream, indent=2)
    print(f"Preserved {len(result['copied'])} local player files; clean ZIP unchanged.")
