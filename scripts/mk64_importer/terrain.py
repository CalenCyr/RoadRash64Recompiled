"""Prepare and encode the accepted finite course terrain from a user's ROM.

Rendering variants, physical contact and exploration are separate stages. No
route corridor or world-sized floor is generated. The final render triangles
are the only authority for additional explorable ground.
"""

from collections import defaultdict
from fractions import Fraction as F
from pathlib import Path
import copy, json

from race_pack_camera_variants import (
    prefer_exact_camera_detail,
    preserve_native_collision_order,
    conform_quantized_camera_edge,
)
from race_pack_ground_detail import (
    area,
    attributes,
    cross,
    scalar,
    side,
    subtract,
    subtract_union,
    prefer_camera_ground_detail,
)
from race_pack_low_rails import lower_selected_rails, lower_attached_rail_caps
from race_pack_dk_royal import correct_dk_royal
from race_pack_yoshi_valley import correct_yoshi_valley
from race_pack_koopa_beach import correct_koopa_beach, preserve_koopa_collision_order
from race_pack_luigi_raceway import correct_luigi_raceway
from race_pack_terrain_variants import prefer_authored_terrain_detail
from race_pack_visible_terrain import (
    promote_encoded_visible_terrain,
    promote_visible_terrain,
    preserve_existing_contacts_and_split_added,
)

from . import cells, geometry, materials
from .baseline import baseline, _digest
from .collision import filter_course_collision, filter_pack_collision
from .source_rom import require
from .terrain_contacts import authored_surfaces, derive, preserve_floor_order

POLICY = json.loads(Path(__file__).with_name("terrain_policy.json").read_text())


def _moo_ground(source):
    """Preserve Moo's verified original-union projection and TMEM selection."""
    result = copy.deepcopy(source)
    paired = POLICY["moo_pairs"]
    leaves = defaultdict(list)
    for index, triangle in enumerate(source["triangles"]):
        leaves[triangle["display_list"]].append((index, triangle))
    output, cache, changes, removed = [], {}, [], []
    for index, triangle in enumerate(source["triangles"]):
        material = source["materials"][triangle["material"]]
        leaf = triangle["display_list"]
        if leaf in (
            "d_course_moo_moo_farm_packed_dl_6358",
            "d_course_moo_moo_farm_packed_dl_6408",
        ) and "gMMFTextureSignNintendo1__view_" in str(material["texture"]):
            removed.append(index)
            continue
        if leaf not in paired:
            output.append(triangle)
            continue
        vertices = [source["vertices"][v] for v in triangle["vertices"]]
        xyz = [v["position"] for v in vertices]
        normal = cross(
            [xyz[1][k] - xyz[0][k] for k in range(3)], [xyz[2][k] - xyz[0][k] for k in range(3)]
        )
        require(
            normal[1] > 0 and normal[1] >= max(abs(normal[0]), abs(normal[2])),
            "Moo alternative is not ground",
        )
        polygon = [(F(v[0]), F(v[2])) for v in xyz]
        clips, details = [], []
        for detail in sorted(paired[leaf]):
            for other_index, other in leaves[detail]:
                if source["materials"][other["material"]]["texture"] != material["texture"]:
                    continue
                points = [source["vertices"][v]["position"] for v in other["vertices"]]
                clip = [(F(v[0]), F(v[2])) for v in points]
                if not area(clip) or area(polygon) == sum(map(area, subtract(polygon, clip)), F(0)):
                    continue
                n = cross(
                    [points[1][k] - points[0][k] for k in range(3)],
                    [points[2][k] - points[0][k] for k in range(3)],
                )
                require(
                    n[1] > 0 and n[1] >= max(abs(n[0]), abs(n[2])),
                    "Moo detailed alternative is not ground",
                )
                clips.append(clip)
                details.append(other_index)
        if not clips:
            output.append(triangle)
            continue
        residual = subtract_union(polygon, clips)
        start = len(output)
        if sum(map(area, residual), F(0)):
            attrs = [attributes(v) for v in vertices]
            denominator = side(*polygon)
            for piece in residual:
                for k in range(1, len(piece) - 1):
                    points = [piece[0], piece[k], piece[k + 1]]
                    if not area(points):
                        continue
                    indices = []
                    for point in points:
                        weights = [
                            side(polygon[1], polygon[2], point) / denominator,
                            side(polygon[2], polygon[0], point) / denominator,
                            side(polygon[0], polygon[1], point) / denominator,
                        ]
                        require(
                            sum(weights) == 1 and all(v >= 0 for v in weights),
                            "Moo interpolation leaves source face",
                        )
                        values = tuple(
                            sum(weights[j] * attrs[j][c] for j in range(3)) for c in range(9)
                        )
                        if values not in cache:
                            cache[values] = len(result["vertices"])
                            values_out = list(map(scalar, values))
                            result["vertices"].append(
                                {
                                    "position": values_out[:3],
                                    "texcoord_st": values_out[3:5],
                                    "color_rgba": values_out[5:],
                                }
                            )
                        indices.append(cache[values])
                    output.append(
                        {**triangle, "vertices": indices, "render_recovery_source_triangle": index}
                    )
        changes.append(
            {
                "triangle": index,
                "detail_triangles": details,
                "residual_triangles": len(output) - start,
            }
        )
    require(len(removed) == 49, "Moo sign TMEM variants changed")
    result["triangles"] = output
    result["validation"].update(render_triangles=len(output), vertices=len(result["vertices"]))
    result["render_recovery_policy"] = {
        "mode": "moo-source-camera-alternative-ground-projection",
        "original_source_sha256": _digest(source),
        "collision_unchanged": True,
        "exact_projected_residuals_preserved": True,
        "removed_sign_tmem_variants": removed,
    }
    return result, {"changes": changes, "removed_sign_views": removed}


