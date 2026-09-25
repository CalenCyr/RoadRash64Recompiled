"""Opt-in collision for an imported course's final visible terrain mesh.

This is an explicit exploration adaptation, not donor collision restoration.
Water and nonterrain overlay materials must be classified by the caller. Keep
source winding and separate height layers; never project or fill a world plane.
"""
from collections import Counter
import copy
import math
import struct


def encoded_visible_faces(blob):
    """Read final quantized visible triangles without changing packet ordering.

    Native cells use a world XY origin and local signed coordinates. The returned
    points are absolute terrain units (4x, 4y, 8z), not rider coordinates.
    """
    origin = struct.unpack_from('>2f', blob, 0x1c)
    faces = []
    for sub in range(struct.unpack_from('>H', blob, 12)[0]):
        ref = 0xf0 + sub * 12
        start = ref + struct.unpack_from('>I', blob, ref)[0]
        packets, visible = struct.unpack_from('>HH', blob, start + 12)
        texture = struct.unpack_from('>H', blob, start + 18)[0]
        p = start + 0x18
        for packet in range(packets):
            count, vertex_count, size, vertex_bytes = struct.unpack_from('>4H', blob, p)
            assert vertex_bytes == vertex_count * 16 and vertex_count <= 32
            vertices = [struct.unpack_from('>hhh', blob, p + 8 + i * 16)
                        for i in range(vertex_count)]
            if packet < visible:
                for i in range(count):
                    word = struct.unpack_from('>H', blob, p + 8 + vertex_bytes + i * 2)[0]
                    indices = (word >> 10 & 31, word >> 5 & 31, word & 31)
                    points = [(int(vertices[j][0] + origin[0]),
                               int(vertices[j][1] + origin[1]), vertices[j][2]) for j in indices]
                    faces.append(dict(texture=texture, vertices=points,
                                      vertex_offset=p + 8, word=word))
            p += size
    return faces


def promote_encoded_visible_terrain(blob, *, eligible_textures, surface=5):
    """Add finite contact to explicitly classified final visible terrain.

    This is the encoded counterpart to ``promote_visible_terrain``. It preserves
    the entire rendered packet prefix byte for byte, so earlier rail edits,
    camera-variant subtraction, material state and quantization are not undone.
    Callers must exclude water/lava, billboards and overlays. No guessed floor,
    bounding wall, route corridor or triangle spanning a genuine void is added.
    Existing native first-hit list order is retained; overlapping added height
    layers are separated before returning. Fail closed on narrow-reference limits.
    """
    from collections import defaultdict
    assert 0 <= surface < 15
    origin = struct.unpack_from('>2f', blob, 0x1c)
    offset = struct.unpack_from('>I', blob, 16)[0]
    arrays = {}
    for sub in range(struct.unpack_from('>H', blob, 12)[0]):
        ref = 0xf0 + sub * 12
        start = ref + struct.unpack_from('>I', blob, ref)[0]
        p = start + 0x18
        for _ in range(struct.unpack_from('>H', blob, start + 12)[0]):
            _, count, size, _ = struct.unpack_from('>4H', blob, p)
            arrays[p + 8] = [(int(x + origin[0]), int(y + origin[1]), z)
                for x, y, z in (struct.unpack_from('>hhh', blob, p + 8 + i * 16)
                                for i in range(count))]
            p += size
    canonical = lambda ps: min(tuple(ps[i:] + ps[:i]) for i in range(3))
    old_groups = []
    known = set()
    for group in range(struct.unpack_from('>H', blob, offset + 14)[0]):
        p = offset + struct.unpack_from('>H', blob, offset + 16 + group * 2)[0]
        bucket, count = struct.unpack_from('>HH', blob, p)
        start = p + 4
        p = start
        for _ in range(count):
            vertices, _, triangles = struct.unpack_from('>HBB', blob, p)
            for i in range(triangles):
                word = struct.unpack_from('>H', blob, p + 4 + i * 2)[0]
                ps = [arrays[vertices][j] for j in (word >> 10 & 31, word >> 5 & 31, word & 31)]
                known.add(canonical(ps))
            p += 4 + 2 * (triangles + (triangles & 1))
        old_groups.append((bucket, count, blob[start:p]))
    assert [b for b, _, _ in old_groups] == list(range(64))
    added = []
    skipped = Counter()
    buckets = [defaultdict(list) for _ in range(64)]
    for face in encoded_visible_faces(blob):
        if face['texture'] not in eligible_textures:
            skipped['excluded_material'] += 1
            continue
        ps = face['vertices']
        key = canonical(ps)
        if key in known:
            skipped['existing_collision_or_duplicate'] += 1
            continue
        a, b, c = ps
        u = [b[i] - a[i] for i in range(3)]
        v = [c[i] - a[i] for i in range(3)]
        n = (u[1]*v[2]-u[2]*v[1], u[2]*v[0]-u[0]*v[2], u[0]*v[1]-u[1]*v[0])
        if n == (0, 0, 0):
            skipped['quantized_degenerate'] += 1
            continue
        assert face['vertex_offset'] <= 65535, 'Visible contact reference exceeds native u16'
        for x in range(8):
            for y in range(8):
                if all(max(p[k] for p in ps) >= origin[k] - 500 + t * 125
                       and min(p[k] for p in ps) <= origin[k] - 375 + t * 125
                       for k, t in enumerate((x, y))):
                    bucket = sum(((x >> bit) & 1) << (2*bit)
                                 | ((y >> bit) & 1) << (2*bit+1) for bit in range(3))
                    buckets[bucket][face['vertex_offset']].append(face['word'])
        added.append(face)
        known.add(key)
    if not added:
        return blob, dict(added=[], added_count=0, skipped=dict(skipped))
    contact = bytearray(0x10 + 64 * 2)
    for bucket, count, original in old_groups:
        lists = [(vertices, words[i:i+255]) for vertices, words in buckets[bucket].items()
                 for i in range(0, len(words), 255)]
        assert len(contact) <= 65535 and count + len(lists) <= 255
        struct.pack_into('>H', contact, 16 + bucket * 2, len(contact))
        contact.extend(struct.pack('>HH', bucket, count + len(lists)))
        contact.extend(original)
        for vertices, words in lists:
            contact.extend(struct.pack('>HBB', vertices, surface, len(words)))
            contact.extend(struct.pack('>' + str(len(words)) + 'H', *words))
            contact.extend(bytes((-len(contact)) % 4))
    struct.pack_into('>IIIHH', contact, 0, 0x3c, len(contact), 0, 0, 64)
    out = bytearray(blob[:offset]) + contact
    out.extend(bytes((-len(out)) % 8))
    struct.pack_into('>I', out, 4, len(out))
    result = preserve_existing_contacts_and_split_added(blob, bytes(out))
    assert result[8:offset] == blob[8:offset], 'Rendered packet prefix changed'
    return result, dict(added=added, added_count=len(added), skipped=dict(skipped))


