"""Keep Luigi's detailed tunnel instead of drawing its distant chord too.

The source camera lists choose either C4C0 (five long tunnel panels) or 3FC0
(the curved, subdivided tunnel). Their end sections agree, but their interiors
do not lie on the same planes. Keeping both creates floating brick surfaces;
exact coplanar subtraction cannot resolve this authored LOD alternative.
All geometry is supplied by the user's ROM. No coordinates or replacement mesh
are stored here. The normal encoder rebuilds finite contact from the retained
mesh after this pass; old exploration contacts must not be reused.
"""

import copy


def correct_luigi_raceway(source, roots, accepted_collision_triangles):
    """Return a corrected source and audit, without changing authored contacts.

    Call after the existing camera-variant passes and before cell encoding and
    visible-terrain contact promotion. Fail closed if the donor identities,
    material family, endpoint correspondence or camera ownership have changed.
    """
    if source['course'] != 'luigi_raceway':
        raise ValueError('Luigi tunnel correction requires Luigi Raceway')
    prefix = 'd_course_luigi_raceway_packed_dl_'
    coarse_name, detail_name = prefix + 'C4C0', prefix + '3FC0'
    coarse = [t for t in source['triangles'] if t['display_list'] == coarse_name]
    detail = [t for t in source['triangles'] if t['display_list'] == detail_name]
    physical = [t for t in accepted_collision_triangles if t['display_list'] == detail_name]
    if len(coarse) != 10 or len(detail) != 60 or len(physical) != 24:
        raise ValueError('Luigi tunnel source topology changed')
    if (not roots.get(coarse_name) or not roots.get(detail_name)
            or roots[coarse_name] & roots[detail_name]):
        raise ValueError('Luigi tunnel meshes are not exclusive camera alternatives')
    if any(t['display_list'] == coarse_name for t in accepted_collision_triangles):
        raise ValueError('Luigi coarse tunnel unexpectedly owns authored collision')

    def positions(triangle):
        return tuple(tuple(source['vertices'][v]['position']) for v in triangle['vertices'])

    def material(triangle):
        value = source['materials'][triangle['material']]
        if not value['texture_enabled'] or any(
                'XLU' in mode or 'TEX_EDGE' in mode for mode in value['render']):
            raise ValueError('Luigi tunnel material is no longer opaque')
        return value['texture']

    if ({material(t) for t in coarse} != {'gLRTexture673C68'}
            or {material(t) for t in detail} != {'gLRTexture673C68', 'gLRTexture6735DC'}):
        raise ValueError('Luigi tunnel material family changed')
    detail_faces = {tuple(sorted(positions(t))) for t in detail}
    if not all(tuple(sorted(positions(t))) in detail_faces for t in physical):
        raise ValueError('Luigi authored contact no longer matches the detailed tunnel')
    endpoints = {p for t in coarse for p in positions(t)}
    detailed_vertices = {p for t in detail for p in positions(t)}
    differences = []
    for point in sorted(endpoints):
        heights = [abs(point[1] - other[1]) for other in detailed_vertices
                   if point[0] == other[0] and point[2] == other[2]]
        if not heights or min(heights) > 1:
            raise ValueError('Luigi tunnel end sections no longer correspond')
        differences.append(min(heights))
    if len(endpoints) != 13 or differences.count(1) != 2:
        raise ValueError('Luigi tunnel endpoint quantization changed')

    result = copy.deepcopy(source)
    result['triangles'] = [t for t in result['triangles'] if t['display_list'] != coarse_name]
    result['validation']['render_triangles'] = len(result['triangles'])
    return result, {
        'mode': 'detailed-authored-tunnel-camera-alternative',
        'removed_leaf': coarse_name,
        'retained_leaf': detail_name,
        'removed_triangles': len(coarse),
        'retained_triangles': len(detail),
        'retained_authored_contact_triangles': len(physical),
        'cooccurring_camera_roots': 0,
        'matching_endpoints': len(endpoints),
        'one_unit_endpoint_height_differences': differences.count(1),
        'authored_collision_unchanged': True,
        'requires_fresh_visible_terrain_contacts': True,
    }