def encode_source(source, texture_dir, registry, placement, scale=0.234375, *, deduplicate=True):
    """Encode the supplied source stage; historical route probes may call this.

    No collision filter or later correction is implicit. The caller chooses its
    historical source stage and placement. Return geometry, cell-name -> bytes.
    Registry changes are retained so the pack assembler owns texture identities.
    """
    adapted, _, _ = materials.adapt(source, Path(texture_dir), registry)
    compiled = geometry.compile_geometry(adapted, scale)
    placement = dict(placement)
    if "expected_occupied_cells" not in placement:
        dx, dy, _ = placement["terrain_translation"]
        placement["expected_occupied_cells"] = [
            c["index"] + dx // 1000 * 70 + dy // 1000 for c in compiled["cells"]
        ]
    materials.translate(compiled, placement)
    if deduplicate:
        texture_ids = {t["symbol"]: t["native_index"] for t in compiled["textures"]}
        for cell in compiled["cells"]:
            seen, kept = set(), []
            for triangle in cell["triangles"]:
                duplicate = False
                if triangle["render"]:
                    material = compiled["materials"][triangle["material"]]
                    if (
                        material["render"] == ["G_RM_AA_ZB_OPA_SURF", "G_RM_AA_ZB_OPA_SURF2"]
                        and not material["native_features"] & 0x90
                    ):
                        texture = (
                            texture_ids[material["texture"]]
                            if material["texture_enabled"]
                            else 65535
                        )
                        key = tuple(tuple(v) for v in triangle["vertices"]), texture
                        if key in seen:
                            duplicate = True
                            triangle["render"] = False
                        else:
                            seen.add(key)
                if not duplicate or triangle["collision"]:
                    kept.append(triangle)
            cell["triangles"] = kept
    return compiled, cells.build(compiled)[0]


