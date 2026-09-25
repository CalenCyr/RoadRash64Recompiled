"""Convert the private neutral course export to Road Rash terrain coordinates.

This is deliberately an offline converter. It does not modify a ROM or enable a
game hook. Clipping keeps rendering and collision on the same transformed mesh.
Source surface names survive conversion; native physics mapping is a later,
explicit step rather than an assumption hidden in the coordinate conversion.
"""

from __future__ import annotations

from collections import Counter, defaultdict
import math

CELL_SIZE = 1000
GRID_HALF = 35000
GRID_WIDTH = 70


def require(condition, message):
    if not condition:
        raise ValueError(message)


def canonical_triangle(indices):
    # Preserve orientation; an opposite-facing triangle is not a duplicate.
    values = tuple(indices)
    return min(values[i:] + values[:i] for i in range(3))


def cross(a, b, c):
    u = [b[i] - a[i] for i in range(3)]
    v = [c[i] - a[i] for i in range(3)]
    return (u[1] * v[2] - u[2] * v[1], u[2] * v[0] - u[0] * v[2], u[0] * v[1] - u[1] * v[0])


def squared_area(vertices):
    return sum(n * n for n in cross(*[v[:3] for v in vertices]))


def clip_polygon(vertices, axis, boundary, greater):
    if not vertices:
        return []
    out = []
    previous = vertices[-1]
    prev_inside = previous[axis] >= boundary if greater else previous[axis] <= boundary
    for current in vertices:
        inside = current[axis] >= boundary if greater else current[axis] <= boundary
        if prev_inside != inside:
            # Shared edges can be traversed in either direction by adjacent
            # triangles. Use one arithmetic order to keep UV rounding equal.
            begin, end = sorted((previous, current))
            fraction = (boundary - begin[axis]) / (end[axis] - begin[axis])
            intersection = [a + fraction * (b - a) for a, b in zip(begin, end)]
            intersection[axis] = boundary
            out.append(intersection)
        if inside:
            out.append(current)
        previous, prev_inside = current, inside
    return out


def round_signed(value):
    # Match a conventional signed fixed-point conversion, including ties.
    return math.floor(value + 0.5) if value >= 0 else math.ceil(value - 0.5)


def quantize(vertex):
    values = [round_signed(value) for value in vertex]
    for i in range(5):
        require(-32768 <= values[i] <= 32767, "terrain vertex coordinate/UV exceeds s16")
    for i in range(5, 9):
        values[i] = max(0, min(255, values[i]))
    return values


def transform(position, center, scale):
    # MK64 Y-up -> RR64 Z-up. The negative sign preserves handedness, unlike
    # swapping Y/Z alone. 41090 queries terrain at four times bike-world X/Y,
    # then quarters query height. 14A10 halves raw vertex Z before that.
    x, y, z = position
    return [(x - center[0]) * scale * 4, -(z - center[1]) * scale * 4, y * scale * 8]


def validate_source(course):
    require(
        course.get("format") == "rr64-mk64-course" and course.get("version") == 1,
        "unsupported intermediate format",
    )
    vertices, materials = course["vertices"], course["materials"]
    require(
        0 < len(vertices) < 100000 and 0 < len(materials) < 1000,
        "intermediate vertex/material count outside converter bounds",
    )
    for vertex in vertices:
        for key, count in (("position", 3), ("texcoord_st", 2), ("color_rgba", 4)):
            values = vertex[key]
            require(
                len(values) == count
                and all(type(x) in (int, float) and math.isfinite(x) for x in values),
                "invalid vertex attributes",
            )
        require(all(0 <= x <= 255 for x in vertex["color_rgba"]), "invalid source vertex color")
    for material_id, material in enumerate(materials):
        require(material["id"] == material_id, "nonsequential material IDs")
    for label in ("triangles", "collision_triangles"):
        for triangle in course[label]:
            indices = triangle["vertices"]
            require(
                len(indices) == 3
                and all(type(i) is int and 0 <= i < len(vertices) for i in indices),
                "triangle index outside source vertex array",
            )
            if label == "triangles":
                index = triangle["material"]
                require(
                    type(index) is int and 0 <= index < len(materials), "unknown triangle material"
                )
    require(3 <= len(course["path"]) <= 8192, "route point count outside converter bounds")
    for p in course["path"]:
        require(
            len(p["position"]) == 3
            and all(type(x) in (int, float) and math.isfinite(x) for x in p["position"]),
            "invalid route point",
        )


