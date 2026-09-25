"""Convert exported static courses into a disjoint native atlas; no stock export."""

from __future__ import annotations
import copy
from . import native_texture as texture
from .common import digest, orientation_key, require


def material_variant(course, m, source_dir, records):
    if not m["texture_enabled"]:
        return None, None
    t = next(t for t in course["textures"] if t["symbol"] == m["texture"])
    pixels = (source_dir / t["file"]).read_bytes()
    require(digest(pixels) == t["sha256"], "Source pixels changed")
    flags = texture.material_features(m, t)
    translucent = any("XLU" in mode for mode in m["render"])
    flags |= 128 if translucent else 0
    cutout = any("TEX_EDGE" in mode for mode in m["render"])
    blob = texture.encode_rgba16(pixels, t["width"], t["height"], cutout=cutout, features=flags)
    key = digest(blob)
    if key not in records:
        index = 926 + len(records)
        require(index < 1280, "Combined texture dependency bitset exhausted")
        records[key] = {
            "index": index,
            "blob": blob,
            "features": flags,
            "format": t["format"],
            "width": t["width"],
            "height": t["height"],
            "cutout": cutout,
        }
    r = records[key]
    if r.get("blob") is None:
        # Address-free digest recipes retain established texture identities;
        # all texels and native records still come from the supplied ROM.
        r["blob"] = blob
    return {
        **t,
        "symbol": f"native_texture_{r['index']}",
        "source_symbol": t["symbol"],
        "native_index": r["index"],
        "features": flags,
        "native_record_sha256": key,
    }, r


def adapt(course, directory, records):
    output = copy.deepcopy(course)
    materials = []
    material_map = {}
    material_keys = {}
    textures = {}
    coverage = []
    for m in course["materials"]:
        t, r = material_variant(course, m, directory, records)
        tile = m["tile_size"]
        uv = [int(tile[1], 0) * 8, int(tile[2], 0) * 8] if m["texture_enabled"] else [0, 0]
        key = (r["index"] if r else 65535, tuple(uv))
        if key not in material_keys:
            material_keys[key] = len(materials)
            materials.append(
                {
                    **m,
                    "id": len(materials),
                    "texture": t["symbol"] if t else None,
                    "native_features": r["features"] if r else 0,
                    "native_uv_offset_st": uv,
                    "source_material_ids": [],
                }
            )
        mid = material_keys[key]
        materials[mid]["source_material_ids"].append(m["id"])
        material_map[m["id"]] = mid
        if t:
            textures[t["symbol"]] = t
        coverage.append(
            {
                "source_material": m["id"],
                "native_material": mid,
                "texture_index": r["index"] if r else None,
                "features": r["features"] if r else 0,
                "original_render_modes": m["render"],
                "original_combine_modes": m["combine"],
                "uv_offset_st": uv,
                "fog_not_imported": "G_FOG" in m["geometry_modes"],
                "translucent_native_standard_mode": bool(r and r["features"] & 128),
            }
        )
    output["materials"] = materials
    output["textures"] = list(textures.values())
    triangles = []
    seen = {}
    for triangle in course["triangles"]:
        mid = material_map[triangle["material"]]
        m = materials[mid]
        t = textures.get(m["texture"])
        # A static union of camera sections can observe the identical oriented
        # primitive with culling both enabled and disabled. The disabled form
        # already covers both: retaining both adds a coplanar duplicate. Never
        # merge differing texels, UV origin, alpha, sampler or winding.
        identity = (
            (
                t["sha256"],
                t["width"],
                t["height"],
                t["features"] & ~16,
                any("TEX_EDGE" in r for r in m["render"]),
            )
            if t
            else ("untextured",)
        )
        key = (orientation_key(triangle["vertices"]), identity, tuple(m["native_uv_offset_st"]))
        if key not in seen:
            seen[key] = len(triangles)
            triangles.append({**triangle, "material": mid})
        elif m["native_features"] & 16:
            triangles[seen[key]]["material"] = mid
    # Opaque first, alpha passes second. Within each pass preserve the source
    # union's first-observation order; original camera-dependent global ordering
    # has no single order in a static union and remains an explicit limitation.
    triangles.sort(key=lambda t: bool(materials[t["material"]]["native_features"] & 128))
    output["triangles"] = triangles
    return output, coverage, len(course["triangles"]) - len(triangles)


def translate(g, placement):
    dx, dy, dz = placement["terrain_translation"]
    require(dx % 1000 == dy % 1000 == 0 and dz == 0, "Non-grid atlas placement")
    expected = set(placement["expected_occupied_cells"])
    for cell in g["cells"]:
        cell["index"] += (dx // 1000) * 70 + dy // 1000
        require(cell["index"] in expected, "Course does not fit reserved interior")
        for t in cell["triangles"]:
            t["vertices"] = [[v[0] + dx, v[1] + dy, *v[2:]] for v in t["vertices"]]
            for v in t["vertices"]:
                require(-32768 <= v[0] <= 32767 and -32768 <= v[1] <= 32767, "Atlas S16 overflow")
    for p in g["path_world"]:
        p[0] += dx / 4
        p[1] += dy / 4
    g["transform"]["atlas_translation_raw"] = [dx, dy, 0]
    g["transform"]["atlas_translation_world"] = [dx / 4, dy / 4, 0]
    g["transform"]["world"] += " + atlas_translation_world"