def prepare_source(source, roots, display_lists, texture_dir, transform):
    """Return final source, pre-camera source, authored contacts and audit."""
    slug = source["course"]
    prepared, baseline_report = baseline(source, roots, display_lists)
    report = {"baseline": baseline_report, "stages": []}
    if slug == "moo_moo_farm":
        # Moo's accepted repair starts from the original union, not already
        # clipped baseline fragments. This distinction preserves exact seams.
        prepared, info = _moo_ground(source)
        report["stages"].append({"moo": info})
    if slug == "frappe_snowland":
        accepted, _ = filter_course_collision(prepared)
        pairs = {k: set(v) for k, v in POLICY["frappe_pairs"].items()}
        prepared, first = prefer_camera_ground_detail(
            prepared, pairs, roots, accepted["collision_triangles"]
        )
        prepared, second = prefer_camera_ground_detail(
            prepared,
            pairs,
            roots,
            accepted["collision_triangles"],
            include_sloping_sides=True,
            allow_adjacent_close_ground=True,
            nonphysical_replacements={
                "d_course_frappe_snowland_packed_dl_1BE8": {
                    "d_course_frappe_snowland_packed_dl_4700"
                }
            },
        )
        report["stages"].append({"frappe_first": first, "frappe_complete": second})
    initial, rejected = filter_course_collision(prepared)
    contacts = authored_surfaces(source, {"transform": transform}, _digest(source))
    contacts["walls"]["rails"], rail_stats = derive(
        contacts["walls"]["triangles"], transform["scale"]
    )
    walls, surfaces = [filter_pack_collision(contacts[k], rejected) for k in ("walls", "surfaces")]
    reference = copy.deepcopy(initial)
    texture_dir = Path(texture_dir)
    textures = {t["symbol"]: t for t in prepared["textures"]}
    if slug in ("dks_jungle_parkway", "royal_raceway"):
        prepared, info = correct_dk_royal(
            prepared,
            roots,
            initial["collision_triangles"],
            lambda name: (texture_dir / textures[name]["file"]).read_bytes(),
        )
        report["stages"].append(info)
    elif slug == "yoshi_valley":
        prepared, walls, surfaces, info = correct_yoshi_valley(
            prepared, roots, initial["collision_triangles"], walls, surfaces, transform
        )
        report["stages"].append(info)
    elif slug in ("rainbow_road", "banshee_boardwalk"):
        lookup = {t["id"]: t for t in walls["triangles"]}
        selected = {
            r["id"]
            for r in walls["rails"]
            if slug == "rainbow_road"
            or all(lookup[i]["source"]["surface"] == "BRIDGE" for i in r["triangle_ids"])
        }
        require(
            len(selected) == (518 if slug == "rainbow_road" else 113),
            "Rail panel selection changed",
        )
        lowered, walls, surfaces, info = lower_selected_rails(
            prepared, walls, surfaces, transform, selected, 1.25
        )
        reference, _ = filter_course_collision(lowered)
        if slug == "banshee_boardwalk":
            prepared, caps = lower_attached_rail_caps(prepared, lowered)
            require(caps["cap_triangles"] == 226, "Banshee attached rail caps changed")
            report["stages"].append({"rails": info, "caps": caps})
        else:
            prepared = lowered
            report["stages"].append({"rails": info})
    elif slug == "wario_stadium":
        prepared, info = prefer_exact_camera_detail(
            prepared,
            roots,
            [],
            render_alternative_pairs=POLICY["wario_pairs"],
            render_alternative_textures={"gWSTexture670AC8"},
        )
        report["stages"].append({"wario_crowds": info})
    else:
        prepared, info = prefer_exact_camera_detail(prepared, roots, initial["collision_triangles"])
        report["stages"].append({"exact_camera": info})
        if slug == "luigi_raceway":
            prepared, info = prefer_exact_camera_detail(
                prepared,
                roots,
                initial["collision_triangles"],
                render_alternative_pairs=[
                    (
                        "d_course_luigi_raceway_packed_dl_9ED0",
                        "d_course_luigi_raceway_packed_dl_8240",
                    )
                ],
            )
            report["stages"].append({"luigi_scenery": info})
            prepared, info = correct_luigi_raceway(
                prepared, roots, initial["collision_triangles"]
            )
            report["stages"].append({"luigi_tunnel": info})
        elif slug == "koopa_troopa_beach":
            # Whole authored near/far shells are not always coplanar. Select
            # their detailed form before promoting visible terrain to contact.
            prepared, info = correct_koopa_beach(
                prepared, roots, initial["collision_triangles"]
            )
            report["stages"].append({"koopa_terrain": info})
        elif slug == "kalimari_desert":
            prepared, info = prefer_authored_terrain_detail(
                prepared,
                POLICY["kalimari_pairs"],
                roots,
                initial["collision_triangles"],
                compatible_textures=POLICY["kalimari_compatible_textures"],
            )
            report["stages"].append({"kalimari_ground": info})
    final, rejected_after = filter_course_collision(prepared)
    require(rejected == rejected_after, "Terrain correction changed original collision acceptance")
    report.update(rail_classification=rail_stats, rejected_original_contact_ids=sorted(rejected))
    return final, reference, walls, surfaces, report


