"""Keep Koopa Beach's authored near terrain, including its open shortcut.

The original renderer selects coarse/detail terrain by section camera. Their
union is not a more detailed mesh: distant mountains close the shortcut, and
distant beach/waterline faces have different elevations. Keep authenticated near
cliff shells and clip coarse ground against its near surface and joined physical
neighbours. A finite inland top closes the otherwise open authored cliff rim.

This recipe contains identifiers and hashes only. Geometry still comes from the
user's supported ROM. Original collision, ramps, waterfall, water, tunnel floor,
unlisted scenery and genuine openings are untouched. Additional exploration
contact must be regenerated from the resulting visible mesh by the importer.
Beach and shoreline corrections retain every uncovered coarse ground fragment;
only the explicitly authenticated cliff shells are selected as complete leaves.
"""

from collections import defaultdict
from pathlib import Path
import copy
import hashlib
import json
import math
import struct


POLICY = json.loads(Path(__file__).with_suffix(".json").read_text(encoding="utf-8"))
PREFIX = "d_course_koopa_troopa_beach_packed_dl_"


def _shape_digest(source, triangles):
    """Authenticate leaf geometry without depending on gameplay color fixes."""
    rows = sorted(
        (
            source["materials"][t["material"]]["texture"],
            [source["vertices"][v]["position"] for v in t["vertices"]],
        )
        for t in triangles
    )
    return hashlib.sha256(json.dumps(rows, separators=(",", ":")).encode()).hexdigest()


