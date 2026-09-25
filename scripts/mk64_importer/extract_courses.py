"""Extract neutral race-course data from a user's verified cartridge image.

Address recipes describe layout only. Geometry, paths, collision, display lists,
and texels are always read from the ROM; no donor source checkout is required.
"""

from __future__ import annotations
from collections import Counter
import hashlib, json, struct
from pathlib import Path
from .source_rom import Donor, COURSES, US_SHA1, require
from .source_displaylists import DisplayLists
from .source_walk import Walker, canonical, orientation_key

LAYOUT = json.loads(Path(__file__).with_name("source_layout.json").read_text())


def digest(data):
    return hashlib.sha256(data).hexdigest()


def state(spec):
    result = Walker.state(**{k: v for k, v in spec.items() if k in ("edge", "cull", "shade")})
    if spec.get("intensity"):
        result["combine"] = ["G_CC_MODULATEI"] * 2
    if spec.get("rgba"):
        result["combine"] = ["G_CC_MODULATERGBA"] * 2
    if spec.get("decal"):
        result["combine"] = ["G_CC_DECALRGBA"] * 2
    if spec.get("fog"):
        result["geometry_modes"].add("G_FOG")
        if not spec.get("edge") and not spec.get("intensity"):
            result["combine"][0] = "G_CC_MODULATERGB"
        result["combine"][1] = "G_CC_PASS2"
        result["render"][0] = "G_RM_FOG_SHADE_A"
    if spec.get("translucent"):
        result["render"] = ["G_RM_AA_ZB_XLU_INTER", "G_RM_NOOP2"]
    return result


