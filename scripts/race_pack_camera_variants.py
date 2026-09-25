"""Resolve exact opaque camera alternatives before native course encoding.

Donor cameras can select different textures for the same terrain plane. A
static union must not draw the distant alternative over physical detail merely
because their textures differ. This pass needs verified per-leaf camera-root
membership and the donor's accepted physical triangles. It changes rendering
only; noncoplanar layers, alpha passes and physical triangles are protected.
"""
from collections import defaultdict
from fractions import Fraction as F
import copy

from race_pack_ground_detail import area, attributes, cross, scalar, side, subtract_union


def prefer_exact_camera_detail(before, roots, accepted_collision_triangles, *,
                               render_alternative_pairs=(),
                               render_alternative_textures=None):
    """Return a new source course and exact coverage audit; never mutate inputs.

    Only same-facing, exactly coplanar opaque triangles from mutually exclusive
    donor camera roots qualify. An accepted physical triangle always wins over
    a nonphysical alternative; both physical or both nonphysical remain intact.
    A differing texture is allowed, but the retained authored materials are not
    modified. Uncovered pieces retain original winding and interpolated UV/RGBA.

    Explicit (coarse leaf, detail leaf) pairs may also resolve two nonphysical
    alternatives with the SAME authored material. This covers off-road scenery
    that the donor never made solid, so physical-detail preference cannot select
    a winner. Pairs still need exclusive camera roots and exact coverage. Their
    detail leaves cannot themselves be coarse leaves: the retained replacement
    must survive this pass. No other nonphysical overlap is changed.
    An optional texture whitelist narrows those explicit pairs to an audited
    material family when a donor leaf also contains unrelated scenery.
    """
    alternatives = set(map(tuple, render_alternative_pairs))
    alternative_textures = (None if render_alternative_textures is None else
                            set(render_alternative_textures))
    if any(len(pair) != 2 for pair in alternatives):
        raise ValueError('Camera alternatives need coarse/detail leaf pairs')
    coarse_leaves = {a for a, _ in alternatives}
    detail_leaves = {b for _, b in alternatives}
    if coarse_leaves & detail_leaves:
        raise ValueError('Camera detail leaves must remain unmodified')
    known_leaves = {t['display_list'] for t in before['triangles']}
    if not (coarse_leaves | detail_leaves) <= known_leaves:
        raise ValueError('Camera alternative leaf is absent from the source')
    def positions(t):
        return [tuple(F(v) for v in before['vertices'][i]['position']) for i in t['vertices']]

    def key(t):
        return tuple(sorted(positions(t)))

    def opaque(t):
        m = before['materials'][t['material']]
        return m['texture_enabled'] and not any('XLU' in v or 'TEX_EDGE' in v for v in m['render'])

    def plane(t):
        p = positions(t)
        n = cross([p[1][k]-p[0][k] for k in range(3)], [p[2][k]-p[0][k] for k in range(3)])
        if not any(n):
            return None
        # Positive divisor preserves orientation; opposite faces cannot match.
        divisor = abs(next(v for v in n if v))
        exact = tuple(v/divisor for v in (*n, -sum(n[k]*p[0][k] for k in range(3))))
        axis = max(range(3), key=lambda k:abs(n[k]))
        axes = tuple(k for k in range(3) if k != axis)
        polygon = [tuple(v[k] for k in axes) for v in p]
        bounds = tuple((min(v[k] for v in polygon), max(v[k] for v in polygon)) for k in range(2))
        return exact, polygon, bounds

    accepted = {key(t) for t in accepted_collision_triangles}
    details = defaultdict(list)
    planes = {}
    for i,t in enumerate(before['triangles']):
        if not opaque(t) or not roots.get(t['display_list']):
            continue
        shape = plane(t)
        if shape is None:
            continue
        planes[i] = shape
        if key(t) in accepted or t['display_list'] in detail_leaves:
            details[shape[0]].append((i,t,shape[1],shape[2]))

    after = copy.deepcopy(before)
    result,changes,cache = [],[],{}
    for i,t in enumerate(before['triangles']):
        if i not in planes or key(t) in accepted or t['display_list'] in detail_leaves:
            result.append(t)
            continue
        exact,subject,box = planes[i]
        material = before['materials'][t['material']]
        choices = []
        for j,u,p,b in details[exact]:
            if roots[t['display_list']] & roots[u['display_list']]:
                continue
            if key(u) not in accepted:
                if (t['display_list'], u['display_list']) not in alternatives:
                    continue
                if alternative_textures is not None and material['texture'] not in alternative_textures:
                    continue
                if material != before['materials'][u['material']]:
                    continue
            elif material['texture'] == before['materials'][u['material']]['texture']:
                continue  # The existing same-texture physical recovery owns this case.
            if any(min(box[k][1],b[k][1]) <= max(box[k][0],b[k][0]) for k in range(2)):
                continue
            if sum(map(area,subtract_union(subject,[p])),F(0)) < area(subject):
                choices.append((j,p))
        if not choices:
            result.append(t)
            continue
        residual = subtract_union(subject,[p for _,p in choices])
        original_area = area(subject)
        residual_area = sum(map(area,residual),F(0))
        assert 0 <= residual_area < original_area
        first = len(result)
        unrepresentable_area = F(0)
        if residual_area:
            attrs = [list(map(F,attributes(before['vertices'][v]))) for v in t['vertices']]
            denominator = side(subject[0],subject[1],subject[2])
            for polygon in residual:
                for k in range(1,len(polygon)-1):
                    points = [polygon[0],polygon[k],polygon[k+1]]
                    if not area(points):
                        continue
                    point_values = []
                    for point in points:
                        weights = [side(subject[1],subject[2],point)/denominator,
                                   side(subject[2],subject[0],point)/denominator,
                                   side(subject[0],subject[1],point)/denominator]
                        assert sum(weights)==1 and all(0<=v<=1 for v in weights)
                        values = tuple(sum(weights[j]*attrs[j][c] for j in range(3)) for c in range(9))
                        point_values.append(values)
                    # Prior source clipping may already contain binary floats.
                    # Preserve rational sliver coverage in the audit if a piece
                    # collapses when represented in the source's numeric format.
                    normal_axis = max(range(3), key=lambda k:abs(exact[k]))
                    axes = [k for k in range(3) if k != normal_axis]
                    represented = [tuple(F(scalar(v[k])) for k in axes) for v in point_values]
                    if not area(represented):
                        unrepresentable_area += area(points)
                        continue
                    indices = []
                    for values in point_values:
                        if values not in cache:
                            cache[values] = len(after['vertices'])
                            v = list(map(scalar,values))
                            after['vertices'].append({'position':v[:3],'texcoord_st':v[3:5],'color_rgba':v[5:]})
                        indices.append(cache[values])
                    result.append({**t,'vertices':indices,'camera_variant_source_triangle':i})
        changes.append({'triangle':i,'coarse_leaf':t['display_list'],
                        'detail_triangles':[j for j,_ in choices],
                        'area':str(original_area),'residual_area':str(residual_area),
                        'unrepresentable_residual_area':str(unrepresentable_area),
                        'residual_triangles':len(result)-first})
    after['triangles'] = result
    after['validation']['render_triangles'] = len(result)
    after['validation']['vertices'] = len(after['vertices'])
    for name in ('collision_triangles','collision_roots','path','paths','materials','textures'):
        assert after[name] == before[name]
    assert after['vertices'][:len(before['vertices'])] == before['vertices']
    return after, {'changes':changes,'before_triangles':len(before['triangles']),
                   'after_triangles':len(result),'fully_covered':sum(r['residual_area']=='0' for r in changes),
                   'partially_clipped':sum(r['residual_area']!='0' for r in changes)}


