"""Assemble a complete local course mod from the two user-supplied ROMs."""

from __future__ import annotations
import struct
from pathlib import Path

from . import CONVERTER_VERSION, SOURCE_REFERENCE_COMMIT, COURSE_SCALE
from .common import COURSES, RR64_SHA256, MK64_SHA256, read_rom, require, digest, json_bytes
from .source_rom import Donor
from .extract_courses import extract_course, LAYOUT as SOURCE_LAYOUT
from .source_displaylists import DisplayLists
from .material_policy import apply as apply_material_policy
from .texture_registry import create as texture_registry, discover, finalize
from .terrain import build_course
from .route_assembly import (
    LAYOUT as ROUTE_LAYOUT,
    root_membership,
    historical_cell_provider,
    assemble_route,
)
from .assets import Assets
from .course_extras import boosts, wood_surface_ids
from . import native_cell


class PackWriter:
    """Write only generated allowlisted files; keep all work outside the pack."""

    def __init__(self, directory):
        self.directory = Path(directory)
        require(
            not self.directory.exists() or not any(self.directory.iterdir()),
            "Import staging folder must be empty",
        )
        self.directory.mkdir(parents=True, exist_ok=True)
        self.files = {}

    def add(self, name, data):
        path = Path(name)
        require(
            not path.is_absolute() and ".." not in path.parts and "\\" not in name,
            "Invalid generated file name",
        )
        require(name not in self.files and len(data) > 0, "Duplicate or empty generated file")
        destination = self.directory / path
        destination.parent.mkdir(parents=True, exist_ok=True)
        destination.write_bytes(data)
        record = {"file": name, "bytes": len(data), "sha256": digest(data)}
        self.files[name] = record
        return record

    def add_json(self, name, data):
        return self.add(name, json_bytes(data))

    def verify_payloads(self, progress):
        """Recheck staged bytes before the catalogue makes the pack complete."""
        for name, record in self.files.items():
            progress.check()
            data = (self.directory / name).read_bytes()
            require(
                len(data) == record["bytes"] and digest(data) == record["sha256"],
                "Generated course file changed",
            )


def _write_work_files(directory, files):
    """Keep decoded intermediates outside the installed pack's file inventory."""
    for filename, data in files.items():
        destination = directory / filename
        destination.parent.mkdir(parents=True, exist_ok=True)
        destination.write_bytes(data)


def prepare_courses(donor, rr_rom, output, work, helper, progress):
    """Build terrain and routes; retain original source state for actor placement."""
    assets = Assets(donor)
    records = texture_registry()
    writer = PackWriter(output)
    courses = []
    geometries = {}
    sources = {}
    items = {}
    walls = {}
    surfaces = {}
    cells = []
    seen_cells = set()
    proof = {}
    for course_index, (course_id, slug, title) in enumerate(COURSES):
        percent = 3 + 64 * course_index / len(COURSES)
        progress.update("courses", "Converting " + title, percent)
        raw, textures = extract_course(donor, slug)
        source = apply_material_policy(raw)
        directory = work / "sources" / slug
        _write_work_files(directory, textures)
        # Discover variants before terrain corrections remove unused draw lists.
        # This preserves stable native texture IDs across every regenerated pack.
        discover(source, directory, records)
        arrays = DisplayLists(donor, slug, SOURCE_LAYOUT["courses"][slug])
        roots = root_membership(source, arrays)
        placement = {"terrain_translation": ROUTE_LAYOUT["stages"][-1]["placements"][slug]}
        geometry, payloads, course_walls, course_surfaces, report = build_course(
            source, roots, arrays, directory, records, placement
        )
        progress.check()
        report.pop("source")
        geo_hash = digest(json_bytes(geometry))
        legacy = None
        if slug == "mario_raceway":
            legacy, extra = extract_course(donor, slug, include_legacy_pipe=True)
            _write_work_files(directory, extra)
        provider = historical_cell_provider(
            source, directory, roots, legacy_source=legacy, cancel=progress.check
        )
        route, blob = assemble_route(
            source,
            provider,
            helper,
            work / "routes" / slug,
            geo_hash,
            cancel=progress.check,
            progress=lambda message: progress.update("routes", message, percent + 2),
        )
        writer.add_json(f"courses/{slug}/route/route.json", route)
        writer.add(f"courses/{slug}/route/route.bin", blob)
        writer.add(f"courses/{slug}/preview.rgba16", assets.preview(slug))
        indices = []
        for filename, data in sorted(payloads.items()):
            index = int(Path(filename).stem.split("-")[1])
            require(0 <= index < 4900 and index not in seen_cells, "Imported course cells overlap")
            require(
                struct.unpack_from(">I", rr_rom, 0x18D398 + (926 + index) * 12)[0] == 0,
                "Imported course overwrites original game terrain",
            )
            native_cell.validate_cell(data, roundtrip=True)
            require(struct.unpack_from(">I", data, 8)[0] == index, "Terrain cell identity mismatch")
            seen_cells.add(index)
            indices.append(index)
            cells.append(
                {
                    "course": slug,
                    "index": index,
                    **writer.add(f"courses/{slug}/cells/{filename}", data),
                }
            )
        entry = {
            "id": slug,
            "name": title,
            "source_course_id": course_id,
            "source_to_world_scale": COURSE_SCALE,
            "route": f"courses/{slug}/route/route.json",
            "preview": f"courses/{slug}/preview.rgba16",
            "preview_width": 128,
            "preview_height": 78,
            "cells": indices,
            "item_boxes": f"courses/{slug}/items.json",
            "walls": f"courses/{slug}/walls.json",
            "surfaces": f"courses/{slug}/surfaces.json",
            "hazards": f"courses/{slug}/hazards.json",
            "sky": assets.sky(slug, geometry, source),
        }
        pads = boosts(source, course_surfaces)
        if pads is not None:
            entry["boosts"] = f"courses/{slug}/boosts.json"
            writer.add_json(entry["boosts"], pads)
        wood = wood_surface_ids(source, course_surfaces)
        if wood:
            entry["audio_wood_surfaces"] = wood
        courses.append(entry)
        geometries[slug] = geometry
        # Actor paths and placement use the source floor, not promoted terrain.
        sources[slug] = source
        items[slug] = assets.items(slug, geometry)
        walls[slug] = course_walls
        surfaces[slug] = course_surfaces
        proof[slug] = report
    textures = []
    for row, blob in finalize(records):
        textures.append({**row, **writer.add(f"textures/texture-{row['index']:04d}.bin", blob)})
    writer.add("items/item-box.bin", assets.item_model())
    return dict(
        writer=writer,
        courses=courses,
        geometries=geometries,
        sources=sources,
        items=items,
        walls=walls,
        surfaces=surfaces,
        cells=cells,
        textures=textures,
        proof=proof,
    )