def compile_geometry(course, scale=0.05):
    require(
        math.isfinite(scale) and 0.025 <= scale <= 1,
        "prototype scale must be finite and between 0.025 and 1",
    )
    validate_source(course)
    source_vertices = course["vertices"]
    bounds = course["validation"]
    center = [(bounds["bounds_min"][i] + bounds["bounds_max"][i]) * 0.5 for i in (0, 2)]
    vertices = [
        transform(v["position"], center, scale) + v["texcoord_st"] + v["color_rgba"]
        for v in source_vertices
    ]
    collision = defaultdict(list)
    for triangle in course["collision_triangles"]:
        key = canonical_triangle(triangle["vertices"])
        value = {k: triangle[k] for k in ("surface", "section", "flags")}
        if value not in collision[key]:
            collision[key].append(value)
    source = []
    attached = set()
    for triangle in course["triangles"]:
        key = canonical_triangle(triangle["vertices"])
        # Duplicate material passes must not duplicate physics contacts.
        attributes = collision.get(key, []) if key not in attached else []
        if attributes:
            attached.add(key)
        source.append(
            {
                "indices": triangle["vertices"],
                "material": triangle["material"],
                "render": True,
                "collision": attributes,
            }
        )
    for key, attributes in collision.items():
        if key not in attached:
            source.append(
                {"indices": list(key), "material": None, "render": False, "collision": attributes}
            )

    cells = defaultdict(list)
    counts = Counter()
    for index, triangle in enumerate(source):
        # Native 14A10 computes (A-B) x (C-B), the reverse of the conventional
        # face normal. Reverse source winding to keep the floor facing up.
        original = [list(vertices[triangle["indices"][i]]) for i in (0, 2, 1)]
        if triangle["material"] is not None:
            offset = course["materials"][triangle["material"]].get("native_uv_offset_st", [0, 0])
            for v in original:
                v[3] -= offset[0]
                v[4] -= offset[1]
        lower = [min(v[i] for v in original) for i in range(2)]
        upper = [max(v[i] for v in original) for i in range(2)]
        low_cell = [math.floor((x + GRID_HALF) / CELL_SIZE) for x in lower]
        high_cell = [math.floor((x + GRID_HALF) / CELL_SIZE) for x in upper]
        require(
            all(0 <= x < GRID_WIDTH for x in low_cell + high_cell),
            "course exceeds Road Rash's fixed terrain grid",
        )
        for gx in range(low_cell[0], high_cell[0] + 1):
            for gy in range(low_cell[1], high_cell[1] + 1):
                clipped = original
                for axis, cell in enumerate((gx, gy)):
                    low = cell * CELL_SIZE - GRID_HALF
                    clipped = clip_polygon(clipped, axis, low, True)
                    clipped = clip_polygon(clipped, axis, low + CELL_SIZE, False)
                if len(clipped) < 3:
                    continue
                rounded = [quantize(v) for v in clipped]
                for k in range(1, len(rounded) - 1):
                    original_piece = [clipped[0], clipped[k], clipped[k + 1]]
                    if squared_area(original_piece) < 1e-14:
                        continue
                    piece = [rounded[0], rounded[k], rounded[k + 1]]
                    if squared_area(piece) == 0:
                        counts["quantization_collapsed_triangles"] += 1
                        if triangle["collision"]:
                            counts["quantization_collapsed_collision_triangles"] += 1
                        continue
                    cell_index = gx * GRID_WIDTH + gy
                    cells[cell_index].append(
                        {
                            "vertices": piece,
                            "material": triangle["material"],
                            "render": triangle["render"],
                            "collision": triangle["collision"],
                            "source_triangle": index,
                        }
                    )
                    counts["triangles_after_clipping"] += 1
                    counts["render_triangles"] += int(triangle["render"])
                    counts["collision_triangles"] += bool(triangle["collision"])
    path = [transform(p["position"], center, scale) for p in course["path"]]
    # Route/actor state uses rider world units, distinct from terrain storage.
    path = [[p[0] * 0.25, p[1] * 0.25, p[2] * 0.125] for p in path]
    length = sum(math.dist(a, b) for a, b in zip(path, path[1:] + path[:1]))
    output = {
        "format": "rr64-private-course-geometry",
        "version": 1,
        "source": course["source"],
        "course": course["course"],
        "transform": {
            "scale": scale,
            "source_center_xz": center,
            "world": "(x-center_x, -(z-center_z), y) * scale",
            "terrain_vertex_scale_from_world": [4, 4, 8],
            "reverse_triangle_winding": True,
        },
        "textures": course["textures"],
        "materials": course["materials"],
        "path_world": path,
        "cells": [
            {"index": index, "triangles": triangles} for index, triangles in sorted(cells.items())
        ],
        "validation": {
            **dict(counts),
            "cells": len(cells),
            "path_world_length": length,
            "source_render_triangles": len(course["triangles"]),
            "source_collision_triangles": len(course["collision_triangles"]),
            "collision_only_source_triangles": len(collision) - len(attached),
        },
        "limitations": [
            "Road Rash uses integer terrain vertices; quantization losses are reported.",
            "Native collision surface mapping and material conversion are not applied here.",
            "Route lane bounds, start/finish placement and native contacts still need verification.",
            "Dynamic Mario Kart actors and gameplay are outside this static-course prototype.",
        ],
    }
    return output