def conform_quantized_camera_edge(geometry, *, material, coarse_vertices,
                                 detail_vertex):
    """Join an audited coarse edge to an existing quantized detail vertex.

    Exact source clipping can leave a sub-unit overlap when both meshes round
    independently to the receiving engine's integer grid. The caller identifies
    the proven residual coarse face and retained detail point. Remove the small
    edge wedge only if retained faces of this material cover it completely.
    This does not move detail vertices, bias depth, fill holes or alter original
    physical faces. Subsequent exploration contact comes from the final mesh.
    """
    result = copy.deepcopy(geometry)
    if type(material) is not int or not 0 <= material < len(result['materials']):
        raise ValueError('unknown native material')
    material_state = result['materials'][material]
    if (material_state['render'] != ['G_RM_AA_ZB_OPA_SURF', 'G_RM_AA_ZB_OPA_SURF2'] or
            material_state.get('native_features', 0) & 0x90):
        raise ValueError('only opaque single-sided camera alternatives qualify')
    expected = tuple(tuple(v) for v in coarse_vertices)
    point = tuple(detail_vertex)
    if len(expected) != 3 or len(point) != 3:
        raise ValueError('expected one coarse triangle and detail vertex')
    if any(type(v) is not int for p in (*expected, point) for v in p):
        raise ValueError('native boundary must already be quantized')
    matches = [(c, i, t) for c in result['cells'] for i,t in enumerate(c['triangles'])
               if t['render'] and t['material'] == material and
               tuple(tuple(v[:3]) for v in t['vertices']) == expected]
    if len(matches) != 1:
        raise ValueError('coarse boundary identity is not unique')
    cell, index, triangle = matches[0]
    if triangle['collision']:
        raise ValueError('authored physical face cannot be clipped')
    a,b,c = expected
    n = cross([b[k]-a[k] for k in range(3)], [c[k]-a[k] for k in range(3)])
    if not any(n) or sum(n[k]*(point[k]-a[k]) for k in range(3)):
        raise ValueError('detail vertex is not on the coarse plane')
    axis = max(range(3), key=lambda k:abs(n[k]))
    axes = [k for k in range(3) if k != axis]
    project = lambda p:tuple(F(p[k]) for k in axes)
    polygon = list(map(project, expected)); p = project(point)
    signed = side(*polygon)
    weights = [side(polygon[1],polygon[2],p)/signed,
               side(polygon[2],polygon[0],p)/signed,
               side(polygon[0],polygon[1],p)/signed]
    if not all(0 < w < 1 for w in weights):
        raise ValueError('detail point must lie inside the coarse edge wedge')
    edge = min(range(3), key=lambda k:side(polygon[k],polygon[(k+1)%3],p)**2 /
               sum((polygon[(k+1)%3][j]-polygon[k][j])**2 for j in (0,1)))
    u,v,w = edge, (edge+1)%3, (edge+2)%3
    # A sub-unit integer-rounding defect, never arbitrary scenery overlap.
    distance2 = side(polygon[u],polygon[v],p)**2 / sum((polygon[v][j]-polygon[u][j])**2 for j in (0,1))
    if distance2 > 1:
        raise ValueError('edge wedge exceeds native quantization error')
    wedge = [polygon[u],polygon[v],p]
    covering = []
    shares_detail = False
    for other in cell['triangles']:
        if other is triangle or not other['render'] or other['material'] != material:
            continue
        xyz = [tuple(v[:3]) for v in other['vertices']]
        if any(sum(n[k]*(q[k]-a[k]) for k in range(3)) for q in xyz):
            continue
        normal = cross([xyz[1][k]-xyz[0][k] for k in range(3)],
                       [xyz[2][k]-xyz[0][k] for k in range(3)])
        if sum(normal[k]*n[k] for k in range(3)) <= 0:
            continue
        covering.append(list(map(project,xyz)))
        shares_detail |= point in xyz
    if not shares_detail or sum(map(area,subtract_union(wedge,covering)),F(0)):
        raise ValueError('retained detail does not cover the removed wedge')
    attrs = triangle['vertices']
    vertex = list(point) + [round(sum(weights[j]*attrs[j][k] for j in range(3)))
                            for k in range(3,len(attrs[0]))]
    replacements = [{**triangle,'vertices':[attrs[u],vertex,attrs[w]]},
                    {**triangle,'vertices':[vertex,attrs[v],attrs[w]]}]
    cell['triangles'][index:index+1] = replacements
    return result, {'cell':cell['index'],'material':material,
                    'coarse_vertices':expected,'detail_vertex':point,
                    'removed_wedge_area':str(area(wedge)),
                    'retained_detail_covers_wedge':True,
                    'edge_distance_squared':str(distance2)}


