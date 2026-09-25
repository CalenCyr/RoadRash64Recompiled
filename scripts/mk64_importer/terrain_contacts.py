"""Pure finite authored contacts and conservative rail panel classification.

Derived from verified user-ROM course vertices; no packaged donor assets.
"""

import collections, math, struct
from collections import Counter, defaultdict, deque


def sub(a, b):
    return tuple((x - y for x, y in zip(a, b)))


def dot(a, b):
    return sum((x * y for x, y in zip(a, b)))


def cross(a, b):
    return (a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0])


def unit(a):
    length = math.sqrt(dot(a, a))
    return tuple((x / length for x in a)) if length else None


def normal(points):
    return unit(cross(sub(points[1], points[0]), sub(points[2], points[0])))


def panel(a, b, scale):
    """Return exact original base/top vertices, or None for an unproven quad."""
    ap, bp = ([tuple(p) for p in a["vertices"]], [tuple(p) for p in b["vertices"]])
    if (
        a["source"]["display_list"] != b["source"]["display_list"]
        or a["source"]["surface"] != b["source"]["surface"]
    ):
        return None
    shared = set(ap) & set(bp)
    points = sorted(set(ap + bp))
    if len(shared) != 2 or len(points) != 4:
        return None
    na, nb = (normal(ap), normal(bp))
    if (
        na is None
        or nb is None
        or abs(na[2]) > 0.12
        or (abs(nb[2]) > 0.12)
        or (abs(dot(na, nb)) < 0.98)
    ):
        return None
    edge = sorted(shared)
    direction = sub(edge[1], edge[0])
    pa = next((p for p in ap if p not in shared))
    pb = next((p for p in bp if p not in shared))
    side_a = dot(cross(direction, sub(pa, edge[0])), na)
    side_b = dot(cross(direction, sub(pb, edge[0])), na)
    if side_a * side_b >= -1e-12:
        return None
    candidates = []
    for i, j, k, l in ((0, 1, 2, 3), (0, 2, 1, 3), (0, 3, 1, 2)):
        columns = [
            sorted((points[i], points[j]), key=lambda p: p[2]),
            sorted((points[k], points[l]), key=lambda p: p[2]),
        ]
        heights = [top[2] - base[2] for base, top in columns]
        skews = [math.hypot(top[0] - base[0], top[1] - base[1]) for base, top in columns]
        if (
            min(heights) <= 0
            or max(skews) > scale + 1e-08
            or any((s > h * 0.12 + 1e-08 for s, h in zip(skews, heights)))
        ):
            continue
        if any((set(column) == shared for column in columns)):
            continue
        columns.sort(key=lambda c: c[0])
        base = [columns[0][0], columns[1][0]]
        top = [columns[0][1], columns[1][1]]
        if math.hypot(base[1][0] - base[0][0], base[1][1] - base[0][1]) <= scale:
            continue
        if dot(sub(top[1], top[0]), sub(base[1], base[0])) <= 0:
            continue
        candidates.append({"base": [list(p) for p in base], "top": [list(p) for p in top]})
    return candidates[0] if len(candidates) == 1 else None


def derive(triangles, scale):
    by_id = {t["id"]: t for t in triangles}
    assert len(by_id) == len(triangles) and all(
        (math.isfinite(v) for t in triangles for p in t["vertices"] for v in p)
    )
    edges = collections.defaultdict(list)
    for t in triangles:
        points = [tuple(p) for p in t["vertices"]]
        for a, b in zip(points, points[1:] + points[:1]):
            edges[tuple(sorted((a, b)))].append(t["id"])
    candidates = {}
    for ids in edges.values():
        if len(ids) != 2:
            continue
        i, j = sorted(ids)
        found = panel(by_id[i], by_id[j], scale)
        if found is not None:
            candidates[i, j] = found
    counts = collections.Counter((i for pair in candidates for i in pair))
    rails = []
    for pair, value in sorted(candidates.items()):
        if any((counts[i] != 1 for i in pair)):
            continue
        rails.append({"id": len(rails), "triangle_ids": list(pair), **value})
    used = [i for r in rails for i in r["triangle_ids"]]
    assert len(used) == len(set(used))
    return (
        rails,
        {
            "candidate_panels": len(candidates),
            "ambiguous_triangles": sum((n > 1 for n in counts.values())),
            "unclassified_triangles": len(triangles) - len(used),
        },
    )


