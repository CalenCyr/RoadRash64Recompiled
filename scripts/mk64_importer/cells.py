"""Build private native terrain-cell records from converted course geometry.

No ROM/game installation is changed. Texture IDs refer to the separate imported
material catalogue (926 and above). All authored collision surfaces map to
placeholder native index 5; original semantics remain in the manifest.
"""

from __future__ import annotations

from collections import Counter
import hashlib
import struct

from . import native_cell as native


def require(ok, message):
    if not ok:
        raise ValueError(message)


def fingerprint(data):
    return hashlib.sha256(data).hexdigest()


def decoded_payloads(blob):
    """Independent semantic decoding for comparison with every input triangle."""
    origin = struct.unpack_from(">2f", blob, 0x1C)
    vertex_arrays = {}
    rendered, collision = Counter(), Counter()
    for submesh in range(struct.unpack_from(">H", blob, 12)[0]):
        ref = 0xF0 + submesh * 12
        start = ref + struct.unpack_from(">I", blob, ref)[0]
        packets, visible = struct.unpack_from(">HH", blob, start + 12)
        texture = struct.unpack_from(">H", blob, start + 18)[0]
        p = start + 0x18
        for packet in range(packets):
            triangles, count, size, vertex_bytes = struct.unpack_from(">4H", blob, p)
            vertices = []
            for i in range(count):
                x, y, z, flag, s, t, r, g, b, a = struct.unpack_from(
                    ">hhhHhh4B", blob, p + 8 + i * 16
                )
                require(flag == 0, "unexpected output vertex flag")
                vertices.append((int(x + origin[0]), int(y + origin[1]), z, s, t, r, g, b, a))
            vertex_arrays[p + 8] = vertices
            for i in range(triangles):
                word = struct.unpack_from(">H", blob, p + 8 + vertex_bytes + i * 2)[0]
                indices = (word >> 10 & 31, word >> 5 & 31, word & 31)
                if packet < visible:
                    rendered[(tuple(vertices[index] for index in indices), texture)] += 1
            p += size
    c = struct.unpack_from(">I", blob, 16)[0]
    for group in range(struct.unpack_from(">H", blob, c + 14)[0]):
        p = c + struct.unpack_from(">H", blob, c + 16 + group * 2)[0]
        bucket, lists = struct.unpack_from(">HH", blob, p)
        p += 4
        for _ in range(lists):
            offset, surface, count = struct.unpack_from(">HBB", blob, p)
            for i in range(count):
                word = struct.unpack_from(">H", blob, p + 4 + i * 2)[0]
                indices = (word >> 10 & 31, word >> 5 & 31, word & 31)
                collision[
                    (bucket, tuple(vertex_arrays[offset][index] for index in indices), surface)
                ] += 1
            p += 4 + 2 * (count + (count & 1))
    return rendered, collision


def expected_payloads(cell_index, triangles):
    rendered, collision = Counter(), Counter()
    origin = native.cell_origin(cell_index)
    for tri in triangles:
        vertices = tuple((v.x, v.y, v.z, v.s, v.t, *v.rgba) for v in tri.vertices)
        if tri.render:
            rendered[(vertices, tri.texture)] += 1
        if tri.surface is None:
            continue
        # Independent conservative coverage check, including exact boundaries.
        for x in range(8):
            for y in range(8):
                inside = True
                for axis, bin_index in enumerate((x, y)):
                    lo = origin[axis] - 500 + bin_index * 125
                    hi = lo + 125
                    values = [v[axis] for v in vertices]
                    inside &= max(values) >= lo and min(values) <= hi
                if inside:
                    bucket = sum(
                        ((x >> b) & 1) << (2 * b) | ((y >> b) & 1) << (2 * b + 1) for b in range(3)
                    )
                    collision[(bucket, vertices, tri.surface)] += 1
    return rendered, collision


