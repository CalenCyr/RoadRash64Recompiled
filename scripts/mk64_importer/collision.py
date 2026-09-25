"""Pure MK64 initialization collision filters; no I/O and no render mutations.

Pinned source 58cfcb022e10f83bc3b889d7e97508cae6837098:
render_courses.c parse_course_displaylists; collision.c add_collision_triangle.
Metadata retains original unfiltered collision indices, never renumbered IDs.
"""

import copy
import math
import struct


def f32(value):
    return struct.unpack(">f", struct.pack(">f", value))[0]


def rejection_reason(course, triangle):
    vertices = [course["vertices"][i] for i in triangle["vertices"]]
    assert all(0 <= v["flag"] < 16 for v in vertices), "Collision needs original donor vertex flags"
    if all(v["flag"] == 4 for v in vertices):
        return "all_vertex_flags_4"
    a, b, c = [v["position"] for v in vertices]
    u = [b[i] - a[i] for i in range(3)]
    v = [c[i] - a[i] for i in range(3)]
    n = [u[1] * v[2] - u[2] * v[1], u[2] * v[0] - u[0] * v[2], u[0] * v[1] - u[1] * v[0]]
    # Source stores cross products in f64, passes their squared sum to sqrtf,
    # then assigns the division back to f32 normalY. Thresholds are float too.
    magnitude = f32(math.sqrt(f32(sum(x * x for x in n))))
    if magnitude == 0:
        return "degenerate"
    ny = f32(f32(n[1]) / magnitude)
    flags = triangle["flags"]
    if flags & 0x8000 and (ny < -f32(0.9) or ny > f32(0.9)):
        return "section_8000_rejects_near_horizontal"
    if flags & 0x2000 and -f32(0.1) < ny < f32(0.1):
        return "section_2000_rejects_near_vertical"
    return None


def filter_course_collision(course):
    """Return a deep copy and set of rejected ORIGINAL collision-triangle IDs.

    Call before adapting/compiling cells. The render triangle list, every vertex
    attribute, all paths, and all other original fields remain unchanged.
    Returned collision list order is original accepted order. Do not use its
    new list positions as existing surfaces/walls/hazard identity numbers.
    """
    removed = {
        i for i, t in enumerate(course["collision_triangles"]) if rejection_reason(course, t)
    }
    key = lambda t: tuple(sorted(tuple(course["vertices"][i]["position"]) for i in t["vertices"]))
    accepted_geometry = {
        key(t) for i, t in enumerate(course["collision_triangles"]) if i not in removed
    }
    rejected_geometry = {
        key(t) for i, t in enumerate(course["collision_triangles"]) if i in removed
    }
    assert (
        not accepted_geometry & rejected_geometry
    ), "Mixed-acceptance duplicate geometry requires explicit identity mapping"
    out = copy.deepcopy(course)
    out["collision_triangles"] = [
        t for i, t in enumerate(out["collision_triangles"]) if i not in removed
    ]
    return out, removed


def filter_pack_collision(metadata, removed_ids):
    """Filter existing surfaces/walls in a copy, preserving retained IDs/coords.

    A rail survives only when every referenced wall triangle survives. No rail
    or triangle is renumbered. Unsupported rail layouts are rejected explicitly.
    """
    assert metadata["format"] in ("rr64-course-surfaces", "rr64-course-walls")
    out = copy.deepcopy(metadata)
    out["triangles"] = [t for t in out["triangles"] if t["id"] not in removed_ids]
    if "rails" in out:
        keep = {t["id"] for t in out["triangles"]}
        for rail in out["rails"]:
            assert "triangle_ids" in rail, "Expected existing rail triangle references"
        out["rails"] = [r for r in out["rails"] if all(i in keep for i in r["triangle_ids"])]
    return out