def convert(mk64_rom, rr64_rom, output, helper, motion_helper, progress):
    """Return the completed catalogue hash, or fail without installation."""
    progress.update("verify", "Checking the selected ROMs", 0)
    mk64 = read_rom(mk64_rom, MK64_SHA256, 0xC00000, "Mario Kart 64 USA")
    rr64 = read_rom(rr64_rom, RR64_SHA256, 0x2000000, "Road Rash 64 USA")
    donor = Donor(mk64)
    output = Path(output)
    work = output.parent / "work"
    require(Path(helper).is_file(), "The course import helper is missing. Reinstall this build.")
    require(
        Path(motion_helper).is_file(), "The course motion helper is missing. Reinstall this build."
    )
    work.mkdir(parents=True, exist_ok=True)
    prepared = prepare_courses(donor, rr64, output, work, helper, progress)
    writer = prepared["writer"]

    progress.update("actors", "Converting animated course objects", 68)
    from .actor_hazards import build as rigid_build
    from .actor_sprites import build as sprite_build
    from .actor_meshes import build as mesh_build

    rigid_bank, rigid_models = rigid_build(donor)
    sprite_bank, sprite_manifest = sprite_build(donor, rigid_bank)
    mesh_bank, mesh_manifest = mesh_build(donor)
    progress.check()
    # Placement and composition consume the same freshly decoded source state.
    from .actor_placement import build as placement_build
    from .actors import compose

    placement = placement_build(donor, mesh_manifest, prepared["sources"], motion_helper)
    Assets.special_koopa_item(
        prepared["items"]["koopa_troopa_beach"],
        prepared["geometries"]["koopa_troopa_beach"],
        placement["special_koopa_item"],
    )
    bank, hazards, items = compose(
        prepared["courses"],
        prepared["geometries"],
        prepared["sources"],
        prepared["items"],
        sprite_manifest,
        mesh_manifest,
        sprite_bank,
        mesh_bank,
        rigid_models,
        placement,
    )
    from race_pack_neon_transparency import adapt_neon_models

    neon = {m["id"]: m["texture"].get("asset", "") for m in sprite_manifest["models"]}
    bank, _ = adapt_neon_models(bank, neon)
    writer.add("hazards/hazard-models.bin", bank)
    for course in prepared["courses"]:
        slug = course["id"]
        writer.add_json(course["item_boxes"], items[slug])
        writer.add_json(course["hazards"], hazards[slug])
        writer.add_json(course["walls"], prepared["walls"][slug])
        writer.add_json(course["surfaces"], prepared["surfaces"][slug])

    from .audio_banks import effects, songs

    writer.add("audio/course-audio.bin", effects(mk64, progress))
    writer.add("audio/course-music.bin", songs(mk64, progress))
    writer.add_json(
        "coverage.json",
        {
            "format": "rr64-rom-import-coverage",
            "version": 1,
            "converter_version": CONVERTER_VERSION,
            "courses": len(COURSES),
            "cells": len(prepared["cells"]),
            "source_rom_sha256": MK64_SHA256,
            "base_rom_sha256": RR64_SHA256,
            "stock_assets_serialized": False,
            "game_launched": False,
        },
    )
    catalogue = {
        "format": "rr64-race-pack-catalogue",
        "version": 1,
        "group_id": "mk64",
        "group_name": "MK64",
        "converter_version": CONVERTER_VERSION,
        "source_reference_commit": SOURCE_REFERENCE_COMMIT,
        "base_rom_sha256": RR64_SHA256,
        "source_rom_sha256": MK64_SHA256,
        "source_to_world_scale": COURSE_SCALE,
        "courses": prepared["courses"],
        "textures": prepared["textures"],
        "cells": prepared["cells"],
        "files": [record for name, record in sorted(writer.files.items())],
        "coverage": "coverage.json",
        "stock_assets_serialized": False,
        "live_acceptance": False,
        "item_box_asset": "items/item-box.bin",
        "hazard_asset": "hazards/hazard-models.bin",
        "course_audio_asset": "audio/course-audio.bin",
        "course_music_asset": "audio/course-music.bin",
    }
    progress.update("validate", "Checking the complete course pack", 98)
    writer.verify_payloads(progress)
    # The catalogue is emitted last; the native worker owns validation/install.
    result = writer.add_json("catalogue.json", catalogue)
    progress.update("complete", "Course conversion complete", 100)
    return result["sha256"]