def build(course):
    require(
        course.get("format") == "rr64-private-course-geometry" and course.get("version") == 1,
        "unsupported converted geometry format",
    )
    require(
        course["transform"]["terrain_vertex_scale_from_world"] == [4, 4, 8]
        and course["transform"]["reverse_triangle_winding"] is True,
        "unconfirmed coordinate contract",
    )
    textures = course["textures"]
    require(0 < len(textures) <= native.TEXTURES, "too many provisional textures")
    by_symbol = {
        texture["symbol"]: texture.get("native_index", i) for i, texture in enumerate(textures)
    }
    require(len(by_symbol) == len(textures), "duplicate texture symbols")
    materials = course["materials"]
    mapping = []
    material_counts, surface_counts = Counter(), Counter()
    for index, material in enumerate(materials):
        require(material["id"] == index, "nonsequential material IDs")
        texture = material["texture"] if material["texture_enabled"] else None
        require(texture is None or texture in by_symbol, "unknown material texture")
        warnings = [
            "Native material-state compatibility is unverified; source state is retained only in this manifest."
        ]
        if texture is not None:
            warnings.append(
                "RGBA16 source texture is not installed or converted to a native terrain texture record."
            )
        if material["render"] != ["G_RM_AA_ZB_OPA_SURF", "G_RM_AA_ZB_OPA_SURF2"]:
            warnings.append(
                "Source alpha/cutout render mode is not represented by the cell encoder."
            )
        if "G_CULL_BACK" not in material["geometry_modes"]:
            warnings.append(
                "Source disabled back-face culling is not represented by the cell encoder."
            )
        mapping.append(
            {
                "material_id": index,
                "provisional_texture_index": by_symbol[texture] if texture else 0xFFFF,
                "source_state": material,
                "warnings": warnings,
            }
        )
    payloads, cells, totals = {}, [], Counter()
    seen_cells = set()
    attribution = []
    for cell in course["cells"]:
        cell_index = cell["index"]
        require(
            type(cell_index) is int and cell_index not in seen_cells, "invalid/duplicate cell index"
        )
        seen_cells.add(cell_index)
        triangles, cell_attribution = [], []
        for number, tri in enumerate(cell["triangles"]):
            require(type(tri["render"]) is bool, "invalid triangle visibility")
            require(len(tri["vertices"]) == 3, "triangle requires exactly three vertices")
            for v in tri["vertices"]:
                require(
                    len(v) == 9 and all(type(n) is int for n in v), "unquantized/malformed vertex"
                )
            material_id = tri["material"]
            require(
                (material_id is None and not tri["render"])
                or (type(material_id) is int and 0 <= material_id < len(materials)),
                "unknown triangle material",
            )
            texture = (
                mapping[material_id]["provisional_texture_index"]
                if material_id is not None
                else 0xFFFF
            )
            require(isinstance(tri["collision"], list), "invalid collision attribution")
            for attribute in tri["collision"]:
                require(
                    isinstance(attribute["surface"], str)
                    and type(attribute["section"]) is int
                    and type(attribute["flags"]) is int,
                    "invalid source surface attribution",
                )
                surface_counts[attribute["surface"]] += 1
            vertices = tuple(native.Vertex(*v[:5], tuple(v[5:])) for v in tri["vertices"])
            triangles.append(
                native.Triangle(
                    vertices,
                    texture=texture,
                    surface=5 if tri["collision"] else None,
                    render=tri["render"],
                    translucent=bool(
                        material_id is not None
                        and materials[material_id].get("native_features", 0) & 128
                    ),
                )
            )
            material_counts[str(material_id)] += int(tri["render"])
            cell_attribution.append(
                {
                    "input_triangle": number,
                    "source_triangle": tri["source_triangle"],
                    "material": material_id,
                    "source_collision": tri["collision"],
                    "native_surface_placeholder": 5 if tri["collision"] else None,
                }
            )
        blob = native.encode_cell(cell_index, triangles)
        require(
            native.encode_cell(cell_index, triangles) == blob, "cell encoding is nondeterministic"
        )
        counts = native.validate_cell(blob, roundtrip=True)
        require(
            decoded_payloads(blob) == expected_payloads(cell_index, triangles),
            "encoded rendering/contact payload differs from original triangles or bucket coverage",
        )
        name = f"cell-{cell_index:04d}.bin"
        payloads[name] = blob
        cells.append(
            {
                "index": cell_index,
                "file": name,
                "bytes": len(blob),
                "sha256": fingerprint(blob),
                "input_triangles": len(triangles),
                "input_collision_triangles": sum(t.surface is not None for t in triangles),
                "validated": counts,
            }
        )
        totals.update(counts)
        totals["bytes"] += len(blob)
        totals["input_triangles"] += len(triangles)
        totals["input_collision_triangles"] += sum(t.surface is not None for t in triangles)
        attribution.append({"cell": cell_index, "triangles": cell_attribution})
    require(bool(cells), "course has no cells")
    manifest = {
        "format": "rr64-private-native-course-cells",
        "version": 1,
        "source": course["source"],
        "course": course["course"],
        "transform": course["transform"],
        "validation": {
            "cells": len(cells),
            **dict(totals),
            "deterministic_reencode": True,
            "all_references_valid": True,
            "semantic_render_and_contact_comparison": True,
            "native_execution": False,
            "game_modified": False,
        },
        "cells": cells,
        "textures": [
            {"provisional_native_index": t.get("native_index", i), "source": t}
            for i, t in enumerate(textures)
        ],
        "materials": mapping,
        "render_triangle_material_counts": dict(material_counts),
        "source_surface_piece_counts": dict(surface_counts),
        "triangle_attribution": attribution,
        "limitations": [
            "Private offline binaries; not installed, launched, packaged or visually accepted.",
            "Texture records are separate catalogue assets; native imported indices start at 926.",
            "Direct RGBA16/IA16, mirror/clamp, culling and UV scale are converted; original fog, animation and specialized combiner semantics are not reproduced.",
            "Native collision surface 5 is an explicit placeholder for every source surface, not a verified semantic mapping.",
            "Collision list triangle counts include conservative bucket duplication, not unique geometry.",
            "Opaque geometry is grouped by texture; translucent consecutive runs preserve the supplied pass order. Camera-dependent ordering across cells is not reproduced.",
            "Route, native execution and gameplay evidence are reported separately; this encoder does not establish playability.",
        ],
    }
    return payloads, manifest