def _exploration_metadata(walls, surfaces, faces, *, first_id=40000):
    """Append finite final triangles, preserving original authored identities."""
    walls, surfaces = copy.deepcopy(walls), copy.deepcopy(surfaces)
    first_id = max(first_id, 1 + max((t["id"] for t in surfaces["triangles"]), default=-1))
    counts = [0, 0]
    for index, (key, face) in enumerate(sorted(faces.items())):
        points = [[p[0] / 4, p[1] / 4, p[2] / 8] for p in (key[0], key[2], key[1])]
        row = {
            "id": first_id + index,
            "vertices": points,
            "source": {
                "adaptation": "visible-terrain-exploration",
                "encoded_cell": face["cell"],
                "texture": face.get("texture"),
                "encoded_vertices": key,
                "surface": "VISIBLE_TERRAIN",
                "section": 255,
                "flags": 0,
            },
        }
        surfaces["triangles"].append(row)
        counts[1] += 1
        a, b, c = points
        normal = cross([b[k] - a[k] for k in range(3)], [c[k] - a[k] for k in range(3)])
        if (
            abs(normal[2])
            <= max(abs(normal[0]), abs(normal[1])) + 1e-4 * sum(v * v for v in normal) ** 0.5
        ):
            walls["triangles"].append(row)
            counts[0] += 1
    for data, count in zip((walls, surfaces), counts):
        if count:
            data["exploration_adaptation"] = {
                "mode": "finite-final-visible-triangle-contact",
                "added": count,
                "no_world_plane": True,
                "after_terrain_variant_selection": True,
            }
    return walls, surfaces


def _wario_quantized_seam(compiled):
    """Use converted source identities, never packaged donor coordinates."""
    recipe = POLICY["wario_quantized_seam"]
    coarse = [
        (cell, t)
        for cell in compiled["cells"]
        for t in cell["triangles"]
        if t["render"]
        and t["material"] == recipe["material"]
        and t["source_triangle"] == recipe["coarse_triangle"]
    ]
    require(len(coarse) == 1, "Wario coarse residual identity changed")
    cell, triangle = coarse[0]
    detail = [
        t
        for t in cell["triangles"]
        if t["render"]
        and t["material"] == recipe["material"]
        and t["source_triangle"] == recipe["detail_triangle"]
    ]
    require(len(detail) == 1, "Wario retained detail identity changed")
    point = detail[0]["vertices"][recipe["detail_vertex"]][:3]
    return conform_quantized_camera_edge(
        compiled,
        material=recipe["material"],
        coarse_vertices=[v[:3] for v in triangle["vertices"]],
        detail_vertex=point,
    )