def promote_visible_terrain(course, *, excluded_textures, excluded_materials=()):
    output = copy.deepcopy(course)
    vertices = course['vertices']

    def key(triangle):
        # Geometric duplicates may have different UV/vertex-cache indices.
        points = tuple(tuple(vertices[i]['position']) for i in triangle['vertices'])
        return min(points[i:] + points[:i] for i in range(3))

    known = {key(t) for t in course['collision_triangles']}
    added, skipped = [], Counter()
    for index, triangle in enumerate(course['triangles']):
        material = course['materials'][triangle['material']]
        if triangle['material'] in excluded_materials:
            skipped['explicit_nonterrain_overlay'] += 1
            continue
        if material['texture'] in excluded_textures:
            skipped['water'] += 1
            continue
        if any('XLU' in mode or 'TEX_EDGE' in mode for mode in material['render']):
            skipped['alpha_or_cutout_decoration'] += 1
            continue
        identity = key(triangle)
        if identity in known:
            skipped['existing_collision_or_duplicate'] += 1
            continue
        a, b, c = [vertices[i]['position'] for i in triangle['vertices']]
        u, v = [[q[k] - a[k] for k in range(3)] for q in (b, c)]
        normal = [u[1]*v[2]-u[2]*v[1], u[2]*v[0]-u[0]*v[2], u[0]*v[1]-u[1]*v[0]]
        size = math.sqrt(sum(n*n for n in normal))
        if size <= 1e-10:
            skipped['degenerate'] += 1
            continue
        contact = dict(vertices=triangle['vertices'], surface='SNOW_OFFROAD',
                       section=255, flags=0, display_list=triangle['display_list'],
                       visible_exploration_source_triangle=index)
        output['collision_triangles'].append(contact)
        added.append(dict(render_triangle=index, collision_index=len(output['collision_triangles'])-1,
                          normal=[n/size for n in normal], texture=material['texture']))
        known.add(identity)
    assert output['triangles'] == course['triangles']
    assert output['vertices'] == course['vertices']
    return output, dict(added=added, skipped=dict(skipped), added_count=len(added),
                       original_collision_count=len(course['collision_triangles']),
                       new_collision_count=len(output['collision_triangles']))


