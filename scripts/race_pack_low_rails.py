"""Lower explicitly selected rail panels in source meshes and contacts.

This is an opt-in Road Rash gameplay adaptation, not a generic height heuristic.
The builder supplies authenticated original panel metadata and its transform.
"""
from copy import deepcopy
import math


def lower_rainbow_rails(source, walls, surfaces, transform, height=1.25):
    if source['course'] != 'rainbow_road' or walls['course'] != 'rainbow_road' or surfaces['course'] != 'rainbow_road':
        raise ValueError('explicit Rainbow rail adaptation only')
    return lower_selected_rails(source, walls, surfaces, transform,
                                {r['id'] for r in walls['rails']}, height)


def lower_selected_rails(source, walls, surfaces, transform, rail_ids, height=1.25):
    """Lower authenticated panels only; callers must classify actual guardrails.

    A donor wall paired into a rectangle may also be a building or bridge side.
    Those are not railings merely because generic metadata calls them rails.
    Unselected geometry and metadata remain unchanged.
    """
    if not source['course'] == walls['course'] == surfaces['course']:
        raise ValueError('course ownership mismatch')
    selected = set(rail_ids)
    if not selected or not selected <= {r['id'] for r in walls['rails']}:
        raise ValueError('unknown or empty rail selection')
    if not math.isfinite(height) or not 0 < height < 1.6:
        raise ValueError('height must fit the native low-rail range')
    scale = transform['scale']
    if scale != walls['source_to_world_scale'] or scale <= 0:
        raise ValueError('rail transform mismatch')
    center = transform['source_center_xz'];offset = transform['atlas_translation_world']
    def world(p):
        return ((p[0]-center[0])*scale+offset[0], -(p[2]-center[1])*scale+offset[1], p[1]*scale+offset[2])
    result, wall_result, surface_result = deepcopy(source), deepcopy(walls), deepcopy(surfaces)
    lookup = {t['id']: t for t in walls['triangles']}
    top_map = {};rail_ids = set()
    for rail in walls['rails']:
        if rail['id'] not in selected:
            continue
        if len(rail['triangle_ids']) != 2 or rail_ids.intersection(rail['triangle_ids']):
            raise ValueError('ambiguous rail ownership')
        rail_ids.update(rail['triangle_ids'])
        points = {tuple(p) for i in rail['triangle_ids'] for p in lookup[i]['vertices']}
        if points != {tuple(p) for p in rail['base'] + rail['top']}:
            raise ValueError('rail metadata differs from physical faces')
        for base, top in zip(rail['base'], rail['top']):
            # Six original panels have one source-unit endpoint skew. Shorten
            # along the same authored edge, retaining its slope and plane.
            if math.dist(base[:2], top[:2]) > scale or top[2]-base[2] <= height:
                raise ValueError('expected a taller source rail panel')
            fraction = height/(top[2]-base[2])
            replacement = (base[0]+(top[0]-base[0])*fraction,
                           base[1]+(top[1]-base[1])*fraction, base[2]+height)
            if tuple(top) in top_map and top_map[tuple(top)] != replacement:
                raise ValueError('ambiguous shared rail top')
            top_map[tuple(top)] = replacement
    keys = {};changed_collision = set();clones = {}
    def key(t):
        return (t['display_list'], tuple(sorted(tuple(source['vertices'][i]['position']) for i in t['vertices'])))
    def update_triangle(t):
        indices = []
        for index in t['vertices']:
            position = world(source['vertices'][index]['position'])
            if position in top_map:
                if index not in clones:
                    vertex = deepcopy(source['vertices'][index])
                    x,y,z = top_map[position]
                    vertex['position'] = [(x-offset[0])/scale+center[0],
                                          (z-offset[2])/scale, -(y-offset[1])/scale+center[1]]
                    clones[index] = len(result['vertices']);result['vertices'].append(vertex)
                index = clones[index]
            indices.append(index)
        t['vertices'] = indices
    for id in sorted(rail_ids):
        wall = lookup[id];index = wall['source']['collision_triangle']
        triangle = source['collision_triangles'][index]
        if [list(world(source['vertices'][i]['position'])) for i in triangle['vertices']] != wall['vertices']:
            raise ValueError('source rail authentication failed')
        keys[key(triangle)] = id
        update_triangle(result['collision_triangles'][index]);changed_collision.add(index)
    render_count = 0;seen = set()
    for original, triangle in zip(source['triangles'], result['triangles']):
        k = key(original)
        if k in keys:
            update_triangle(triangle);render_count += 1;seen.add(keys[k])
    if seen != rail_ids:
        raise ValueError('a physical rail has no matching visible face')
    for metadata in (wall_result, surface_result):
        found = set()
        for triangle in metadata['triangles']:
            if triangle['id'] in rail_ids:
                triangle['vertices'] = [list(top_map.get(tuple(p), tuple(p))) for p in triangle['vertices']]
                found.add(triangle['id'])
        if found != rail_ids:
            raise ValueError('wall/surface rail identities differ')
    for rail in wall_result['rails']:
        if rail['id'] in selected:
            rail['top'] = [list(top_map[tuple(p)]) for p in rail['top']]
    report = {'course':source['course'],'height_world':height,'panels':len(selected),
              'wall_triangles':len(rail_ids),'render_triangles':render_count,
              'cloned_top_vertices':len(clones),'changed_collision_indices':sorted(changed_collision)}
    return result, wall_result, surface_result, report


