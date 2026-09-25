"""Authenticated DK/Royal terrain recipes for own-ROM course conversion.

Only conversion policy, donor symbol names and pair relationships are shipped.
Source geometry and texture bytes always come from the caller's extracted ROM.
Apply after the importer's ordinary physical-ground recovery. Final encoding,
contact-order preservation and exploration contacts remain exporter operations.
"""
from copy import deepcopy
import hashlib
import json
from pathlib import Path

from race_pack_camera_variants import prefer_exact_camera_detail
from race_pack_opaque_duplicates import collapse_opaque_cutout_duplicates
from race_pack_terrain_variants import prefer_authored_terrain_detail


def _authenticate_opaque_textures(source, names, texture_bytes):
    """Grant TEX_EDGE exceptions only for authenticated fully opaque RGBA16."""
    report = []
    textures = {t['symbol']: t for t in source['textures']}
    for name in names:
        record = textures.get(name)
        if not record or record['format'] != 'rgba16':
            raise ValueError('Expected an authenticated RGBA16 terrain texture')
        pixels = texture_bytes(name)
        digest = hashlib.sha256(pixels).hexdigest()
        if (len(pixels) != record['width'] * record['height'] * 2 or
                digest != record['sha256'] or
                not all(pixels[i] & 1 for i in range(1, len(pixels), 2))):
            raise ValueError('Terrain texture differs or contains transparent pixels')
        report.append({'texture': name, 'sha256': digest,
                       'rgba16_texels': len(pixels) // 2, 'transparent_texels': 0})
    return report


def _remove_exact_render_duplicates(source, pairs):
    """Remove only identical oriented render faces in classified alternatives."""
    def oriented(triangle):
        points = [tuple(source['vertices'][v]['position']) for v in triangle['vertices']]
        return min(tuple(points[i:] + points[:i]) for i in range(3))

    target = {}
    for coarse, detail in pairs:
        target.setdefault(coarse, set()).update(
            oriented(t) for t in source['triangles'] if t['display_list'] == detail)
    kept, removed = [], []
    for index, triangle in enumerate(source['triangles']):
        if (triangle['display_list'] in target and
                oriented(triangle) in target[triangle['display_list']]):
            removed.append(index)
        else:
            kept.append(triangle)
    source['triangles'] = kept
    source['validation']['render_triangles'] = len(kept)
    return removed


def correct_dk_royal(source, roots, accepted_collision_triangles, texture_bytes):
    """Return corrected source and a staged audit without touching caller data.

    ``roots`` maps source leaves to sets of donor camera sections. Accepted
    collision faces are supplied by the normal donor filter, with original
    vertex references; the source's original contact list is preserved. Callers
    must apply their normal filter before native encoding. ``texture_bytes`` is
    a callback taking a texture symbol and returning extracted RGBA16 bytes.
    No walls, hazard, ramp, audio, route, or texture metadata is modified here.
    """
    recipe = json.loads(Path(__file__).with_name('race_pack_dk_royal.json').read_text())
    course = source['course']
    if course not in recipe['courses']:
        raise ValueError('This recipe supports DK Jungle Parkway and Royal Raceway only')
    current = deepcopy(source)
    reports = []
    for stage in recipe['courses'][course]:
        if stage['type'] == 'exact_camera':
            current, audit = prefer_exact_camera_detail(current, roots, accepted_collision_triangles)
            reports.append({'type': 'exact_camera', 'audit': audit})
            continue

        # Each stage's source indices refer to the preceding stage, not earlier
        # triangles with the same name. Keep only the latest ancestry marker.
        for triangle in current['triangles']:
            triangle.pop('terrain_variant_source_triangle', None)
        opaque = stage.get('opaque_textures', [])
        authentication = _authenticate_opaque_textures(current, opaque, texture_bytes)
        duplicate_audit = None
        if stage.get('collapse_opaque_duplicates'):
            current, duplicate_audit = collapse_opaque_cutout_duplicates(
                current, opaque_textures=opaque)
        pairs = stage['pairs']
        if stage.get('filter_absent_pairs'):
            known = {t['display_list'] for t in current['triangles']}
            pairs = [pair for pair in pairs if set(pair) <= known]
        current, audit = prefer_authored_terrain_detail(
            current, pairs, roots, accepted_collision_triangles,
            compatible_textures=stage['compatible_textures'],
            adjacent_physical_pairs=stage['adjacent_physical_pairs'],
            opaque_cutout_textures=opaque)
        removed = (_remove_exact_render_duplicates(current, pairs)
                   if stage['remove_exact_render_duplicates'] else [])
        reports.append({'type': 'terrain', 'audit': audit,
                        'opaque_texture_authentication': authentication,
                        'opaque_duplicate_audit': duplicate_audit,
                        'exact_render_duplicates_removed': removed})
    return current, {'course': course, 'stages': reports}