def preserve_existing_contacts_and_split_added(before, after):
    """Retain old contacts, and separate overlapping new height layers.

    Native14A10 stops a packet list at its first floor below the query. Separate
    new lists let its existing cross-list nearest-layer selection see every new
    layer. Disjoint or coplanar triangles may share a list. This changes data
    only; the original floor and cache code remain intact.
    """
    from collections import defaultdict, deque
    import struct

    def decode(blob):
        arrays = {}
        for sub in range(struct.unpack_from('>H', blob, 12)[0]):
            ref = 0xf0 + sub*12
            start = ref + struct.unpack_from('>I', blob, ref)[0]
            p = start + 0x18
            for _ in range(struct.unpack_from('>H', blob, start+12)[0]):
                _, count, size, _ = struct.unpack_from('>4H', blob, p)
                arrays[p+8] = [blob[p+8+i*16:p+8+(i+1)*16] for i in range(count)]
                p += size
        offset = struct.unpack_from('>I', blob, 16)[0]
        result = []
        for group in range(struct.unpack_from('>H', blob, offset+14)[0]):
            p = offset + struct.unpack_from('>H', blob, offset+16+group*2)[0]
            bucket, lists = struct.unpack_from('>HH', blob, p)
            p += 4
            for list_id in range(lists):
                vertices, surface, count = struct.unpack_from('>HBB', blob, p)
                for i in range(count):
                    word = struct.unpack_from('>H', blob, p+4+i*2)[0]
                    values = tuple(arrays[vertices][j] for j in (word>>10&31, word>>5&31, word&31))
                    result.append(((bucket, values, surface), (vertices, word), list_id))
                p += 4 + 2*(count+(count&1))
        return offset, result

    _, original = decode(before)
    offset, current = decode(after)
    remaining = Counter(key for key, _, _ in original)
    locations = defaultdict(deque)
    for key, ref, _ in current:
        locations[key].append(ref)
    assert not (remaining - Counter(key for key, _, _ in current)), 'Original contacts removed'
    buckets = [[] for _ in range(64)]
    for key, _, old_list in original:
        bucket, _, surface = key
        vertex_offset, word = locations[key].popleft()
        lists = buckets[bucket]
        identity = (vertex_offset, surface, old_list)
        if not lists or lists[-1][0] != identity or len(lists[-1][1]) == 255:
            lists.append((identity, []))
        lists[-1][1].append(word)
    def compatible(first, second):
        a = [struct.unpack_from('>hhh', v) for v in first]
        b = [struct.unpack_from('>hhh', v) for v in second]
        u = [a[1][k]-a[0][k] for k in range(3)]
        v = [a[2][k]-a[0][k] for k in range(3)]
        n = [u[1]*v[2]-u[2]*v[1], u[2]*v[0]-u[0]*v[2], u[0]*v[1]-u[1]*v[0]]
        if all(sum(n[k]*(p[k]-a[0][k]) for k in range(3)) == 0 for p in b):
            return True
        # Strict separating axes exclude mere shared edges. No overlapping
        # projected interiors means native first-hit order is immaterial.
        for polygon in (a, b):
            for i in range(3):
                p, q = polygon[i], polygon[(i+1)%3]
                axis = (p[1]-q[1], q[0]-p[0])
                if axis == (0, 0):
                    continue
                aa = [x*axis[0]+y*axis[1] for x,y,_ in a]
                bb = [x*axis[0]+y*axis[1] for x,y,_ in b]
                if max(aa) <= min(bb) or max(bb) <= min(aa):
                    return True
        return False

    added_lists = [[] for _ in range(64)]
    for key, _, _ in current:
        if not locations[key]:
            continue
        bucket, _, surface = key
        vertex_offset, word = locations[key].popleft()
        for identity, words, geometries in added_lists[bucket]:
            if identity[:2] == (vertex_offset, surface) and len(words) < 255 and all(compatible(key[1], other) for other in geometries):
                words.append(word)
                geometries.append(key[1])
                break
        else:
            added_lists[bucket].append(((vertex_offset, surface, -1), [word], [key[1]]))
    for bucket, lists in enumerate(added_lists):
        buckets[bucket].extend((identity, words) for identity, words, _ in lists)
    contact = bytearray(0x10+64*2)
    for bucket, lists in enumerate(buckets):
        assert len(contact) <= 65535 and len(lists) <= 255, (len(contact), len(lists))
        struct.pack_into('>H', contact, 0x10+bucket*2, len(contact))
        contact.extend(struct.pack('>HH', bucket, len(lists)))
        for (vertices, surface, _), words in lists:
            contact.extend(struct.pack('>HBB', vertices, surface, len(words)))
            contact.extend(struct.pack('>'+str(len(words))+'H', *words))
            contact.extend(bytes((-len(contact)) % 4))
    struct.pack_into('>IIIHH', contact, 0, 0x3c, len(contact), 0, 0, 64)
    result = bytearray(after[:offset]) + contact
    result.extend(bytes((-len(result)) % 8))
    struct.pack_into('>I', result, 4, len(result))
    assert Counter(k for k, _, _ in decode(bytes(result))[1]) == Counter(k for k, _, _ in current)
    return bytes(result)
