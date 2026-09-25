"""Fit ROM paths and reproduce the established scale/probe route pipeline.

Only authored atlas translations and conversion rules are installed. Every
curve, height, lane margin and recovery mask is calculated from the selected
ROM and freshly encoded native collision cells. The headless contact helper
runs the native fresh/cached floor queries; it does not launch the game.
"""

from __future__ import annotations
import bisect, csv, hashlib, json, math, subprocess
from pathlib import Path
from .common import f32, require
from .route import encode

LAYOUT = json.loads(Path(__file__).with_name("route_layout.json").read_text())


def mix(a, b, t):
    return [a[i] + (b[i] - a[i]) * t for i in range(len(a))]


def bezier(curve, t):
    return [
        (1 - t) ** 2 * curve[0][i] + 2 * (1 - t) * t * curve[1][i] + t * t * curve[2][i]
        for i in range(len(curve[0]))
    ]


def digest(data):
    return hashlib.sha256(data).hexdigest()


class PathSampler:
    def __init__(self, points):
        self.points = []
        for point in points:
            if not self.points or math.dist(point[:2], self.points[-1][:2]) > 0.00001:
                self.points.append(point)
        if len(self.points) > 1 and math.dist(self.points[0][:2], self.points[-1][:2]) < 0.00001:
            self.points.pop()
        require(len(self.points) >= 3, "Degenerate source race path")
        self.cumulative = [0.0]
        for a, b in zip(self.points, self.points[1:] + self.points[:1]):
            self.cumulative.append(self.cumulative[-1] + math.dist(a[:2], b[:2]))
        self.period = self.cumulative[-1]

    def at(self, s):
        s %= self.period
        i = min(len(self.points) - 1, bisect.bisect_right(self.cumulative, s) - 1)
        t = (s - self.cumulative[i]) / (self.cumulative[i + 1] - self.cumulative[i])
        return mix(self.points[i], self.points[(i + 1) % len(self.points)], t)