def authored_surfaces(course, geometry, source_hash):
    """Use current authored collision faces, never render-only decoration."""
    center = geometry["transform"]["source_center_xz"]
    translation = geometry["transform"]["atlas_translation_world"]
    scale = geometry["transform"]["scale"]
    seen = set()
    surfaces = []
    walls = []
    for index, t in enumerate(course["collision_triangles"]):
        points = [course["vertices"][i]["position"] for i in t["vertices"]]
        a, b = [[points[j][k] - points[0][k] for k in range(3)] for j in (1, 2)]
        n = [a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0]]
        squared = [v * v for v in n]
        if not sum(squared):
            continue
        key = tuple(sorted((tuple(p) for p in points)))
        if key in seen:
            continue
        seen.add(key)
        vertices = [
            [
                (p[0] - center[0]) * scale + translation[0],
                -(p[2] - center[1]) * scale + translation[1],
                p[1] * scale + translation[2],
            ]
            for p in points
        ]
        record = {
            "id": index,
            "vertices": vertices,
            "source": {
                "collision_triangle": index,
                "display_list": t["display_list"],
                "surface": t["surface"],
                "section": t["section"],
                "flags": t["flags"],
                "source_vertices": t["vertices"],
            },
        }
        surfaces.append(record)
        if squared[1] < max(squared[0], squared[2]):
            walls.append(record)
    common = {
        "version": 1,
        "course": course["course"],
        "source_course_sha256": source_hash,
        "source_to_world_scale": scale,
        "coordinate_order": "rider x,z,height; final atlas translation",
        "source_reference": "src/racing/collision.c:1735 dominant-axis wall classification",
    }
    return {
        "walls": common | {"format": "rr64-course-walls", "triangles": walls},
        "surfaces": common | {"format": "rr64-course-surfaces", "triangles": surfaces},
    }


def preserve_floor_order(old_blob, new_blob):
    """Keep all original nonvertical contact records in first-hit order.

    Only authenticated vertical rail contacts differ. They are appended per bucket
    after the unchanged floors; their finite collision is handled by wall metadata.
    """

    def decode(blob):
        arrays = {}
        for sub in range(struct.unpack_from(">H", blob, 12)[0]):
            ref = 240 + sub * 12
            start = ref + struct.unpack_from(">I", blob, ref)[0]
            p = start + 24
            for _ in range(struct.unpack_from(">H", blob, start + 12)[0]):
                _, count, size, _ = struct.unpack_from(">4H", blob, p)
                arrays[p + 8] = [blob[p + 8 + i * 16 : p + 8 + (i + 1) * 16] for i in range(count)]
                p += size
        c = struct.unpack_from(">I", blob, 16)[0]
        out = []
        for group in range(struct.unpack_from(">H", blob, c + 14)[0]):
            p = c + struct.unpack_from(">H", blob, c + 16 + group * 2)[0]
            bucket, lists = struct.unpack_from(">HH", blob, p)
            p += 4
            for _ in range(lists):
                off, surf, count = struct.unpack_from(">HBB", blob, p)
                for i in range(count):
                    word = struct.unpack_from(">H", blob, p + 4 + i * 2)[0]
                    vs = tuple(
                        (arrays[off][j] for j in (word >> 10 & 31, word >> 5 & 31, word & 31))
                    )
                    out.append(((bucket, vs, surf), (off, word)))
                p += 4 + 2 * (count + (count & 1))
        return (c, out)

    def vertical(k):
        a, b, c = [struct.unpack_from(">hhh", v) for v in k[1]]
        return (b[0] - a[0]) * (c[1] - a[1]) == (b[1] - a[1]) * (c[0] - a[0])

    _, oldrows = decode(old_blob)
    offset, newrows = decode(new_blob)
    oldfloor = [k for k, _ in oldrows if not vertical(k)]
    newfloor = [k for k, _ in newrows if not vertical(k)]
    assert Counter(oldfloor) == Counter(newfloor), "nonvertical original contact changed"
    refs = defaultdict(deque)
    for k, v in newrows:
        refs[k].append(v)
    ordered = sorted(
        [(k, refs[k].popleft()) for k in oldfloor] + [(k, v) for k, v in newrows if vertical(k)],
        key=lambda kv: kv[0][0],
    )
    buckets = [[] for _ in range(64)]
    for (bucket, _, surf), (off, word) in ordered:
        lists = buckets[bucket]
        if not lists or lists[-1][:2] != (off, surf) or len(lists[-1][2]) == 255:
            lists.append((off, surf, []))
        lists[-1][2].append(word)
    collision = bytearray(144)
    for bucket, lists in enumerate(buckets):
        assert len(collision) <= 65535 and len(lists) <= 255
        struct.pack_into(">H", collision, 16 + bucket * 2, len(collision))
        collision.extend(struct.pack(">HH", bucket, len(lists)))
        for off, surf, words in lists:
            collision.extend(struct.pack(">HBB", off, surf, len(words)))
            collision.extend(struct.pack(">" + str(len(words)) + "H", *words))
            collision.extend(bytes(-len(collision) % 4))
    struct.pack_into(">IIIHH", collision, 0, 60, len(collision), 0, 0, 64)
    out = bytearray(new_blob[:offset]) + collision
    out.extend(bytes(-len(out) % 8))
    struct.pack_into(">I", out, 4, len(out))
    assert [k for k, _ in decode(out)[1] if not vertical(k)] == oldfloor
    return bytes(out)
