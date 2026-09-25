"""Reproduce the common course preparation using the user's decoded ROM.

This is the input stage for the later course-specific terrain recipes. It
selects Mario's authenticated detailed camera alternatives, resolves exact
coplanar camera conflicts, and applies the game's racing vertex initialization.
Collision indices and original vertices retain their identities. All geometry,
display-list commands, and image bytes come from the supplied cartridge image;
the adjacent policy file contains only conversion rules and initialization calls.
"""

from __future__ import annotations

from collections import Counter, defaultdict
import copy
from fractions import Fraction as F
from functools import reduce
import hashlib
import json
from math import gcd
from pathlib import Path

from race_pack_ground_detail import (
    area,
    attributes,
    cross,
    scalar,
    side,
    signed,
    subtract,
    subtract_union,
)
from .source_rom import require

POLICY = json.loads(Path(__file__).with_name("baseline_policy.json").read_text())


def _digest(source):
    """Identify the current decoded stage, not a historical developer file."""
    data = json.dumps(source, separators=(",", ":")).encode() + b"\n"
    return hashlib.sha256(data).hexdigest()


def _mario_variants(source, roots):
    result = copy.deepcopy(source)
    if source["course"] != "mario_raceway":
        return result, []
    prefix = "d_course_mario_raceway_packed_dl_"
    collision = Counter(t["display_list"] for t in source["collision_triangles"])
    render = Counter(t["display_list"] for t in source["triangles"])
    evidence = []
    for coarse, detail in POLICY["mario_detailed_leaf_pairs"].items():
        coarse, detail = prefix + coarse, prefix + detail
        require(
            render[coarse] and render[detail] and collision[detail] and not collision[coarse],
            "Mario terrain alternative is not authenticated",
        )
        require(
            roots.get(coarse) and roots.get(detail) and not roots[coarse] & roots[detail],
            "Mario terrain alternatives are not mutually exclusive",
        )
        evidence.append(
            {
                "coarse": coarse,
                "detailed": detail,
                "coarse_triangles": render[coarse],
                "detailed_triangles": render[detail],
                "detailed_collision_triangles": collision[detail],
                "cooccurring_roots": 0,
            }
        )
    excluded = {row["coarse"] for row in evidence}
    result["triangles"] = [t for t in result["triangles"] if t["display_list"] not in excluded]
    removed = len(source["triangles"]) - len(result["triangles"])
    result["validation"].update(
        render_triangles=len(result["triangles"]),
        coarse_terrain_variant_triangles_excluded=removed,
        coarse_terrain_variant_leaf_lists_excluded=len(excluded),
    )
    result["render_variant_policy"] = {
        "mode": "always-detailed-source-terrain",
        "excluded_leaf_lists": sorted(excluded),
        "reason": "Camera-dependent coarse and detailed source terrain variants must not render simultaneously",
        "source_course_sha256": _digest(source),
    }
    result["limitations"].append(
        "Mario camera-dependent terrain LODs use the detailed collision-backed source meshes at every distance; no geometric remeshing."
    )
    return result, evidence