def _close_inland_rim(source, coarse):
    """Derive one finite plateau from the authenticated open cliff boundary.

    The donor never expects a kart above the inward cliff rim and leaves that
    surface absent. Keep the outer sea edge open. No coordinates, donor mesh or
    texture pixels are stored in the recipe; every rim point comes from the ROM.
    Ear clipping respects its concave outline and a conforming subdivision keeps
    texture coordinates and native triangle sizes bounded. The tunnel is below
    this top and retains its own independent walls/floor/openings.
    """
    policy = POLICY['inland_closure']
    edges = defaultdict(list)
    for triangle in source['triangles']:
        material = source['materials'][triangle['material']]
        if (triangle['display_list'] in coarse or not material['texture']
                or any('XLU' in m or 'TEX_EDGE' in m for m in material['render'])):
            continue
        points = [tuple(source['vertices'][v]['position']) for v in triangle['vertices']]
        for i in range(3):
            edges[tuple(sorted((points[i], points[(i+1) % 3])))].append(triangle)
    graph = defaultdict(set)
    for (a,b), triangles in edges.items():
        if len(triangles) == 1:
            graph[a].add(b)
            graph[b].add(a)
    unseen = set(graph)
    selected = None
    while unseen:
        pending = [min(unseen)]
        component = set()
        while pending:
            point = pending.pop()
            if point not in component:
                component.add(point)
                pending.extend(graph[point] - component)
        unseen -= component
        boundary = sorted(e for e in edges if e[0] in component and len(edges[e]) == 1)
        digest = hashlib.sha256(json.dumps(boundary, separators=(',', ':')).encode()).hexdigest()
        if digest != policy['boundary_sha256']:
            continue
        if (selected is not None or len(component) != policy['boundary_vertices']
                or any(len(graph[p]) != 2 for p in component)):
            raise ValueError('Koopa inland rim is not one authenticated closed loop')
        if any(source['materials'][edges[e][0]['material']]['texture'] not in policy['boundary_materials']
               for e in boundary):
            raise ValueError('Koopa inland rim material changed')
        selected = component
    if selected is None:
        raise ValueError('Koopa inland rim is missing or changed')
    ring = [min(selected)]
    previous = None
    while True:
        point = next(p for p in sorted(graph[ring[-1]]) if p != previous)
        if point == ring[0]:
            break
        previous = ring[-1]
        ring.append(point)
    if len(ring) != len(selected):
        raise ValueError('Koopa rim traversal did not cover its boundary')

    def side(a,b,p):
        return (b[0]-a[0])*(p[2]-a[2])-(b[2]-a[2])*(p[0]-a[0])

    area2 = sum(ring[i][0]*ring[(i+1)%len(ring)][2]
                -ring[(i+1)%len(ring)][0]*ring[i][2] for i in range(len(ring)))
    if area2 < 0:
        ring.reverse()
    pending = list(ring)
    triangles = []
    while len(pending) > 3:
        ears = []
        for i,b in enumerate(pending):
            a,c = pending[i-1],pending[(i+1)%len(pending)]
            if side(a,b,c) <= 0:
                continue
            if any(p not in (a,b,c) and min(side(a,b,p),side(b,c,p),side(c,a,p)) >= 0
                   for p in pending):
                continue
            # Prefer the short diagonal; avoid a fan across the long narrow rim.
            ears.append((sum((a[k]-c[k])**2 for k in range(3)), i, (a,c,b)))
        if not ears:
            raise ValueError('Koopa rim cannot be triangulated without crossing its outline')
        _,index,triangle = min(ears)
        triangles.append(triangle)
        del pending[index]
    triangles.append((pending[0],pending[2],pending[1]))
    # Improve interior diagonals before subdivision. A narrow ear can turn a
    # small authored rim-height change into an artificial near-vertical ramp.
    # Local max-min projected quality flips retain the exact concave boundary
    # and interpolate the same donor heights using well-shaped surface faces.
    def quality(t):
        perimeter2 = sum((t[i][0]-t[(i+1)%3][0])**2
                         +(t[i][2]-t[(i+1)%3][2])**2 for i in range(3))
        return abs(side(*t))/perimeter2 if perimeter2 else 0

    def upward(t):
        return t if side(*t) < 0 else (t[0],t[2],t[1])

    for _ in range(len(ring)**2):
        incident = defaultdict(list)
        for i,t in enumerate(triangles):
            for j in range(3):
                incident[tuple(sorted((t[j],t[(j+1)%3])))].append(i)
        changed = False
        for (a,b), owners in sorted(incident.items()):
            if len(owners) != 2:
                continue
            i,j = owners
            c = next(p for p in triangles[i] if p not in (a,b))
            d = next(p for p in triangles[j] if p not in (a,b))
            if side(c,d,a)*side(c,d,b) >= 0:
                continue
            replacements = upward((c,d,a)),upward((d,c,b))
            if min(map(quality,replacements)) <= min(quality(triangles[i]),quality(triangles[j])) + 1e-12:
                continue
            triangles[i],triangles[j] = replacements
            changed = True
            break
        if not changed:
            break
    else:
        raise ValueError('Koopa inland triangulation did not converge')
    # Split the same interior edge in every incident triangle. Retain the donor
    # rim edges intact: independently rounded midpoint coordinates on a cap-only
    # boundary split can leave a native T-junction against the unsplit cliff.
    # Interior faces still share identical interpolated positions and heights.
    rim_edges = {tuple(sorted((ring[i],ring[(i+1)%len(ring)]))) for i in range(len(ring))}
    # The longest fixed rim edge sets the attainable size near that edge; using
    # a smaller global limit there cannot converge without splitting the rim.
    limit2 = max(policy['target_interior_edge_length'] ** 2,
                 max(sum((a[k]-b[k])**2 for k in range(3)) for a,b in rim_edges))
    while True:
        long_edges = {
            tuple(sorted((t[i],t[(i+1)%3]))) for t in triangles for i in range(3)
            if sum((t[i][k]-t[(i+1)%3][k])**2 for k in range(3)) > limit2
            and tuple(sorted((t[i],t[(i+1)%3]))) not in rim_edges
        }
        if not long_edges:
            break
        edge = min(long_edges, key=lambda e:(-sum((e[0][k]-e[1][k])**2 for k in range(3)),e))
        midpoint = tuple((edge[0][k]+edge[1][k])/2 for k in range(3))
        split = []
        for t in triangles:
            if edge[0] not in t or edge[1] not in t:
                split.append(t)
                continue
            i = next(i for i in range(3) if {t[i],t[(i+1)%3]} == set(edge))
            a,b,c = t[i],t[(i+1)%3],t[(i+2)%3]
            split.extend(((a,midpoint,c),(midpoint,b,c)))
        triangles = split
    # Inherit the planar UV density and color from authored beach, without
    # embedding donor texture or UV arrays. Per-face whole-period shifts retain
    # wrapped continuity while fitting the native signed texture coordinates.
    material = next(i for i,m in enumerate(source['materials']) if m['texture'] == policy['top_material'])
    reference = next(t for t in source['triangles'] if t['material'] == material
                     and t['display_list'] not in coarse)
    vertices = [source['vertices'][i] for i in reference['vertices']]
    xyz = [v['position'] for v in vertices]
    denominator = side(*xyz)
    if denominator == 0:
        raise ValueError('Koopa beach UV reference has no projected area')
    period = 32 * next(t['width'] for t in source['textures'] if t['symbol'] == policy['top_material'])
    start = len(source['triangles'])
    for points in triangles:
        uvs = []
        for point in points:
            weights = [side(xyz[1],xyz[2],point)/denominator,
                       side(xyz[2],xyz[0],point)/denominator,
                       side(xyz[0],xyz[1],point)/denominator]
            uvs.append([sum(weights[j]*vertices[j]['texcoord_st'][k] for j in range(3)) for k in range(2)])
        offset = [math.floor(min(v[k] for v in uvs)/period)*period for k in range(2)]
        indices = []
        for point,uv in zip(points,uvs):
            indices.append(len(source['vertices']))
            source['vertices'].append({'position':list(point),
                'texcoord_st':[round(uv[k]-offset[k]) for k in range(2)],
                'color_rgba':list(vertices[0]['color_rgba'])})
        source['triangles'].append({'vertices':indices,'material':material,
            'display_list':'rr64_koopa_inland_top','derived_inland_closure':True})
    source['validation'].update(render_triangles=len(source['triangles']),vertices=len(source['vertices']))
    return {'boundary_vertices':len(ring),'triangles':len(source['triangles'])-start,
            'projected_area':abs(area2)/2,'sea_boundary_unchanged':True,
            'boundary_sha256':policy['boundary_sha256']}