def build_course(source, roots, display_lists, texture_dir, registry, placement):
    """Return final geometry, cell payloads, walls, surfaces, and conversion audit.

    ``placement`` supplies terrain_translation and optional expected cells. The
    registry is the pack assembler's authenticated stable material registry.
    Nonterrain race/boost metadata is applied separately by the pack assembler.
    """
    # The transform depends only on original bounds and the selected atlas.
    bounds = source["validation"]
    center = [(bounds["bounds_min"][i] + bounds["bounds_max"][i]) / 2 for i in (0, 2)]
    offset = placement["terrain_translation"]
    transform = {
        "scale": 0.234375,
        "source_center_xz": center,
        "atlas_translation_world": [offset[0] / 4, offset[1] / 4, 0],
    }
    final, reference, walls, surfaces, report = prepare_source(
        source, roots, display_lists, texture_dir, transform
    )
    compiled, encoded = encode_source(final, texture_dir, registry, placement)
    _, originals = encode_source(reference, texture_dir, registry, placement)
    slug = source["course"]
    if slug == "wario_stadium":
        compiled, seam = _wario_quantized_seam(compiled)
        encoded = cells.build(compiled)[0]
        report["quantized_seam"] = seam
    if slug == "frappe_snowland":
        # Keep the earlier finite-snow contact pass before the common encoded
        # promotion. Some independently clipped faces quantize to one face;
        # reproducing this order preserves the proven native lookup records.
        shade = [i for i, m in enumerate(final["materials"]) if not m["texture_enabled"]]
        require(shade == [14], "Frappe shade material identity changed")
        river = [
            t
            for t in final["triangles"]
            if t["display_list"] == "d_course_frappe_snowland_packed_dl_4700"
        ]
        require(
            river
            and all(
                final["materials"][t["material"]]["texture"] == "gFSTexture675434" for t in river
            ),
            "Frappe river classification changed",
        )
        expanded, expansion = promote_visible_terrain(
            final, excluded_textures={"gFSTexture675434"}, excluded_materials=shade
        )
        compiled, expanded_cells = encode_source(expanded, texture_dir, registry, placement)
        encoded = {
            name: preserve_existing_contacts_and_split_added(originals[name], data)
            for name, data in expanded_cells.items()
        }
        report["source_exploration"] = expansion
    eligible = {
        row["native_index"] for row in POLICY["exploration_materials"][slug] if row["eligible"]
    }
    original_ids = {t["native_index"]: t["source_symbol"] for t in compiled["textures"]}
    for row in POLICY["exploration_materials"][slug]:
        require(
            original_ids.get(row["native_index"]) == row["source_symbol"],
            "Exploration material registry identity changed",
        )
    faces, payloads = {}, {}
    by_name = {f"cell-{c['index']:04d}.bin": c["index"] for c in compiled["cells"]}
    for name, data in encoded.items():
        old = originals[name]
        if slug == "frappe_snowland":
            render, current = cells.decoded_payloads(data)
            _, prior = cells.decoded_payloads(old)
            visible = {
                min(tuple(tuple(v[:3]) for v in points[i:] + points[:i]) for i in range(3)): texture
                for points, texture in render
            }
            for _, points, _ in current - prior:
                positions = [tuple(v[:3]) for v in points]
                key = min(tuple(positions[i:] + positions[:i]) for i in range(3))
                require(key in visible, "Exploration contact is not final visible terrain")
                faces.setdefault(key, {"cell": by_name[name], "texture": visible[key]})
        else:
            if slug == "yoshi_valley":
                data = preserve_floor_order(old, data)
            elif slug == "koopa_troopa_beach":
                data = preserve_koopa_collision_order(old, data)
            else:
                data = preserve_native_collision_order(old, data)
        data, promoted = promote_encoded_visible_terrain(data, eligible_textures=eligible)
        for face in promoted["added"]:
            points = face["vertices"]
            key = min(tuple(points[i:] + points[:i]) for i in range(3))
            faces.setdefault(key, {**face, "cell": by_name[name]})
        payloads[name] = data
    walls, surfaces = _exploration_metadata(
        walls, surfaces, faces, first_id=30000 if slug == "frappe_snowland" else 40000
    )
    report.update(source=final, finite_exploration_faces=len(faces), cell_count=len(payloads))
    return compiled, payloads, walls, surfaces, report