def _eligible_conflicts(source, roots):
    """Find exact positive-area conflicts between mutually exclusive leaves.

    Original vertices are integral. Canonical integer plane keys prevent nearby
    roads, real stacked decks, and slope changes from being treated as overlap.
    Root membership and collision ownership distinguish camera alternatives.
    """
    groups = defaultdict(list)
    collision = Counter(t["display_list"] for t in source["collision_triangles"])
    for index, triangle in enumerate(source["triangles"]):
        material = source["materials"][triangle["material"]]
        leaf = triangle["display_list"]
        if (
            not roots.get(leaf)
            or not material["texture_enabled"]
            or any("XLU" in mode or "TEX_EDGE" in mode for mode in material["render"])
        ):
            continue
        xyz = [source["vertices"][v]["position"] for v in triangle["vertices"]]
        require(
            all(isinstance(value, int) for p in xyz for value in p),
            "Baseline preparation requires original integral vertices",
        )
        normal = cross(
            [xyz[1][k] - xyz[0][k] for k in range(3)], [xyz[2][k] - xyz[0][k] for k in range(3)]
        )
        if not any(normal):
            continue
        plane = (*normal, -sum(normal[k] * xyz[0][k] for k in range(3)))
        divisor = reduce(gcd, map(abs, plane))
        plane = tuple(value // divisor for value in plane)
        if next(value for value in plane if value) < 0:
            plane = tuple(-value for value in plane)
        axis = max(range(3), key=lambda k: abs(normal[k]))
        axes = [k for k in range(3) if k != axis]
        polygon = [tuple(F(p[k]) for k in axes) for p in xyz]
        bounds = tuple((min(p[k] for p in polygon), max(p[k] for p in polygon)) for k in range(2))
        groups[plane, material["texture"]].append((index, leaf, polygon, bounds))
    eligible = defaultdict(set)
    for entries in groups.values():
        physical = [entry for entry in entries if collision[entry[1]]]
        visual = [entry for entry in entries if not collision[entry[1]]]
        for index, leaf, polygon, bounds in visual:
            for detail, detail_leaf, clip, clip_bounds in physical:
                if roots[leaf] & roots[detail_leaf]:
                    continue
                if any(
                    min(bounds[k][1], clip_bounds[k][1]) <= max(bounds[k][0], clip_bounds[k][0])
                    for k in range(2)
                ):
                    continue
                if sum(map(area, subtract(polygon, clip)), F(0)) < area(polygon):
                    eligible[index].add(detail)
    return eligible


def _recover_terrain(source, roots):
    eligible = _eligible_conflicts(source, roots)
    result = copy.deepcopy(source)
    if not eligible:
        return result, {"changes": [], "counts": {}}
    output, changes, cache = [], [], {}
    counts = Counter()
    for index, triangle in enumerate(source["triangles"]):
        if index not in eligible:
            output.append(triangle)
            continue
        vertices = [source["vertices"][v] for v in triangle["vertices"]]
        xyz = [v["position"] for v in vertices]
        normal = cross(
            [xyz[1][k] - xyz[0][k] for k in range(3)], [xyz[2][k] - xyz[0][k] for k in range(3)]
        )
        axis = max(range(3), key=lambda k: abs(normal[k]))
        axes = [k for k in range(3) if k != axis]
        polygon = [tuple(F(p[k]) for k in axes) for p in xyz]
        details = sorted(eligible[index])
        clips = [
            [
                tuple(F(source["vertices"][v]["position"][k]) for k in axes)
                for v in source["triangles"][detail]["vertices"]
            ]
            for detail in details
        ]
        residual = subtract_union(polygon, clips)
        source_area, residual_area = area(polygon), sum(map(area, residual), F(0))
        require(0 <= residual_area < source_area, "Invalid exact terrain residual area")
        counts["eligible_triangles"] += 1
        row = {
            "triangle": index,
            "source_list": triangle["display_list"],
            "detail_triangles": details,
            "projected_source_area": str(source_area),
            "projected_residual_area": str(residual_area),
            "covered_fraction": str((source_area - residual_area) / source_area),
            "residual_polygons": len(residual),
        }
        if not residual_area:
            counts["fully_covered_triangles_removed"] += 1
            row["action"] = "remove"
        else:
            counts["partially_covered_triangles_split"] += 1
            row["action"] = "split"
            start = len(output)
            denominator = side(polygon[0], polygon[1], polygon[2])
            attrs = [attributes(v) for v in vertices]
            for piece in residual:
                require((signed(piece) > 0) == (signed(polygon) > 0), "Terrain winding changed")
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
                            sum(weights) == 1 and all(0 <= v <= 1 for v in weights),
                            "Terrain interpolation leaves source face",
                        )
                        values = tuple(
                            sum(weights[j] * attrs[j][c] for j in range(3)) for c in range(9)
                        )
                        require(
                            tuple(values[k] for k in axes) == point,
                            "Terrain position interpolation changed",
                        )
                        if values not in cache:
                            cache[values] = len(result["vertices"])
                            values_out = [scalar(value) for value in values]
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
            row["residual_output_triangles"] = len(output) - start
            counts["residual_triangles_emitted"] += len(output) - start
        changes.append(row)
    result["triangles"] = output
    result["validation"].update(render_triangles=len(output), vertices=len(result["vertices"]))
    result["render_recovery_policy"] = {
        "mode": "exact-source-coplanar-variant-subtraction",
        "source_sha256": _digest(source),
        "rational_precision_tolerance": 0,
        "partial_residuals_preserved": True,
        "split_residuals": True,
        "collision_and_original_vertices_unchanged": True,
    }
    return result, {"changes": changes, "counts": dict(counts)}