def correct_koopa_beach(source, roots, accepted_collision_triangles):
    """Return (source, report); call after baseline, before final contact encode.

    Fail closed if a recipe's retained physical leaf or section relationship is
    different. Baseline exact-plane recovery may already remove some coarse
    faces. Complete cliff shells carry different topology at the tunnel, while
    beach/shore projection retains uncovered coarse fragments. This pass never
    modifies the original collision array.
    """
    if source.get("course") != "koopa_troopa_beach":
        return source, {"applied": False}
    leaves = defaultdict(list)
    for triangle in source["triangles"]:
        leaves[triangle["display_list"]].append(triangle)
    physical = {t["display_list"] for t in accepted_collision_triangles}
    discarded, ground_pairs, pairs = set(), [], []
    for row in POLICY["pairs"]:
        coarse, detail = PREFIX + row["coarse"], PREFIX + row["detail"]
        if coarse in physical or detail not in physical:
            raise ValueError("Koopa camera replacement changed physical leaf ownership")
        if (not roots.get(coarse) or not roots.get(detail)
                or set(roots[coarse]) & set(roots[detail])):
            raise ValueError("Koopa terrain leaves are not exclusive camera alternatives")
        if _shape_digest(source, leaves[detail]) != row["detail_shape_sha256"]:
            raise ValueError("Koopa retained terrain differs from authenticated donor geometry")
        if row["mode"] == "near-cliff-shell":
            discarded.add(coarse)
        elif row["mode"] == "projected-ground":
            if leaves[coarse]:
                ground_pairs.append((coarse, detail))
        else:
            raise ValueError("Unknown Koopa terrain selection policy")
        pairs.append({"coarse": coarse, "detail": detail,
                      "mode": row["mode"],
                      "coarse_triangles": len(leaves[coarse]),
                      "retained_triangles": len(leaves[detail])})
    result = copy.deepcopy(source)
    result["triangles"] = [t for t in result["triangles"]
                           if t["display_list"] not in discarded]
    result["validation"]["render_triangles"] = len(result["triangles"])
    shell_removed = len(source["triangles"]) - len(result["triangles"])
    adjacent = []
    for row in POLICY['adjacent_ground']:
        coarse,detail = PREFIX+row['coarse'],PREFIX+row['detail']
        if detail not in physical or _shape_digest(source,leaves[detail]) != row['detail_shape_sha256']:
            raise ValueError('Koopa adjacent physical terrain differs from authenticated geometry')
        if leaves[coarse]:
            adjacent.append((coarse,detail))
    from race_pack_terrain_variants import prefer_authored_terrain_detail
    result, ground = prefer_authored_terrain_detail(
        result, ground_pairs, roots, accepted_collision_triangles,
        adjacent_physical_pairs=adjacent,
        compatible_textures=POLICY['compatible_ground_textures'])
    closure = _close_inland_rim(result, {PREFIX+r['coarse'] for r in POLICY['pairs']})
    # Clipping only appends interpolated vertices; original indices remain stable.
    assert result["vertices"][:len(source["vertices"])] == source["vertices"]
    assert result["collision_triangles"] == source["collision_triangles"]
    return result, {
        "applied": True,
        "mode": "koopa-authenticated-near-terrain",
        "pairs": pairs,
        "removed_cliff_triangles": shell_removed,
        "ground_projection": ground,
        "inland_closure": closure,
        "before_triangles": len(source["triangles"]),
        "after_triangles": len(result["triangles"]),
        "original_collision_unchanged": True,
        "unlisted_render_geometry_unchanged": True,
    }


def preserve_koopa_collision_order(before, after):
    """Preserve old physical first-hit order and conservative cell bounds.

    Removing a distant beach shell can shrink its cell's render envelope. The
    inland cap may expand it instead. Use the union so neither old contact nor
    new visible terrain is culled. Bounds are metadata; the shared verifier still
    compares every original contact triangle, origin, grid index and vertex.
    """
    from race_pack_camera_variants import preserve_native_collision_order

    old_bounds = struct.unpack_from(">4f", before, 0x40)
    new_bounds = struct.unpack_from(">4f", after, 0x40)
    union = (max(old_bounds[0],new_bounds[0]), min(old_bounds[1],new_bounds[1]),
             min(old_bounds[2],new_bounds[2]), max(old_bounds[3],new_bounds[3]))
    bounded = bytearray(after)
    prior = bytearray(before)
    struct.pack_into('>4f',bounded,0x40,*union)
    struct.pack_into('>4f',prior,0x40,*union)
    # This still authenticates all other headers and every original physical
    # triangle/vertex/surface/bucket; it cannot silently drop terrain contact.
    return preserve_native_collision_order(bytes(prior), bytes(bounded))
