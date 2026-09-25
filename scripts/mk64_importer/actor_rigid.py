"""Generate original rigid hazards and preserve the established atlas history."""

import copy
import math
from .common import require
from .route_assembly import LAYOUT


def source_floor(source, x, z):
    """The donor train's shared height uses the highest suitable authored floor."""
    hits = []
    for triangle in source["collision_triangles"]:
        a, b, c = (source["vertices"][i]["position"] for i in triangle["vertices"])
        u = [b[i] - a[i] for i in range(3)]
        v = [c[i] - a[i] for i in range(3)]
        normal = [u[1] * v[2] - u[2] * v[1], u[2] * v[0] - u[0] * v[2], u[0] * v[1] - u[1] * v[0]]
        if not normal[1] or abs(normal[1]) < max(abs(normal[0]), abs(normal[2])):
            continue
        det = (b[2] - c[2]) * (a[0] - c[0]) + (c[0] - b[0]) * (a[2] - c[2])
        if not det:
            continue
        s = ((b[2] - c[2]) * (x - c[0]) + (c[0] - b[0]) * (z - c[2])) / det
        t = ((c[2] - a[2]) * (x - c[0]) + (a[0] - c[0]) * (z - c[2])) / det
        if min(s, t, 1 - s - t) < -1e-9:
            continue
        height = s * a[1] + t * b[1] + (1 - s - t) * c[1]
        if height <= 2000:
            hits.append((height, triangle["display_list"]))
    require(hits, "Missing authored train ground")
    return max(hits)


def build(sources, models, placement):
    rigid, runtime = placement["rigid"], placement["runtime"]
    stages = LAYOUT["stages"]
    train_y, train_ground = source_floor(
        sources["kalimari_desert"], *runtime["kalimari_desert"]["points_xz"][0]
    )
    result = {}
    for slug, source in sources.items():
        center = [
            (
                min(v["position"][i] for v in source["vertices"])
                + max(v["position"][i] for v in source["vertices"])
            )
            / 2
            for i in (0, 2)
        ]
        translation = [v / 4 for v in stages[0]["placements"][slug]]

        def world(point):
            x, y, z = point
            return [
                (x - center[0]) * 0.05 + translation[0],
                -(z - center[1]) * 0.05 + translation[1],
                y * 0.05 + translation[2],
            ]

        def definition(kind, model, position, **extra):
            bounds = models[model]["collision_model_bounds"]
            return (
                dict(
                    id=0,
                    kind=kind,
                    model=model,
                    subtype=0,
                    phase=0,
                    path=0,
                    node=0,
                    position=position,
                    offset=copy.deepcopy(bounds["offset"]),
                    half_extent=copy.deepcopy(bounds["half_extent"]),
                    scale=1,
                    speed=0,
                    minimum_height=-100,
                    lane=0,
                )
                | extra
            )

        definitions = []
        paths = []
        adaptations = []
        if slug == "bowsers_castle":
            for spawn in rigid["thwomp_spawn_tables"]["gThwompSpawns100CCExtra"]:
                definitions.append(
                    definition(
                        "thwomp",
                        0,
                        world(spawn["source_position"]),
                        subtype=spawn["subtype"],
                        phase=spawn["phase"],
                        scale=1.5 if spawn["subtype"] == 6 else 1,
                    )
                )
            adaptations = [
                "Original100cc/Extra11-spawn table is shared across all Road Rash game modes.",
                "Original subtype6 scale1.5 retained; source camera-trigger gates are adapted by runtime to shared race state.",
            ]
        elif slug == "choco_mountain":
            for spawn in rigid["falling_rocks"]:
                definitions.append(
                    definition(
                        "rock",
                        6,
                        world(spawn["source_position"]),
                        phase=spawn["id"],
                        minimum_height=world([0, -80, 0])[2],
                    )
                )
            adaptations = [
                "Source render_courses.c sets reset plane D_8015F8E4=-80 for Choco; copied exactly before unit conversion.",
                "Native Road Rash sphere/contact response is an adaptation, not the original kart hit effect.",
            ]
        elif slug == "toads_turnpike":
            source_path = runtime[slug]
            paths = [
                {k: [world(v) for v in source_path[k]] for k in ("left", "right")}
                | {"points": [world(v) for v in source_path["center"]]}
            ]
            for family in rigid["traffic"]["families"]:
                for index in range(7):
                    node = (
                        index * source_path["count"] // 7 + family["path_offset"]
                    ) % source_path["count"]
                    lane = index % 3
                    model = family["models"][index % len(family["models"])]
                    definitions.append(
                        definition(
                            "traffic",
                            model,
                            paths[0]["points"][node],
                            node=node,
                            subtype=lane,
                            lane=(lane - 1) * 0.6,
                            speed=(5.0 if lane == 2 else 10 / 3) * 0.05,
                        )
                    )
            adaptations = [
                "Seven vehicles per family retained from race mode; deterministic i%3 lane selection borrowed from source time trials.",
                "Source100cc speeds:5 or10/3 source units per update; original box-truck paint variants cycle deterministically.",
                "Forward non-mirror source path shared across all Road Rash game modes.",
            ]
        elif slug == "kalimari_desert":
            source_path = runtime[slug]
            points = [world([x, train_y, z]) for x, z in source_path["points_xz"]]
            paths = [dict(points=points)]
            for train in range(2):
                node = (train * source_path["count"] // 2 + 160) % source_path["count"]
                for part in range(7):
                    node = (node + (3 if part == 5 else 4)) % source_path["count"]
                    model = 15 if part < 5 else 14 if part == 5 else 13
                    definitions.append(
                        definition(
                            "train", model, points[node], node=node, subtype=part, speed=0.25
                        )
                    )
            adaptations = [
                "Two full seven-part trains from source single-player mode are shared across all Road Rash game modes.",
                "Original source shared train height is "
                + str(train_y)
                + " from "
                + train_ground
                + ".",
            ]
        for index, entry in enumerate(definitions):
            entry["id"] = index
        # Reproduce each historical affine conversion. Rounding through the
        # accepted scale sequence also keeps generated network pack data stable.
        for before, after in zip(stages, stages[1:]):
            factor = after["scale"] / before["scale"]
            previous = [v / 4 for v in before["placements"][slug]]
            current = [v / 4 for v in after["placements"][slug]]

            def point(value):
                return [(value[i] - previous[i]) * factor + current[i] for i in range(3)]

            for entry in definitions:
                entry["position"] = point(entry["position"])
                for key in ("offset", "half_extent"):
                    entry[key] = [v * factor for v in entry[key]]
                for key in ("speed", "minimum_height"):
                    entry[key] *= factor
            for path in paths:
                for key in ("points", "left", "right"):
                    if key in path:
                        path[key] = [point(v) for v in path[key]]
        require(
            len(definitions) <= 128 and all(5 <= len(p["points"]) <= 4096 for p in paths),
            "Rigid actor capacity",
        )
        require(
            all(
                math.isfinite(v)
                for d in definitions
                for k in ("position", "offset", "half_extent")
                for v in d[k]
            ),
            "Nonfinite rigid actor",
        )
        result[slug] = dict(
            format="rr64-course-hazards",
            version=1,
            course=slug,
            coordinate_space="RR rider world; atlas already included",
            definitions=definitions,
            paths=paths,
            source_commit=rigid["source_commit"],
            adaptations=adaptations,
            private_assets_not_distributable=True,
            source_to_world_scale=stages[-1]["scale"],
        )
    return result
