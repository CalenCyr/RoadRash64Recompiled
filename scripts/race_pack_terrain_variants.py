"""Choose one authored ground mesh for explicitly verified camera alternatives.

The donor selects near and distant terrain separately. A static union can make
their differently sloped triangles intersect. Exact-plane deduplication cannot
repair that. This offline pass clips only a caller-verified coarse/detail pair
in the ground plane, retaining all uncovered coarse terrain and its attributes.
It never guesses pairs, edits authored collision, or moves a triangle in depth.
After conversion, exploration contact must be rebuilt from the retained mesh;
reusing contacts previously added to discarded scenery creates invisible floors.
"""
from collections import defaultdict
from fractions import Fraction as F
import copy

from race_pack_ground_detail import area, attributes, cross, scalar, side, subtract_union


def prefer_authored_terrain_detail(before, pairs, roots, accepted_collision_triangles,
                                  *, compatible_textures=(), adjacent_physical_pairs=(),
                                  opaque_cutout_textures=()):
    """Resolve listed same-texture, exclusive-camera upward terrain pairs.

    A pair is (coarse leaf, detailed leaf), established from donor section lists
    and terrain topology, not merely intersecting bounds. Both leaves must be
    present and mutually exclusive. Retained leaves cannot also be discarded.
    Physical source triangles, vertical faces, alpha passes, other materials,
    real mesh openings and all unlisted leaves are protected.

    An explicitly listed adjacent physical leaf may share camera roots with the
    coarse leaf. It must join a retained, exclusive-camera primary detail leaf
    along an actual 3D source edge. This handles a coarse road extending beyond
    its detailed road boundary into the detailed shoulder; only the shoulder's
    accepted physical triangles may clip that residual. It does not admit an
    arbitrary projected overlap or a lower floor beneath a real overpass.
    """
    pairs = set(map(tuple, pairs))
    adjacent_physical_pairs = set(map(tuple, adjacent_physical_pairs))
    compatible_textures = set(map(tuple, compatible_textures))
    # A donor can leave TEX_EDGE enabled on an entirely opaque terrain texture.
    # The caller must authenticate every texel of explicitly named assets; do
    # not infer opacity from the display-list name or silently admit alpha art.
    opaque_cutout_textures = set(opaque_cutout_textures)
    if any(not isinstance(t, str) or not t for t in opaque_cutout_textures):
        raise ValueError('Expected authenticated opaque cutout texture names')
    if any(len(p) != 2 for p in pairs | adjacent_physical_pairs):
        raise ValueError('Expected coarse/detail leaf pairs')
    coarse = {a for a, _ in pairs}
    detailed = {b for _, b in pairs | adjacent_physical_pairs}
    known = {t['display_list'] for t in before['triangles']}
    if (coarse & detailed or not (coarse | detailed) <= known
            or not {a for a, _ in adjacent_physical_pairs} <= coarse):
        raise ValueError('Retained terrain leaves must exist and remain unmodified')
    for a, b in pairs:
        if not roots.get(a) or not roots.get(b) or roots[a] & roots[b]:
            raise ValueError('Terrain alternatives must be exclusive in donor cameras')

    def positions(t):
        return [tuple(F(v) for v in before['vertices'][i]['position']) for i in t['vertices']]

    def key(t):
        return tuple(sorted(positions(t)))

    def shape(t):
        m = before['materials'][t['material']]
        if not m['texture_enabled'] or any('XLU' in v for v in m['render']):
            return None
        if any('TEX_EDGE' in v for v in m['render']):
            if (m['texture'] not in opaque_cutout_textures
                    or any(before['vertices'][i]['color_rgba'][3] != 255 for i in t['vertices'])):
                return None
        p = positions(t)
        n = cross([p[1][k]-p[0][k] for k in range(3)], [p[2][k]-p[0][k] for k in range(3)])
        if n[1] <= 0:
            return None
        polygon = [(v[0], v[2]) for v in p]
        bounds = [(min(v[k] for v in polygon), max(v[k] for v in polygon)) for k in range(2)]
        return polygon, bounds

    physical = {key(t) for t in accepted_collision_triangles}
    physical_edges = defaultdict(set)
    for t in before['triangles']:
        if key(t) in physical:
            p = positions(t)
            physical_edges[t['display_list']].update(
                tuple(sorted((p[i], p[(i + 1) % 3]))) for i in range(3))
    for a, b in adjacent_physical_pairs:
        if not roots.get(b) or not any(
                c == a and physical_edges[primary] & physical_edges[b]
                for c, primary in pairs):
            raise ValueError('Adjacent terrain must share a physical 3D edge with primary detail')
    targets = defaultdict(list)
    for j, t in enumerate(before['triangles']):
        if t['display_list'] not in detailed:
            continue
        s = shape(t)
        if s:
            targets[t['display_list']].append((j, t, *s))
    mapping = defaultdict(set)
    for a, b in pairs | adjacent_physical_pairs:
        mapping[a].add(b)
    after = copy.deepcopy(before)
    result, changes, cache = [], [], {}
    for i, t in enumerate(before['triangles']):
        leaf = t['display_list']
        s = shape(t) if leaf in coarse and key(t) not in physical else None
        if s is None:
            result.append(t)
            continue
        subject, box = s
        material = before['materials'][t['material']]
        choices = []
        for detail in sorted(mapping[leaf]):
            for j, u, polygon, bounds in targets[detail]:
                if (leaf, detail) in adjacent_physical_pairs and key(u) not in physical:
                    continue
                near_texture = before['materials'][u['material']]['texture']
                if (material['texture'] != near_texture
                        and (material['texture'], near_texture) not in compatible_textures):
                    continue
                if any(min(box[k][1], bounds[k][1]) <= max(box[k][0], bounds[k][0]) for k in range(2)):
                    continue
                if sum(map(area, subtract_union(subject, [polygon])), F(0)) < area(subject):
                    choices.append((j, polygon))
        if not choices:
            result.append(t)
            continue
        residual = subtract_union(subject, [p for _, p in choices])
        original_area = area(subject)
        residual_area = sum(map(area, residual), F(0))
        assert 0 <= residual_area < original_area
        first = len(result)
        collapsed = F(0)
        attrs = [list(map(F, attributes(before['vertices'][v]))) for v in t['vertices']]
        denominator = side(*subject)
        for polygon in residual:
            for k in range(1, len(polygon)-1):
                points = [polygon[0], polygon[k], polygon[k+1]]
                if not area(points):
                    continue
                values = []
                for point in points:
                    w = [side(subject[1], subject[2], point)/denominator,
                         side(subject[2], subject[0], point)/denominator,
                         side(subject[0], subject[1], point)/denominator]
                    assert sum(w) == 1 and all(0 <= v <= 1 for v in w)
                    values.append(tuple(sum(w[j]*attrs[j][c] for j in range(3)) for c in range(9)))
                if not area([(F(scalar(v[0])), F(scalar(v[2]))) for v in values]):
                    collapsed += area(points)
                    continue
                indices = []
                for value in values:
                    if value not in cache:
                        cache[value] = len(after['vertices'])
                        v = list(map(scalar, value))
                        after['vertices'].append(dict(position=v[:3], texcoord_st=v[3:5], color_rgba=v[5:]))
                    indices.append(cache[value])
                result.append({**t, 'vertices': indices, 'terrain_variant_source_triangle': i})
        changes.append(dict(triangle=i, coarse_leaf=leaf, detail_triangles=[j for j, _ in choices],
                            area=str(original_area), residual_area=str(residual_area),
                            unrepresentable_residual_area=str(collapsed), residual_triangles=len(result)-first))
    after['triangles'] = result
    after['validation']['render_triangles'] = len(result)
    after['validation']['vertices'] = len(after['vertices'])
    for name in ('collision_triangles', 'collision_roots', 'path', 'paths', 'materials', 'textures'):
        assert after[name] == before[name]
    return after, dict(changes=changes, before_triangles=len(before['triangles']), after_triangles=len(result))