def initial_route(source, laps=3):
    """Fit at the original .05 scale before any binary32 scale changes."""
    slug = source["course"]
    scale = LAYOUT["stages"][0]["scale"]
    translation = [v / 4 for v in LAYOUT["stages"][0]["placements"][slug]]
    center = [
        (
            min(v["position"][i] for v in source["vertices"])
            + max(v["position"][i] for v in source["vertices"])
        )
        / 2
        for i in (0, 2)
    ]
    points = [
        [
            (v["position"][0] - center[0]) * scale + translation[0],
            -(v["position"][2] - center[1]) * scale + translation[1],
            v["position"][1] * scale,
        ]
        for v in source["path"]
    ]
    path = PathSampler(points)
    finish = path.at(0)
    tangent = [path.at(0.5)[i] - path.at(-0.5)[i] for i in range(2)]
    length = math.hypot(*tangent)
    require(length >= 0.001, "Degenerate finish tangent")
    normal = [v / length for v in tangent]
    spacing = max(2.0, path.period / 950)
    chosen = None
    for half in (12.0, 9.0, 6.0, 4.0, 2.0):
        middle = -half / 3
        controls_s = [middle]
        s = middle + half * 2
        while s < path.period + middle - half * 2 - 1e-5:
            controls_s.append(s)
            s += spacing
        controls_s.append(path.period + middle - half * 2)
        controls = [path.at(s) for s in controls_s]
        curves = []
        arcs = []
        for i, point in enumerate(controls):
            previous = controls_s[i - 1] if i else controls_s[-1] - path.period
            following = controls_s[i + 1] if i + 1 < len(controls) else controls_s[0] + path.period
            arcs.append(
                [(previous + controls_s[i]) * 0.5, controls_s[i], (controls_s[i] + following) * 0.5]
            )
            a = mix(controls[i - 1], point, 0.5)
            b = mix(point, controls[(i + 1) % len(controls)], 0.5)
            curves.append([[f32(v) for v in p[:2]] for p in (a, point, b)])

        def signed(t):
            return sum((bezier(curves[0], t)[i] - finish[i]) * normal[i] for i in range(2))

        if not signed(0) < 0 < signed(1):
            continue

        def parameter(offset):
            low, high = 0.0, 1.0
            for _ in range(48):
                mid = (low + high) * 0.5
                if signed(mid) < offset:
                    low = mid
                else:
                    high = mid
            return f32((low + high) * 0.5)

        finish_t = parameter(0)
        start_offset = max(-8.0, signed(0) * 0.7)
        start_t = parameter(start_offset)
        maximum = 0.0
        for curve, arc in zip(curves, arcs):
            for step in range(9):
                t = step / 8
                u = 1 - t
                s = u * u * arc[0] + 2 * u * t * arc[1] + t * t * arc[2]
                maximum = max(maximum, math.dist(bezier(curve, t), path.at(s)[:2]))
        if maximum <= 0.55:
            chosen = (curves, arcs, start_t, finish_t, start_offset, maximum, half)
            break
    require(chosen is not None, "Cannot fit a continuous finish approach to this source route")
    curves, arcs, start_t, finish_t, start_offset, maximum, half = chosen
    blob, native = encode(curves, start_t, finish_t, laps)
    heights = []
    for arc in arcs:
        heights.extend([f32(path.at(arc[0])[2]), f32(path.at(arc[1])[2])])
    heights.extend([heights[0], heights[1], heights[2], heights[2], heights[2]])
    grid = []
    for rank in range(14):
        s = -8.0 - (rank // 2) * 3.0
        mid = path.at(s)
        before, after = path.at(s - 0.1), path.at(s + 0.1)
        heading = math.atan2(after[1] - before[1], after[0] - before[0]) % (2 * math.pi)
        lateral = -0.6 if rank % 2 == 0 else 0.6
        grid.append(
            {
                "rank": rank,
                "source_arc": s,
                "center": mid,
                "position": [
                    f32(mid[0] - math.sin(heading) * lateral),
                    f32(mid[1] + math.cos(heading) * lateral),
                    f32(mid[2]),
                ],
                "heading": f32(heading),
            }
        )
    route = {
        "format": "rr64-private-course-route",
        "version": 2,
        "course": slug,
        "binary": "route.bin",
        "byte_order": "big-endian",
        "binary_sha256": digest(blob),
        "curve_count": len(curves),
        "laps": laps,
        "native": native,
        "start_parameter": start_t,
        "lane_half_width": 1.5,
        "curves": curves,
        "curve_source_arcs": arcs,
        "record_heights": heights,
        "source_grid": grid,
        "geometry": {
            "finish_source": "MK64 primary path point zero / gPathStartZ",
            "finish_plane_origin": finish,
            "finish_plane_normal": normal,
            "source_path_horizontal_length": path.period,
            "maximum_corresponding_path_deviation": maximum,
            "approach_half_span": half,
            "start_plane_distance": start_offset,
        },
        "coverage": {
            "terrain_contact_verified": False,
            "grid_contact_verified": False,
            "branch_policy": "Primary closed path with ordinary free riding between waypoints.",
            "stacked_roads": "Record heights retained for height-aware recovery.",
        },
    }
    return route, blob


def rescale(route, blob, stage):
    old = LAYOUT["stages"][stage - 1]
    new = LAYOUT["stages"][stage]
    slug = route["course"]
    factor = new["scale"] / old["scale"]
    before = [v / 4 for v in old["placements"][slug]]
    after = [v / 4 for v in new["placements"][slug]]

    def point(p):
        return [(p[i] - before[i]) * factor + after[i] for i in range(len(p))]

    route["curves"] = [[[f32(v) for v in point(p)] for p in curve] for curve in route["curves"]]
    route["record_heights"] = [f32(v * factor) for v in route["record_heights"]]
    route["curve_source_arcs"] = [[v * factor for v in arc] for arc in route["curve_source_arcs"]]
    for p in route["source_grid"]:
        p["source_arc"] *= factor
        p["center"] = point(p["center"])
        p["position"] = [f32(v) for v in point(p["position"])]
    route["lane_half_width"] *= factor
    if "curve_lane_half_widths" in route:
        route["curve_lane_half_widths"] = [
            math.floor(v / 0.15 * factor + 1e-6) * 0.15 for v in route["curve_lane_half_widths"]
        ]
    route["geometry"]["finish_plane_origin"] = point(route["geometry"]["finish_plane_origin"])
    for key in (
        "source_path_horizontal_length",
        "maximum_corresponding_path_deviation",
        "approach_half_span",
        "start_plane_distance",
    ):
        route["geometry"][key] *= factor
    updated, native = encode(
        route["curves"],
        route["start_parameter"],
        route["native"]["finish_parameter"],
        route["laps"],
        route["lane_half_width"],
        route["lane_half_width"],
    )
    result = bytearray(updated)
    require(len(result) == len(blob), "Scaling changed route topology")
    for offset in range(0, len(result), 16):
        for side in (4, 5):
            result[offset + side] = min(255, max(1, math.floor(blob[offset + side] * factor)))
    route["native"] = native
    route["binary_sha256"] = digest(result)
    return bytes(result)


def curve_sample(route, index, step, denominator):
    curve = route["curves"][index]
    t = step / denominator
    u = 1 - t
    x, z = [sum(w * p[k] for w, p in zip([u * u, 2 * u * t, t * t], curve)) for k in range(2)]
    h = sum(
        w * v
        for w, v in zip(
            [u * u, 2 * u * t, t * t], route["record_heights"][2 * index : 2 * index + 3]
        )
    )
    dx, dz = [
        2 * (u * (curve[1][k] - curve[0][k]) + t * (curve[2][k] - curve[1][k])) for k in range(2)
    ]
    length = math.hypot(dx, dz)
    require(length > 0, "Degenerate route tangent")
    return x, z, h, -dz / length, dx / length


def probes(route, blob, widths=False):
    result = []
    for i in range(len(route["curves"])):
        for step in range(17 if widths else 33):
            x, z, h, lx, lz = curve_sample(route, i, step, 16 if widths else 32)
            if widths:
                for unit in range(1, 11):
                    for side in (-1, 1):
                        result.append(
                            (
                                i,
                                unit * 100 + step + (5000 if side > 0 else 0),
                                x + lx * side * unit * 0.15,
                                z + lz * side * unit * 0.15,
                                h,
                            )
                        )
            else:
                result.append((i, step, x, z, h))
                if step % 4 == 0:
                    for side in (-1, 1):
                        offset = side * min(blob[(2 * i + j) * 16 + 4] for j in range(3)) * 0.15
                        result.append(
                            (
                                i,
                                100 + step + (200 if side > 0 else 0),
                                x + lx * offset,
                                z + lz * offset,
                                h,
                            )
                        )
    if not widths:
        for p in route["source_grid"]:
            result.append((100000 + p["rank"], 0, *p["position"]))
    return result


def run_probes(helper, cells, points, directory, label, cancel=None, course_active=False):
    """cells is {native index: bytes}; request paths are relative and space-free."""
    directory = Path(directory)
    directory.mkdir(parents=True, exist_ok=True)
    lines = [str(len(cells))]
    for index, data in sorted(cells.items()):
        filename = f"cell-{index}.bin"
        (directory / filename).write_bytes(data)
        lines.append(f"{index} {filename}")
    request = directory / (label + ".txt")
    lines.append(str(len(points)))
    lines.extend(" ".join(format(v, ".9g") for v in p) for p in points)
    request.write_text("\n".join(lines) + "\n")
    if cancel:
        cancel()
    # The helper always probes every point in both passes. Its default CSV
    # records failures and cache differences only, keeping large imports small.
    arguments = [str(Path(helper).resolve()), request.name]
    if course_active:
        arguments.append("--course-active")
    run = subprocess.run(
        arguments,
        cwd=directory,
        capture_output=True,
        text=True,
        timeout=120,
        creationflags=getattr(subprocess, "CREATE_NO_WINDOW", 0),
    )
    require(run.returncode in (0, 3), "Native course support probe failed: " + run.stderr[-1000:])
    if cancel:
        cancel()
    report = json.loads(run.stdout)
    with Path(str(request) + ".failures.csv").open(newline="") as stream:
        rows = list(csv.DictReader(stream))
    return rows, report


def apply_initial_widths(route, blob, rows):
    count = len(route["curves"])
    allowed = [[True] * 10 for _ in range(count)]
    for row in rows:
        if int(row["pass"]) != 0:
            continue
        if row["hit"] == "0" or float(row["error"]) > 1:
            allowed[int(row["curve"])][(int(row["step"]) % 5000) // 100 - 1] = False
    widths = []
    for values in allowed:
        width = 0
        for unit, good in enumerate(values):
            if not good:
                break
            width = unit + 1
        widths.append(max(1, width))
    result = bytearray(blob)
    for i, width in enumerate(widths):
        for side in (4, 5):
            result[2 * i * 16 + side] = min(width, widths[i - 1])
            result[(2 * i + 1) * 16 + side] = width
    for i in range(3):
        result[(2 * count + i) * 16 + 4 : (2 * count + i) * 16 + 6] = result[
            i * 16 + 4 : i * 16 + 6
        ]
    route["curve_lane_half_widths"] = [v * 0.15 for v in widths]
    route["binary_sha256"] = digest(result)
    return bytes(result)


def apply_support(route, rows):
    count = len(route["curves"])
    bad = set()
    points = {}
    for row in rows:
        index = int(row["curve"])
        if index >= count:
            continue
        if row["hit"] == "0" or float(row["error"]) > 1:
            bad.add(index)
        points.setdefault((index, int(row["step"])), {})[int(row["pass"])] = float(row["ground"])
    for (index, _), values in points.items():
        if len(values) == 2 and abs(values[0] - values[1]) > 0.1251:
            bad.add(index)
    support = route.setdefault("recovery_support", [1] * count)
    for i in bad:
        for delta in (-1, 0, 1):
            support[(i + delta) % count] = 0
    require(any(support), "No safe reset anchors remain")
    return sorted(bad)


def root_membership(source, display_lists):
    """Map reached display-list names to the source roots that call them.

    Only the ROM command graph is traversed. This authenticates mutually
    exclusive camera variants without importing source checkout declarations.
    """
    membership = {}
    for root in source["roots"]:
        # Runtime extras intentionally aggregate otherwise exclusive camera
        # alternatives. They cannot be used as camera co-occurrence evidence.
        if root["kind"] != "section-direction":
            continue
        name = root["name"]
        pending = [name]
        visited = set()
        while pending:
            child = pending.pop()
            if child in visited:
                continue
            visited.add(child)
            require(len(visited) < 100000, "Course display-list graph is unbounded")
            membership.setdefault(child, set()).add(name)
            commands = (
                display_lists.declaration(child)
                if hasattr(display_lists, "declaration")
                else display_lists[child]
            )
            for command, args in commands:
                if command in ("gsSPDisplayList", "gsSPBranchList"):
                    require(len(args) == 1, "Invalid display-list child")
                    pending.append(args[0])
                if command in ("gsSPEndDisplayList", "gsSPBranchList"):
                    break
    return membership


def historical_cell_provider(source, texture_dir, roots, legacy_source=None, cancel=None):
    """Rebuild the four accepted contact-probe stages from decoded ROM data.

    Mario's legacy pipe is supplied by extract_course(include_legacy_pipe=True)
    solely for the first two probe stages. It is never installed as terrain.
    No previous cell files, lane widths, or recovery masks are read.
    """
    from .baseline import _mario_variants
    from .terrain import encode_source

    if source["course"] == "mario_raceway":
        require(legacy_source is not None, "Mario historical contact source is missing")

    def provide(stage):
        require(0 <= stage < 4, "Invalid historical contact stage")
        if cancel:
            cancel()
        selected = legacy_source if stage < 2 and legacy_source is not None else source
        if stage == 3:
            selected = _mario_variants(source, roots)[0]
        spec = LAYOUT["stages"][stage]
        placement = {"terrain_translation": spec["placements"][source["course"]]}
        _, encoded = encode_source(
            selected, texture_dir, {}, placement, spec["scale"], deduplicate=False
        )
        if cancel:
            cancel()
        return {int(Path(name).stem.split("-")[1]): data for name, data in encoded.items()}

    return provide


def correct_final_grid(source, route):
    """Retain Koopa Beach's verified source-grid correction after final scaling.

    This course keeps the original GP stagger rather than the generic fitted
    curve grid. Heights are calculated from the ROM collision triangles, never
    taken from a stored converted route.
    """
    if source["course"] != "koopa_troopa_beach":
        return
    spec = LAYOUT["stages"][-1]
    scale = spec["scale"]
    translation = [v / 4 for v in spec["placements"][source["course"]]]
    center = [
        (
            min(v["position"][i] for v in source["vertices"])
            + max(v["position"][i] for v in source["vertices"])
        )
        / 2
        for i in (0, 2)
    ]

    def point(value):
        return [
            f32((value[0] - center[0]) * scale + translation[0]),
            f32(-(value[2] - center[1]) * scale + translation[1]),
            f32(value[1] * scale + translation[2]),
        ]

    floors = []
    for triangle in source["collision_triangles"]:
        vertices = [point(source["vertices"][i]["position"]) for i in triangle["vertices"]]
        a, b, c = vertices
        u = [b[k] - a[k] for k in range(3)]
        v = [c[k] - a[k] for k in range(3)]
        normal = [u[1] * v[2] - u[2] * v[1], u[2] * v[0] - u[0] * v[2], u[0] * v[1] - u[1] * v[0]]
        if abs(normal[2]) >= max(abs(normal[0]), abs(normal[1])) and abs(normal[2]) > 1e-6:
            floors.append(vertices)
    origin = source["path"][0]["position"]
    grid = []
    for rank in range(14):
        source_position = [
            origin[0] + (20 if rank % 2 == 0 else -20),
            origin[1],
            origin[2] + 30 + 20 * rank,
        ]
        position = point(source_position)
        candidates = []
        for a, b, c in floors:
            denominator = (b[1] - c[1]) * (a[0] - c[0]) + (c[0] - b[0]) * (a[1] - c[1])
            if abs(denominator) < 1e-12:
                continue
            u = (
                (b[1] - c[1]) * (position[0] - c[0]) + (c[0] - b[0]) * (position[1] - c[1])
            ) / denominator
            v = (
                (c[1] - a[1]) * (position[0] - c[0]) + (a[0] - c[0]) * (position[1] - c[1])
            ) / denominator
            weights = [u, v, 1 - u - v]
            if min(weights) >= -1e-7:
                height = sum(weights[k] * vertex[2] for k, vertex in enumerate((a, b, c)))
                if height <= (origin[1] + 50) * scale + translation[2]:
                    candidates.append(height)
        require(candidates, "Koopa starting slot has no source floor")
        position[2] = f32(max(candidates))
        grid.append(
            {
                "rank": rank,
                "position": position,
                "heading": f32(math.pi / 2),
                "source_position": source_position,
                "source_arc": -scale * (30 + 20 * rank),
                "center": point([origin[0], source_position[1], source_position[2]]),
                "source_grid_policy": "MK64 GP slots0-7 exact X/Z; slots8-13 extend original stagger; source floor height",
            }
        )
    route["source_grid"] = grid
    route["grid_provenance"] = {
        "source": "src/spawn_players.c:904-914, spawn_player:76-131",
        "first_eight": "original GP order before character/rank permutation",
        "extra_six": "extend alternating columns and +20 source-Z stagger",
        "source_yaw": 32768,
        "native_heading_radians": math.pi / 2,
        "height": "highest source dominant-axis floor below source path0 height +50; native contact independently checked",
    }


def assemble_route(
    source, cell_provider, helper, work_dir, geometry_sha256="", cancel=None, progress=None
):
    """Return (route metadata, bytes).

    cell_provider(stage_index) must rebuild that stage's native collision cells
    from local ROM source and return {cell_index: bytes}; no stored masks are
    accepted. Stages zero through three retain the established probe policy.
    The final enlarged stage retains these exclusions, as the accepted build
    does, rather than treating scale-amplified slope height error as a new gap.
    """
    route, blob = initial_route(source)
    directory = Path(work_dir)
    audit = []
    for stage, spec in enumerate(LAYOUT["stages"]):
        if cancel:
            cancel()
        if progress:
            progress(f'Checking {source["course"].replace("_"," ")} route support ({stage+1}/5)')
        if stage:
            blob = rescale(route, blob, stage)
        if stage == 4:
            break
        cells = cell_provider(stage)
        folder = directory / f"stage-{stage}"
        if stage == 0:
            rows, report = run_probes(
                helper, cells, probes(route, blob, True), folder, "lane-widths", cancel
            )
            blob = apply_initial_widths(route, blob, rows)
        if stage == 1 and route["course"] == "rainbow_road":
            # Original quantized-floor edge correction. The 24-unit corridor
            # misses three lane-margin points here; 23 stays on native floor.
            updated = bytearray(blob)
            for record in (250, 251, 252):
                for side in (4, 5):
                    updated[record * 16 + side] = min(updated[record * 16 + side], 23)
            route["curve_lane_half_widths"][125] = min(
                route["curve_lane_half_widths"][125], 23 * 0.15
            )
            blob = bytes(updated)
        rows, report = run_probes(
            helper, cells, probes(route, blob), folder, "support", cancel, course_active=stage > 0
        )
        grid_misses = {
            int(row["curve"])
            for row in rows
            if int(row["pass"]) == 0 and int(row["curve"]) >= 100000 and row["hit"] == "0"
        }
        bad = apply_support(route, rows)
        audit.append(
            {
                "scale": spec["scale"],
                "observed_unsupported_curves": bad,
                "grid_hits": 14 - len(grid_misses),
            }
        )
    route["binary_sha256"] = digest(blob)
    route["source_geometry_sha256"] = geometry_sha256
    route["coverage"]["recovery_support"] = {
        "purpose": "Recovery only; normal driving remains free.",
        "derived_from_generated_native_cells": True,
        "stages": audit,
        "excluded_with_one_curve_guard": [
            i for i, v in enumerate(route["recovery_support"]) if not v
        ],
        "final_scale_policy": "Retain established exclusions; enlarged slope height differences do not create new barriers.",
    }
    correct_final_grid(source, route)
    return route, blob