def lower_attached_rail_caps(before, lowered, *, maximum_source_width=12):
    """Move narrow visible rail caps with already authenticated lowered sides.

    Collision rail descriptors describe the vertical panel, not its wooden cap.
    Matching only the descriptor triangles leaves that cap suspended at the old
    height. Find the cap through its actual shared top edge and authored rail
    material, then carry each vertex by the adjacent panel's top displacement.
    Floors, buildings, disconnected strips and all physical contacts stay intact.
    """
    if before['course'] != lowered['course'] or len(before['triangles']) != len(lowered['triangles']):
        raise ValueError('rail source identities differ')
    # Banshee's ordinary cap is three donor units wide; two mitered corners
    # extend eleven units beyond the vertical side's endpoint.
    if not 0 < maximum_source_width <= 12:
        raise ValueError('only narrow authored caps are supported')
    result = deepcopy(lowered)
    tops, edges, materials = {}, [], set()
    for old, new in zip(before['triangles'], lowered['triangles']):
        if old == new:
            continue
        if old['material'] != new['material'] or old['display_list'] != new['display_list']:
            raise ValueError('expected a position-only rail lowering')
        moved = []
        for i, j in zip(old['vertices'], new['vertices']):
            p = tuple(before['vertices'][i]['position'])
            q = tuple(lowered['vertices'][j]['position'])
            if p != q:
                if q[1] >= p[1]:
                    raise ValueError('expected lowered rail tops')
                if p in tops and tops[p] != q:
                    raise ValueError('inconsistent shared rail top')
                tops[p] = q
                moved.append(p)
        if len(moved) == 2:
            edges.append((old['material'], *moved))
            materials.add(old['material'])
    if not edges:
        raise ValueError('no authenticated lowered top edges')

    def near_top(p, material):
        if p in tops:
            return tops[p]
        candidates = []
        for m, a, b in edges:
            if m != material:
                continue
            v = [b[k] - a[k] for k in range(3)]
            length2 = sum(x*x for x in v)
            t = max(0, min(1, sum((p[k]-a[k])*v[k] for k in range(3))/length2))
            closest = [a[k]+t*v[k] for k in range(3)]
            distance2 = sum((p[k]-closest[k])**2 for k in range(3))
            if distance2 <= maximum_source_width**2 + 1e-8:
                delta = [(tops[a][k]-a[k])*(1-t)+(tops[b][k]-b[k])*t for k in range(3)]
                candidates.append((distance2, tuple(p[k]+delta[k] for k in range(3))))
        return min(candidates)[1] if candidates else None

    candidates = {}
    for i, (old, new) in enumerate(zip(before['triangles'], lowered['triangles'])):
        if old != new or old['material'] not in materials:
            continue
        xyz = [tuple(before['vertices'][v]['position']) for v in old['vertices']]
        a, b, c = xyz
        u, v = [b[k]-a[k] for k in range(3)], [c[k]-a[k] for k in range(3)]
        n = [u[1]*v[2]-u[2]*v[1], u[2]*v[0]-u[0]*v[2], u[0]*v[1]-u[1]*v[0]]
        if not any(n) or abs(n[1]) < max(abs(n[0]), abs(n[2])):
            continue  # The cap faces up; another vertical rail is not a cap.
        replacements = [near_top(p, old['material']) for p in xyz]
        if all(p is not None for p in replacements):
            candidates[i] = (xyz, replacements)
    connected = {tuple(sorted((a,b))) for _,a,b in edges}
    selected = set()
    while True:
        added = {i for i,(xyz,_) in candidates.items() if i not in selected and
                 any(tuple(sorted((xyz[k],xyz[(k+1)%3]))) in connected for k in range(3))}
        if not added:
            break
        selected.update(added)
        for i in added:
            xyz = candidates[i][0]
            connected.update(tuple(sorted((xyz[k],xyz[(k+1)%3]))) for k in range(3))
    clones = {}
    for i in sorted(selected):
        old = before['triangles'][i]
        indices = []
        for vertex, position in zip(old['vertices'], candidates[i][1]):
            key = vertex, position
            if key not in clones:
                clone = deepcopy(before['vertices'][vertex])
                clone['position'] = list(position)
                clones[key] = len(result['vertices'])
                result['vertices'].append(clone)
            indices.append(clones[key])
        result['triangles'][i]['vertices'] = indices
    result['validation']['vertices'] = len(result['vertices'])
    return result, {'cap_triangles':len(selected), 'cap_vertices':len(clones),
                    'source_triangle_indices':sorted(selected),
                    'maximum_source_width':maximum_source_width}