def extract_course(donor, slug, include_legacy_pipe=False):
    info = LAYOUT["courses"][slug]
    prefix = f"d_course_{slug}_"
    table = donor.course_table(slug)
    packed_vertices = donor.mio0(table[2] + (table[6] & 0xFFFFFF))
    vertices = []
    for offset in range(0, table[7] * 14, 14):
        x, y, z, s, t, r, g, b, unused = struct.unpack_from(">5h4B", packed_vertices, offset)
        vertices.append(
            {
                "position": [x, y, z],
                "texcoord_st": [s, t],
                "color_rgba": [r & 252, g & 252, b, 255],
                "flag": (r & 3) | ((g & 3) << 2),
                "stored_unused_byte": unused,
            }
        )
    textures = {t["symbol"]: dict(t) for t in info["textures"]}
    arrays = DisplayLists(donor, slug, info)

    def pointer_list(name):
        entry = info["section_tables"][name]
        data = donor.read(slug, entry["address"], entry["count"] * 4)
        return [arrays.name(p[0]) for p in struct.iter_unpack(">I", data)]

    root_table, initial, extras = info["roots"]
    roots = [(name, initial, "section-direction") for name in pointer_list(root_table)]
    roots += [
        (prefix + (name if name.startswith("dl_") else "packed_dl_" + name), spec, "runtime-extra")
        for name, spec in extras
    ]
    if include_legacy_pipe and slug == "mario_raceway":
        # Reproduce the original collision-probe stages before the duplicate
        # multiplayer pipe was removed. This variant is never rendered in the
        # finished pack; it only preserves established route margin metadata.
        position = (
            next(i for i, item in enumerate(roots) if item[0] == prefix + "packed_dl_8E8") + 1
        )
        roots.insert(position, (prefix + "packed_dl_2D68", {}, "runtime-extra"))
    if slug == "sherbet_land":
        roots += [(name, {}, "secondary-section") for name in pointer_list("sherbet_land_dls_2")]
    walker = Walker(arrays, vertices, textures)
    materials = []
    material_ids = {}
    triangles = []
    seen = set()
    observations = 0

    def emit(ids, current, name):
        nonlocal observations
        observations += 1
        try:
            material = walker.material(current)
        except ValueError as error:
            raise ValueError(str(error) + " in " + name) from error
        key = canonical(material)
        if key not in material_ids:
            material_ids[key] = len(materials)
            materials.append({"id": len(materials), **material})
        mid = material_ids[key]
        key = (orientation_key(ids), mid)
        if key not in seen:
            seen.add(key)
            triangles.append({"vertices": ids, "material": mid, "display_list": name})

    for name, spec, _ in roots:
        walker.walk(name, state(spec), emit)
    render_lists = len(walker.visited)
    paths = {}
    for name, entry in info["paths"].items():
        data = donor.read(slug, entry["address"], (entry["count"] + 1) * 8)
        rows = list(struct.iter_unpack(">hhhH", data))
        require(rows[-1][0] == -32768, "Missing route sentinel")
        paths[name] = [{"position": list(row[:3]), "section": row[3]} for row in rows[:-1]]
    entry = info["collision_table"]
    data = donor.read(slug, entry["address"], (entry["count"] + 1) * 8)
    records = list(struct.iter_unpack(">IBBH", data))
    require(records[-1][0] == 0, "Missing collision sentinel")
    collision_roots = [
        (
            arrays.name(address),
            "254" if surface == 254 else LAYOUT["surface_names"][str(surface)],
            section,
            flags,
        )
        for address, surface, section, flags in records[:-1]
    ]
    # Original single/multiplayer pipe meshes are alternatives, not overlapping
    # terrain; keep the detailed 8E8 variant for every imported play mode.
    if slug == "mario_raceway":
        collision_roots = [
            (prefix + "packed_dl_" + name, "SURFACE_DEFAULT", 255, 0)
            for name in (("1140", "8E8", "2D68") if include_legacy_pipe else ("1140", "8E8"))
        ] + collision_roots
    collisions = []
    seen_collision = set()
    for root, surface, section, flags in collision_roots:

        def emit_collision(ids, current, name):
            key = (orientation_key(ids), surface, section, flags)
            if key not in seen_collision:
                seen_collision.add(key)
                collisions.append(
                    {
                        "vertices": ids,
                        "surface": surface,
                        "section": section,
                        "flags": flags,
                        "display_list": name,
                    }
                )

        walker.walk(root, state({}), emit_collision)
    raw_textures = {}
    for texture in textures.values():
        decoded = donor.mio0(texture["rom_offset"], texture["decoded_size"])
        require(texture["block_offset"] == 0, "Unsupported terrain texture sub-block")
        filename = "textures/" + texture["filename"] + "." + texture["format"] + ".bin"
        texture.update(file=filename, sha256=digest(decoded), byte_order="big-endian")
        raw_textures[filename] = decoded
    # Reproduce TMEM load/render row swaps when a tile uses a subview of a load.
    for material in materials:
        view = material.get("texture_view")
        if view is None:
            continue
        original = textures[material["texture"]]
        pixels = raw_textures[original["file"]]
        tmem = {}
        for i, value in enumerate(pixels):
            address = view["load_start"] * 8 + i
            if ((i // 8 * view["load_dxt"]) // 2048) & 1:
                address ^= 4
            tmem[address] = value
        decoded = bytearray()
        for y in range(view["height"]):
            for x in range(view["width"] * 2):
                address = view["render_start"] * 8 + y * view["render_line"] * 8 + x
                if y & 1:
                    address ^= 4
                require(address in tmem, "Uninitialized terrain TMEM sample")
                decoded.append(tmem[address])
        name = original["symbol"] + "__view_" + digest(canonical(view).encode())[:12]
        texture = {
            **original,
            "symbol": name,
            "source_symbol": original["symbol"],
            "width": view["width"],
            "height": view["height"],
            "decoded_size": len(decoded),
            "texture_view": view,
            "file": "textures/" + name + "." + original["format"] + ".bin",
            "sha256": digest(decoded),
        }
        textures[name] = texture
        raw_textures[texture["file"]] = bytes(decoded)
        material["texture"] = name
    path = paths[prefix + "track_path"]
    output = {
        "format": "rr64-mk64-course",
        "version": 1,
        "course": slug,
        "source": {
            "repository": "https://github.com/n64decomp/mk64",
            "commit": LAYOUT["reference_commit"],
            "extraction": "verified-user-rom",
            "rom": {"normalized_sha1": US_SHA1, "normalized_sha256": digest(donor.rom)},
        },
        "vertices": vertices,
        "textures": list(textures.values()),
        "materials": materials,
        "triangles": triangles,
        "path": path,
        "paths": paths,
        "collision_triangles": collisions,
        "roots": [{"name": n, "initial_state": s, "kind": k} for n, s, k in roots],
        "collision_roots": [
            {"name": n, "surface": s, "section": i, "flags": f} for n, s, i, f in collision_roots
        ],
        "validation": {
            "vertices": len(vertices),
            "materials": len(materials),
            "textures": len(textures),
            "render_root_count": len(roots),
            "reached_render_lists": render_lists,
            "render_triangle_observations": observations,
            "render_triangles": len(triangles),
            "path_points": len(path),
            "named_paths": len(paths),
            "collision_triangles": len(collisions),
            "collision_surface_counts": dict(Counter(t["surface"] for t in collisions)),
            "bounds_min": [min(v["position"][i] for v in vertices) for i in range(3)],
            "bounds_max": [max(v["position"][i] for v in vertices) for i in range(3)],
            "all_indices_valid": True,
            "all_reached_commands_supported": True,
            "user_rom_textures_extracted": True,
        },
        "limitations": [
            "Terrain is a static union of camera sections; dynamic actors are converted separately."
        ],
    }
    return output, raw_textures


def extract_courses(rom_path, output_dir, progress=None):
    """Return slug -> neutral course.json path; only the supplied ROM supplies assets."""
    donor = Donor(rom_path, LAYOUT)
    output_dir = Path(output_dir)
    result = {}
    for index, (_, slug, title) in enumerate(COURSES):
        if progress:
            progress(f"Reading {title}", index, len(COURSES))
        output, textures = extract_course(donor, slug)
        directory = output_dir / slug
        directory.mkdir(parents=True, exist_ok=True)
        for filename, data in textures.items():
            path = directory / filename
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_bytes(data)
        path = directory / "course.json"
        path.write_text(json.dumps(output, separators=(",", ":")) + "\n")
        result[slug] = path
    return result
