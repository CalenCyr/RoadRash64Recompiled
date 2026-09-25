"""Reproducible Yoshi Valley terrain/rail policy for user-ROM imports.

This module contains no donor vertices, texture pixels, or ROM bytes. The caller
extracts and authenticates the supported donor, supplies section-camera roots,
accepted collision faces, and its normal source-to-world transformation.
"""
import json
from pathlib import Path

from race_pack_low_rails import lower_selected_rails, lower_attached_rail_caps
from race_pack_terrain_variants import prefer_authored_terrain_detail


def correct_yoshi_valley(source, roots, accepted_collision_triangles,
                         walls, surfaces, transform):
    """Return source, wall/surface metadata and a bounded correction report.

    ``source`` must retain original collision-list indices, matching metadata.
    ``accepted_collision_triangles`` is the caller's normal donor collision
    filter result; final filtering after this function is still the caller's
    responsibility. Source rendering and original contact identities are kept
    separate throughout. Exploration contact is rebuilt from final encoded
    geometry by the exporter, never copied from discarded terrain fragments.
    """
    if source['course'] != 'yoshi_valley':
        raise ValueError('Yoshi Valley recipe only')
    recipe = json.loads(Path(__file__).with_name('race_pack_yoshi_valley.json').read_text())
    first = recipe['first_terrain']
    current, initial_report = prefer_authored_terrain_detail(
        source, first['pairs'], roots, accepted_collision_triangles,
        compatible_textures=first['compatible_textures'])

    def oriented(triangle):
        points = [tuple(current['vertices'][v]['position']) for v in triangle['vertices']]
        return min(tuple(points[i:] + points[:i]) for i in range(3))

    # The original donor also repeats exactly identical physical triangles in
    # alternate visual leaves. Only the camera-classified duplicate is removed;
    # its original collision primitive and unlisted physical faces stay intact.
    targets = {}
    for coarse, detail in first['pairs']:
        targets.setdefault(coarse, set()).update(
            oriented(t) for t in current['triangles'] if t['display_list'] == detail)
    duplicates = 0
    kept = []
    for triangle in current['triangles']:
        if (triangle['display_list'] in targets and
                oriented(triangle) in targets[triangle['display_list']]):
            duplicates += 1
            continue
        triangle.pop('terrain_variant_source_triangle', None)
        kept.append(triangle)
    current['triangles'] = kept
    current['validation']['render_triangles'] = len(kept)

    second = recipe['residual_terrain']
    known = {t['display_list'] for t in current['triangles']}
    pairs = [pair for pair in second['pairs'] if set(pair) <= known]
    current, residual_report = prefer_authored_terrain_detail(
        current, pairs, roots, accepted_collision_triangles,
        compatible_textures=second['compatible_textures'],
        adjacent_physical_pairs=second['adjacent_physical_pairs'])

    # Metadata's generic rectangular walls include signs and cliff faces too.
    # Authenticate the material on both visible triangles of a proposed rail
    # before allowing it to be shortened. Do not lower every rectangular wall.
    lookup = {t['id']: t for t in walls['triangles']}
    names = set(recipe['rail_textures'])
    visible = {}
    for triangle in source['triangles']:
        key = (triangle['display_list'], tuple(sorted(triangle['vertices'])))
        visible.setdefault(key, set()).add(source['materials'][triangle['material']]['texture'])
    selected = set()
    for rail in walls['rails']:
        textures = set()
        for index in rail['triangle_ids']:
            triangle = source['collision_triangles'][lookup[index]['source']['collision_triangle']]
            key = (triangle['display_list'], tuple(sorted(triangle['vertices'])))
            textures.update(visible.get(key, ()))
        if textures and textures <= names:
            selected.add(rail['id'])
    if len(selected) != recipe['expected_rail_panels']:
        raise ValueError('Yoshi guardrail source does not match the authenticated recipe')
    lowered, walls_out, surfaces_out, rail_report = lower_selected_rails(
        current, walls, surfaces, transform, selected, recipe['rail_height_world'])
    lowered, cap_report = lower_attached_rail_caps(current, lowered)
    if cap_report['cap_triangles']:
        raise ValueError('Unexpected Yoshi rail caps; imported donor needs review')
    return lowered, walls_out, surfaces_out, {
        'initial_terrain': initial_report,
        'duplicate_render_triangles_removed': duplicates,
        'residual_terrain': residual_report,
        'rails': rail_report,
        'caps': cap_report,
    }
