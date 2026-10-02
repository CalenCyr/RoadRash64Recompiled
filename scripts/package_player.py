"""Create a tidy player ZIP from an explicitly verified clean package manifest.

The input manifest must contain archive, archiveSHA256 and the complete files
SHA256 map. It is the release packager's allowlist, never a live installation.
Runtime paths and user data stay unchanged. Supporting documents and licenses
move into docs/. Optional replacements require their own path/SHA256 manifest.
No game process is started and no historical package is changed.
"""
from __future__ import annotations

import argparse
import hashlib
import json
import posixpath
import re
import shutil
from pathlib import Path, PurePosixPath
from urllib.parse import unquote
import zipfile


DOCUMENTS = frozenset({
    "BUILD_INFO.json", "CONTROLLER_SUPPORT.md", "COPYRIGHT", "CREDITS.md",
    "LEGAL.md", "LICENSE", "RELEASE_NOTES.md", "SHA256SUMS.txt",
    "THIRD_PARTY_NOTICES.md",
})
REPLACEABLE = DOCUMENTS | {"README.md", "RoadRash64Recompiled.exe",
                         "RoadRash64DirectStart.exe",
                         "RoadRash64Recompiled-x86_64.AppImage"}
CHECKSUMS = "docs/SHA256SUMS.txt"
MARKDOWN_LINK = re.compile(r"(!?\[[^\]]*\]\()([^\s)]+)(\))")
PRIVATE_SUFFIXES = {".rom", ".z64", ".v64", ".n64", ".rtz", ".log", ".dmp", ".sav", ".mp4"}
PRIVATE_ROOTS = {"mods", "mod_config", "race-packs", "saves", "test-logs", "analysis"}
STATE_FILES = {"general.json", "graphics.json", "controls.json", "sound.json", "mods.json",
               "race_packs.json", "cheats.json", "character-preferences.cfg",
               "local-race-options.cfg", "thrash-race-options.cfg", "achievements.txt"}


def digest(path: Path) -> str:
    with path.open("rb") as stream:
        return hashlib.file_digest(stream, "sha256").hexdigest()


def read_json(path: Path) -> dict:
    return json.loads(path.read_text(encoding="utf-8-sig"))


def safe_name(name: str) -> str:
    parts = PurePosixPath(name).parts
    if (not parts or PurePosixPath(name).is_absolute() or
            any(part in {".", ".."} or ":" in part or "\\" in part for part in parts) or
            PurePosixPath(name).as_posix() != name):
        raise ValueError(f"Not a relative package path: {name}")
    return name


def reject_links(path: Path) -> None:
    for part in (path.absolute(), *path.absolute().parents):
        if part.is_symlink() or (hasattr(part, "is_junction") and part.is_junction()):
            raise ValueError(f"Linked output path: {part}")


def destination_name(name: str) -> str:
    if name in DOCUMENTS or name.startswith(("licenses/", "redistribution/")):
        return "docs/" + name
    return name


def rewrite_document(text: str, old_name: str, names: dict[str, str]) -> str:
    """Rewrite only references to files actually present in this package.

    References to upstream/source files are deliberately left alone. License
    texts and runtime assets are never passed through this function.
    """
    old_parent = posixpath.dirname(old_name)
    new_parent = posixpath.dirname(names[old_name]) or "."

    def relocated(reference: str) -> str:
        if re.match(r"^[a-zA-Z][a-zA-Z0-9+.-]*:", reference) or reference.startswith(("/", "#")):
            return reference
        path, marker, anchor = reference.partition("#")
        target = posixpath.normpath(posixpath.join(old_parent, unquote(path)))
        if target not in names:
            return reference
        result = posixpath.relpath(names[target], new_parent)
        return result + (marker + anchor if marker else "")

    # Protect Markdown URLs from the plain-filename pass below.
    links: list[str] = []

    def link(match: re.Match) -> str:
        links.append(match[1] + relocated(match[2]) + match[3])
        return f"\x00LINK{len(links) - 1}\x00"

    text = MARKDOWN_LINK.sub(link, text)
    text = re.sub(r"`([^`\n]+)`", lambda match: "`" + relocated(match[1]) + "`", text)
    # Older package READMEs also mention RELEASE_NOTES.md as plain prose.
    for name in DOCUMENTS | {"README.md"}:
        replacement = relocated(name)
        if name != replacement:
            text = re.sub(r"(?<![\w/`.-])" + re.escape(name) + r"(?![\w/`-]|\.[\w])",
                          lambda _: replacement, text)
    for index, value in enumerate(links):
        text = text.replace(f"\x00LINK{index}\x00", value)
    return text