def _initialize_vertices(source, display_lists):
    result = copy.deepcopy(source)
    operations = []

    def ranges(name, stack=()):
        require(
            name not in stack and len(stack) < 32,
            "Invalid vertex initialization display-list recursion",
        )
        for command, args in display_lists[name]:
            if command == "gsSPDisplayList":
                yield from ranges(args[0], (*stack, name))
            elif command == "gsSPVertex":
                address, count = int(args[0], 0), int(args[1], 0)
                require(
                    address >> 24 == 4 and address % 16 == 0 and 0 < count <= 32,
                    "Invalid initialization vertex range",
                )
                first = (address & 0xFFFFFF) // 16
                require(
                    first + count <= len(source["vertices"]),
                    "Initialization vertex range exceeds course",
                )
                yield first, count

    for call in POLICY["gameplay_vertex_initialization"].get(source["course"], []):
        rgba = call["rgba"]
        selected_ranges = list(ranges(call["root"]))
        require(selected_ranges, "Initialization display list does not load vertices")
        changed = set()
        for first, count in selected_ranges:
            for index in range(first, first + count):
                # The original helper reserves red==0 for alpha-only writes.
                if rgba[0]:
                    result["vertices"][index]["color_rgba"][:3] = rgba[:3]
                result["vertices"][index]["color_rgba"][3] = rgba[3] & 255
                changed.add(index)
        operations.append(
            {
                "root": call["root"],
                "rgba": list(rgba),
                "ranges": selected_ranges,
                "vertices": sorted(changed),
            }
        )
    if operations:
        result["runtime_vertex_initialization"] = {
            "source": "src/racing/render_courses.c",
            "function": "find_vtx_and_set_colours",
            "gameplay_not_credits": True,
            "operations": operations,
        }
    return result, operations


def baseline(source, roots, display_lists):
    """Return ``(prepared_course, report)`` without modifying caller inputs.

    ``source`` is original neutral visual/collision extraction, with unfiltered
    donor collision indices. ``roots`` maps leaf names to sets of actual donor
    section/direction roots. ``display_lists`` is the ROM decoder's lazy command
    mapping. Later recipes and encoding are deliberately separate.
    """
    require(
        "render_recovery_policy" not in source
        and "runtime_vertex_initialization" not in source
        and "render_variant_policy" not in source,
        "Baseline preparation was already applied",
    )
    detailed, mario = _mario_variants(source, roots)
    recovered, recovery = _recover_terrain(detailed, roots)
    prepared, operations = _initialize_vertices(recovered, display_lists)
    changed = {
        i for i, (a, b) in enumerate(zip(recovered["vertices"], prepared["vertices"])) if a != b
    }
    for triangle in recovered["triangles"]:
        if "render_recovery_source_triangle" in triangle:
            original = detailed["triangles"][triangle["render_recovery_source_triangle"]]
            require(
                not changed.intersection(original["vertices"]),
                "Initialized colors require rebuilding terrain residual attributes",
            )
    for name in (
        "materials",
        "textures",
        "collision_triangles",
        "collision_roots",
        "path",
        "paths",
    ):
        require(prepared[name] == source[name], "Baseline unexpectedly changed " + name)
    return prepared, {
        "course": source["course"],
        "mario_alternatives": mario,
        "terrain_recovery": recovery,
        "initialization_operations": operations,
        "initialized_vertices": len(changed),
    }