def preserve_native_collision_order(before, after):
    """Keep native first-hit order when render repacking moves vertex packets.

    Both arguments are validated native cells with identical contact payloads.
    Only the collision lookup lists and total byte count may be rewritten;
    render commands, packet vertices and all physical triangle values stay exact.
    """
    from collections import Counter, deque
    import struct

    def decode(blob):
        assert struct.unpack_from('>I',blob)[0]==0x3f
        assert struct.unpack_from('>I',blob,4)[0]==len(blob)
        arrays={}
        for sub in range(struct.unpack_from('>H',blob,12)[0]):
            ref=0xf0+sub*12
            start=ref+struct.unpack_from('>I',blob,ref)[0]
            p=start+0x18
            for _ in range(struct.unpack_from('>H',blob,start+12)[0]):
                _,count,size,_=struct.unpack_from('>4H',blob,p)
                arrays[p+8]=[blob[p+8+i*16:p+8+(i+1)*16] for i in range(count)]
                p+=size
        c=struct.unpack_from('>I',blob,16)[0]
        assert struct.unpack_from('>I',blob,c)[0]==0x3c
        result=[]
        for group in range(struct.unpack_from('>H',blob,c+14)[0]):
            p=c+struct.unpack_from('>H',blob,c+16+group*2)[0]
            bucket,lists=struct.unpack_from('>HH',blob,p);p+=4
            for _ in range(lists):
                offset,surface,count=struct.unpack_from('>HBB',blob,p)
                for i in range(count):
                    word=struct.unpack_from('>H',blob,p+4+i*2)[0]
                    vertices=tuple(arrays[offset][j] for j in (word>>10&31,word>>5&31,word&31))
                    result.append(((bucket,vertices,surface),(offset,word)))
                p+=4+2*(count+(count&1))
        return c,result

    assert before[8:12]==after[8:12] and before[0x18:0x50]==after[0x18:0x50]
    _,old=decode(before);offset,new=decode(after)
    assert Counter(k for k,_ in old)==Counter(k for k,_ in new),'Contact payload changed'
    if [k for k,_ in old]==[k for k,_ in new]:
        return after
    locations=defaultdict(deque)
    for key,ref in new:locations[key].append(ref)
    buckets=[[] for _ in range(64)]
    for key,_ in old:
        bucket,_,surface=key
        vertex_offset,word=locations[key].popleft()
        lists=buckets[bucket]
        if not lists or lists[-1][:2] != (vertex_offset,surface) or len(lists[-1][2])==255:
            lists.append((vertex_offset,surface,[]))
        lists[-1][2].append(word)
    collision=bytearray(0x10+64*2)
    for bucket,lists in enumerate(buckets):
        assert len(collision)<=65535 and len(lists)<=255
        struct.pack_into('>H',collision,0x10+bucket*2,len(collision))
        collision.extend(struct.pack('>HH',bucket,len(lists)))
        for vertex_offset,surface,words in lists:
            collision.extend(struct.pack('>HBB',vertex_offset,surface,len(words)))
            collision.extend(struct.pack('>'+str(len(words))+'H',*words))
            collision.extend(bytes((-len(collision))%4))
    struct.pack_into('>IIIHH',collision,0,0x3c,len(collision),0,0,64)
    result=bytearray(after[:offset])+collision
    result.extend(bytes((-len(result))%8))
    assert len(result)<=65535*8
    struct.pack_into('>I',result,4,len(result))
    assert [k for k,_ in decode(result)[1]]==[k for k,_ in old]
    return bytes(result)