def package_player(manifest_path: Path, destination: Path, archive: Path,
                   replacements: dict | None = None) -> dict:
    manifest = read_json(manifest_path)
    source_archive = Path(manifest["archive"])
    if digest(source_archive) != manifest["archiveSHA256"]:
        raise ValueError("Source archive has changed.")
    expected = manifest["files"]
    names = {safe_name(name): destination_name(name) for name in expected}
    for name in names:
        path = PurePosixPath(name)
        if (path.suffix.lower() in PRIVATE_SUFFIXES or path.parts[0].lower() in PRIVATE_ROOTS or
                name.removesuffix(".bak") in STATE_FILES or name.startswith("assets/sky/") or
                name.startswith("music/") and name != "music/README.txt"):
            raise ValueError(f"Player data or diagnostic material in package: {name}")
    if len({name.casefold() for name in names.values()}) != len(names):
        raise ValueError("Package paths collide after document consolidation.")
    if "README.md" not in names:
        raise ValueError("A player package must contain README.md.")
    platform = "windows" if "RoadRash64Recompiled.exe" in names else "linux"
    executable = ("RoadRash64Recompiled.exe" if platform == "windows" else
                  "RoadRash64Recompiled-x86_64.AppImage")
    if executable not in names:
        raise ValueError("Unrecognized player package executable.")
    replacements = replacements or {}
    for name, entry in replacements.items():
        if name not in names or name not in REPLACEABLE or name == "SHA256SUMS.txt":
            raise ValueError(f"Not an allowed replacement: {name}")
        if digest(Path(entry["path"])) != entry["sha256"]:
            raise ValueError(f"Replacement has changed: {name}")
    for path in (destination, archive):
        reject_links(path)
        if path.exists():
            raise ValueError(f"Output already exists: {path}")
    destination = destination.resolve()
    archive = archive.resolve()
    if archive.is_relative_to(destination) or source_archive.resolve().is_relative_to(destination):
        raise ValueError("Archives must be outside the new player folder.")

    rewritten = []
    with zipfile.ZipFile(source_archive) as source:
        entries = {}
        roots = set()
        for entry in source.infolist():
            if entry.is_dir():
                continue
            safe_name(entry.filename)
            parts = PurePosixPath(entry.filename).parts
            if len(parts) < 2 or (entry.external_attr >> 16) & 0o170000 == 0o120000:
                raise ValueError("Expected regular files inside one archive folder.")
            roots.add(parts[0])
            name = PurePosixPath(*parts[1:]).as_posix()
            if name in entries:
                raise ValueError(f"Duplicate archive entry: {name}")
            entries[name] = entry
        if len(roots) != 1 or set(entries) != set(expected):
            raise ValueError("Source archive does not match the complete manifest.")
        # Verify even replaced and regenerated entries, before producing output.
        for name, entry in entries.items():
            with source.open(entry) as stream:
                if hashlib.file_digest(stream, "sha256").hexdigest() != expected[name]:
                    raise ValueError(f"Source entry has changed: {name}")
        destination.mkdir(parents=True)
        for name, entry in sorted(entries.items()):
            if name == "SHA256SUMS.txt":
                continue
            target = destination / names[name]
            target.parent.mkdir(parents=True, exist_ok=True)
            if name in replacements:
                shutil.copyfile(replacements[name]["path"], target)
                required_hash = replacements[name]["sha256"]
            else:
                with source.open(entry) as src, target.open("xb") as dst:
                    shutil.copyfileobj(src, dst, 1024 * 1024)
                required_hash = expected[name]
            if digest(target) != required_hash:
                raise ValueError(f"Copy verification failed: {name}")
            if name == "README.md" or name in DOCUMENTS and name.endswith(".md"):
                original = target.read_text(encoding="utf-8")
                updated = rewrite_document(original, name, names)
                if name == "README.md":
                    updated += ("\n\n## Folder guide\n\n"
                                "Release notes, controller help, credits, licenses and build details "
                                "are in [docs](docs/). File checksums in docs/SHA256SUMS.txt "
                                "are relative to this game folder. Keep the full extracted package "
                                "together.\n\n"
                                "This layout keeps existing settings, saves, installed mods, "
                                "imported courses and custom music in their established locations. "
                                "Keep your previous installation when updating.\n")
                if updated != original:
                    target.write_text(updated, encoding="utf-8", newline="\n")
                    rewritten.append(names[name])
            if name.endswith(".AppImage"):
                target.chmod(0o755)

    files = {path.relative_to(destination).as_posix(): digest(path)
             for path in sorted(destination.rglob("*")) if path.is_file()}
    checksums = destination / CHECKSUMS
    checksums.parent.mkdir(parents=True, exist_ok=True)
    checksums.write_text("".join(f"{value}  {name}\n" for name, value in sorted(files.items())),
                         encoding="utf-8", newline="\n")
    files[CHECKSUMS] = digest(checksums)
    archive.parent.mkdir(parents=True, exist_ok=True)
    with zipfile.ZipFile(archive, "x", compression=zipfile.ZIP_DEFLATED, compresslevel=6) as output:
        for name in sorted(files):
            path = destination / name
            entry = zipfile.ZipInfo.from_file(path, destination.name + "/" + name)
            entry.create_system = 3
            entry.external_attr = (0o100755 if name.endswith(".AppImage") else 0o100644) << 16
            entry.compress_type = (zipfile.ZIP_STORED if path.suffix in {".zip", ".AppImage", ".gz", ".xz"}
                                   else zipfile.ZIP_DEFLATED)
            with path.open("rb") as src, output.open(entry, "w") as dst:
                shutil.copyfileobj(src, dst, 1024 * 1024)
    with zipfile.ZipFile(archive) as output:
        if output.testzip() is not None:
            raise ValueError("Output ZIP failed its integrity check.")
        for name, required_hash in files.items():
            with output.open(destination.name + "/" + name) as stream:
                if hashlib.file_digest(stream, "sha256").hexdigest() != required_hash:
                    raise ValueError(f"Output ZIP file differs: {name}")
    if digest(source_archive) != manifest["archiveSHA256"]:
        raise ValueError("Source archive changed while packaging.")
    return dict(passed=True, platform=platform, directory=str(destination), archive=str(archive),
                archiveSHA256=digest(archive), files=files, sourceManifest=str(manifest_path.resolve()),
                sourceArchiveSHA256=manifest["archiveSHA256"],
                relocated={old: new for old, new in names.items() if old != new},
                rewrittenDocuments=rewritten, replacements=replacements,
                rootEntries=sorted(path.name for path in destination.iterdir()),
                runtimePathsUnchanged=True, playerDataIncluded=False,
                gameLaunched=False, published=False)


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("manifest", type=Path)
    parser.add_argument("destination", type=Path)
    parser.add_argument("archive", type=Path)
    parser.add_argument("report", type=Path)
    parser.add_argument("--replacements", type=Path,
                        help='JSON object: package path -> {"path": source file, "sha256": hash}.')
    args = parser.parse_args()
    reject_links(args.report)
    if args.report.exists() or args.report.resolve().is_relative_to(args.destination.resolve()):
        parser.error("Report must be new and outside the player folder.")
    replacements = read_json(args.replacements) if args.replacements else None
    result = package_player(args.manifest, args.destination, args.archive, replacements)
    args.report.parent.mkdir(parents=True, exist_ok=True)
    with args.report.open("x", encoding="utf-8") as stream:
        json.dump(result, stream, indent=2)
        stream.write("\n")
    print(f"Verified {len(result['files'])} files; {len(result['rootEntries'])} top-level entries.")


if __name__ == "__main__":
    main()
