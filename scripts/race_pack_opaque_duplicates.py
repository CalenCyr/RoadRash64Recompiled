"""Collapse proven equivalent opaque/cutout submissions of the same primitive.

This is an offline, explicitly opted-in conversion pass. The caller authenticates
the named source textures as entirely opaque. It preserves the nonculled variant
and never changes source collision, vertex attributes, or unrelated materials.
"""
import copy
from collections import defaultdict


def collapse_opaque_cutout_duplicates(before, *, opaque_textures=()):
    opaque_textures = set(opaque_textures)
    if any(not isinstance(t, str) or not t for t in opaque_textures):
        raise ValueError('Expected authenticated opaque texture names')
    allowed_modes = {'G_SHADE', 'G_SHADING_SMOOTH', 'G_ZBUFFER'}
    opaque_render = ['G_RM_AA_ZB_OPA_SURF', 'G_RM_AA_ZB_OPA_SURF2']
    cutout_render = ['G_RM_AA_ZB_TEX_EDGE', 'G_RM_AA_ZB_TEX_EDGE2']
    opaque_combine = ['G_CC_MODULATEIA', 'G_CC_MODULATEIA']
    cutout_combine = ['G_CC_MODULATEIDECALA', 'G_CC_MODULATEIDECALA']

    def variant(t):
        m = before['materials'][t['material']]
        if not m['texture_enabled'] or m['texture'] not in opaque_textures:
            return None
        if any(before['vertices'][i]['color_rgba'][3] != 255 for i in t['vertices']):
            return None
        modes = set(m['geometry_modes'])
        if (m['render'] == opaque_render and m['combine'] == opaque_combine
                and modes == allowed_modes | {'G_CULL_BACK'}):
            return 'opaque'
        if (m['render'] == cutout_render and m['combine'] == cutout_combine
                and modes == allowed_modes):
            return 'cutout'
        return None

    def primitive(t):
        values = [tuple(before['vertices'][i]['position'] +
                        before['vertices'][i]['texcoord_st'] +
                        before['vertices'][i]['color_rgba']) for i in t['vertices']]
        return min(tuple(values[i:] + values[:i]) for i in range(3))

    def same_material(a, b):
        ignored = {'id', 'render', 'combine', 'geometry_modes'}
        return ({k: v for k, v in a.items() if k not in ignored} ==
                {k: v for k, v in b.items() if k not in ignored})

    cutouts = defaultdict(list)
    for i, t in enumerate(before['triangles']):
        if variant(t) == 'cutout':
            cutouts[(t['display_list'], primitive(t))].append(i)
    removed = set()
    proof = []
    for i, t in enumerate(before['triangles']):
        if variant(t) != 'opaque':
            continue
        for j in cutouts[(t['display_list'], primitive(t))]:
            other = before['triangles'][j]
            if same_material(before['materials'][t['material']],
                             before['materials'][other['material']]):
                removed.add(i)
                proof.append(dict(removed_triangle=i, retained_triangle=j,
                                  leaf=t['display_list'], texture=before['materials'][t['material']]['texture']))
                break
    after = copy.deepcopy(before)
    after['triangles'] = [t for i, t in enumerate(after['triangles']) if i not in removed]
    after['validation']['render_triangles'] = len(after['triangles'])
    return after, dict(removed=proof, before_triangles=len(before['triangles']),
                       after_triangles=len(after['triangles']))
