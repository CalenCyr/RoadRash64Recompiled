"""ROM-derived actor placements; the bundled helper evaluates original arithmetic.

No spawn/path arrays ship here. Address recipes read the user's authenticated
cartridge; importing requires neither a compiler nor a network connection.
"""

from pathlib import Path
import json, struct, subprocess
from .common import require
from . import SOURCE_REFERENCE_COMMIT

LAYOUT = json.loads(Path(__file__).with_name("actor_placement_layout.json").read_text())
ATLAS = json.loads(Path(__file__).with_name("route_layout.json").read_text())["stages"][0]


class Placement:
    def __init__(self, donor, sources, helper):
        self.donor = donor
        self.sources = sources
        self._sources = {}
        self._centers = {}
        self.helper = Path(helper)
        require(self.helper.is_file(), "The bundled course motion helper is missing")

    def source(self, slug):
        if slug not in self._sources:
            value = self.sources[slug]
            self._sources[slug] = (
                json.loads(Path(value).read_text()) if isinstance(value, (str, Path)) else value
            )
        return self._sources[slug]

    def raw(self, symbol):
        info = LAYOUT["symbols"][symbol]
        if "rom_offset" in info:
            start = info["rom_offset"]
            return self.donor.rom[start : start + info["size"]]
        return self.donor.read(info["course"], info["address"], info["size"])

    def table(self, symbol):
        stride = LAYOUT["symbols"][symbol]["stride"]
        return [list(row) for row in struct.iter_unpack(">" + str(stride) + "h", self.raw(symbol))]

    def position(self, name):
        result = []
        for spec in LAYOUT["positions"][name]:
            kind = spec["type"]
            if kind == "zero":
                value = 0
            elif kind == "f64":
                value = struct.unpack_from(">d", self.donor.rom, spec["rom_offset"])[0]
            elif kind == "mips-s16":
                word = struct.unpack_from(">I", self.donor.rom, spec["instruction"])[0]
                require(word >> 26 in (8, 9), "Spawn immediate instruction changed")
                value = struct.unpack(">h", struct.pack(">H", word & 65535))[0]
            else:
                high = struct.unpack_from(">I", self.donor.rom, spec["high_instruction"])[0]
                require(high >> 26 == 15, "Spawn upper immediate changed")
                bits = (high & 65535) << 16
                if "low_instruction" in spec:
                    low = struct.unpack_from(">I", self.donor.rom, spec["low_instruction"])[0]
                    require(
                        low >> 26 == 13 and ((low >> 21) & 31) == ((high >> 16) & 31),
                        "Spawn lower immediate changed",
                    )
                    bits |= low & 65535
                value = (
                    struct.unpack(">d", struct.pack(">Q", bits << 32))[0]
                    if kind == "mips-f64-high"
                    else struct.unpack(">f", struct.pack(">I", bits))[0]
                )
            result.append(value)
        return result

    def path(self, slug, symbol):
        return [row["position"] + [row["section"]] for row in self.source(slug)["paths"][symbol]]

    def spline(self, symbol):
        raw = self.raw(symbol)
        count = struct.unpack_from(">h", raw)[0]
        controls = [
            dict(offset_source=list(row[:3]), velocity=row[3])
            for row in struct.iter_unpack(">4h", raw[2:])
        ]
        require(4 <= count < len(controls) <= 64, "Invalid source spline extent")
        return dict(
            symbol=symbol,
            control_count=count,
            controls=controls,
            source="src/data/some_data.c",
            requires_original_spline_evaluation=True,
        )

    def run(self, mode, rows, parameter=0):
        header = (
            f"spline {parameter} {len(rows)}"
            if mode == "spline"
            else f"{mode} {len(rows)} {parameter}"
        )
        text = header + "\n" + "\n".join(" ".join(str(v) for v in row) for row in rows) + "\n"
        completed = subprocess.run(
            [str(self.helper)],
            input=text,
            capture_output=True,
            text=True,
            timeout=45,
            check=False,
            creationflags=getattr(subprocess, "CREATE_NO_WINDOW", 0),
        )
        require(
            completed.returncode == 0,
            "Course motion conversion failed: " + str(completed.returncode),
        )
        cast = float if mode == "spline" else int
        result = [[cast(v) for v in line.split()] for line in completed.stdout.splitlines()]
        width = 2 if mode == "2d" else 6
        require(
            result and len(result) <= 65536 and all(len(row) == width for row in result),
            "Invalid motion helper result",
        )
        return result

    def world(self, slug, position):
        if slug not in self._centers:
            vertices = self.source(slug)["vertices"]
            self._centers[slug] = [
                (min(v["position"][i] for v in vertices) + max(v["position"][i] for v in vertices))
                * 0.5
                for i in (0, 2)
            ]
        cx, cz = self._centers[slug]
        tx, ty, tz = ATLAS["placements"][slug]
        x, y, z = position
        return [(x - cx) * 0.05 + tx / 4, -(z - cz) * 0.05 + ty / 4, y * 0.05 + tz / 8]


def build(donor, meshes, sources, helper):
    """Return source definitions and exact path samples for the actor composer."""
    reader = Placement(donor, sources, helper)
    return {
        "mesh": _mesh(reader, meshes),
        "effects": _effects(reader),
        "rigid": _rigid(reader),
        "runtime": _runtime(reader),
        "torches": reader.table("gTorchSpawns"),
        "small_fire": reader.table("gFireBreathsSpawns"),
        "large_fire": reader.position("large_fire"),
        "special_koopa_item": reader.position("special_koopa_item"),
    }


def _runtime(reader):
    train = reader.path("kalimari_desert", "d_course_kalimari_desert_train_path")
    traffic = reader.path("toads_turnpike", "d_course_toads_turnpike_track_path")
    train2 = reader.run("2d", train)
    bounds = reader.run("boundary", traffic, 50)
    return {
        "format": "rr64-hazard-runtime-paths",
        "version": 1,
        "source_commit": SOURCE_REFERENCE_COMMIT,
        "coordinate_space": "original MK64 source units, forward non-mirror; root applies course transform once",
        "kalimari_desert": {
            "count": len(train2),
            "source_controls": len(train),
            "processed_controls": len(train) - 1,
            "points_xz": train2,
            "source_function": "generate_2d_path",
            "height_source": "get_surface_height(first.x,2000,first.z), shared constant for all train parts",
        },
        "toads_turnpike": {
            "count": len(traffic),
            "separation": 50.0,
            "center": [row[:3] for row in traffic],
            "left": [row[:3] for row in bounds],
            "right": [row[3:] for row in bounds],
            "source_functions": ["process_path_data (identity copy)", "calculate_track_boundaries"],
        },
    }


def _mesh(reader, models):
    byname = {m["name"]: m["id"] for m in models["models"]}
    families = {f["name"]: f for f in models["families"]}
    paths = {}
    defs = []
    path = reader.path
    spline = reader.spline

    def add(course, kind, name, position, **extra):
        d = dict(
            course=course,
            kind=kind,
            model=byname[name],
            position_source=position,
            scale=1,
            solid=True,
        )
        d.update(extra)
        defs.append(d)
        return d

    add(
        "yoshi_valley",
        "egg",
        "yoshi_egg",
        reader.position("yoshi_egg"),
        target_source=[
            reader.position("yoshi_egg")[0],
            0,
            reader.position("yoshi_egg")[2] + reader.position("yoshi_egg_radius")[0],
        ],
        collision_radius_source=20,
        path_radius_source=70,
        source="actors.c:139–144 and1084; actors/yoshi_egg/update.inc.c",
    )
    for p, sub, yaw in [
        (reader.position("crossing" + str(i)), 1 if i < 2 else 0, 0 if i < 2 else 57344)
        for i in range(4)
    ]:
        add(
            "kalimari_desert",
            "crossing",
            "crossing_both_inactive",
            p,
            subtype=sub,
            rotation=[0, yaw, 0],
            solid=False,
            animation_frames=3,
            frame_ticks=20,
            source="actors.c:1118–1138; railroad_crossing render/update",
        )
    for i, (p, flags) in enumerate(
        [(reader.position("mario_sign" + str(i)), 0 if i == 0 else 0x4000) for i in range(2)]
    ):
        add(
            "mario_raceway",
            "sign",
            "mario_sign",
            p,
            source_flags=flags,
            collision_radius_source=7,
            collision_height_source=200,
            source="actors.c:1062–1068; collision_mario_sign:1577",
        )
    for p in [reader.position("wario_sign" + str(i)) for i in range(3)]:
        add("wario_stadium", "sign", "wario_sign", p, solid=False, source="actors.c:1148–1156")
    add(
        "luigi_raceway",
        "balloon",
        "hot_air_balloon",
        reader.position("balloon"),
        solid=False,
        initial_offset_source=[0, 300, 0],
        item_box_attachment_source=[0, -10, 0],
        source="update_objects.c:7538–7631",
        activation="D80165898 source mode gate; credits position excluded",
    )
    add(
        "banshee_boardwalk",
        "fish",
        "race_cheep_cheep",
        reader.position("fish"),
        solid=False,
        source="update_objects.c:4008–4063; sourceplayer0 donorpath160..170 trigger",
        source_scale_baked=2,
    )

    penguin = families["penguin"]
    clip_start = [c["model_ids"][0] for c in penguin["clips"]]
    clip_count = [c["frame_count"] for c in penguin["clips"]]
    positions = [
        reader.position("penguin" + str(i)) for i in (0, 1, 1, 2, 2, 3, 3, 4, 4, 5, 6, 7, 8, 9, 10)
    ]
    yaw = [
        0,
        32768,
        0,
        32768,
        0,
        32768,
        0,
        32768,
        0,
        0x9000,
        0x5000,
        0xC000,
        0x4000,
        0x8000,
        0x9000,
    ]
    for i, p in enumerate(positions):
        d = add(
            "sherbet_land",
            "penguin",
            models["models"][clip_start[0]]["name"],
            p,
            subtype=i,
            rotation=[0, yaw[i], 0],
            clip_start=clip_start,
            clip_count=clip_count,
            animation_frames=clip_count[0],
            collision_radius_source=12 if i == 0 else 4,
            source="update_objects.c:7177–7536",
        )
        d["scale"] = 0.2 if i == 0 else 0.08 if i <= 8 else 0.04
        if i == 0:
            d.update(
                path_symbol="D_800E659C",
                activation="source big penguin one-player only",
                source_motion="func8008B78C original spline; control offsets are not world path vertices",
            )
        elif i <= 8:
            d.update(
                circle_radius_source=100 if i <= 2 else 80,
                circle_angle_step=-256 if i in (5, 6) else 256 if i in (3, 4) else 336,
                source_phase=(i << 15) & 65535,
            )
        else:
            d.update(source_heading=yaw[i], initial_visual_heading=(yaw[i] + 32768) & 65535)
    paths["D_800E659C"] = spline("D_800E659C")
    paths["D_800E6668"] = spline("D_800E6668")
    rainbow = path("rainbow_road", "d_course_rainbow_road_track_path")
    paths["rainbow_primary"] = dict(
        points_source=[r[:3] for r in rainbow],
        kind="donor_primary_exact",
        source="d_course_rainbow_road_track_path",
    )
    chomp = families["chain_chomp"]["clips"][0]
    for i, node in enumerate((500, 800, 1100)):
        p = list(rainbow[node][:3])
        p[1] -= 15
        add(
            "rainbow_road",
            "chomp",
            models["models"][chomp["model_ids"][0]]["name"],
            p,
            node=node,
            path_symbol="rainbow_primary",
            animation_frames=chomp["frame_count"],
            speed_source=4,
            collision_radius_source=10,
            collision_height_source=30,
            source="update_objects.c:7634–7699",
        )
    flags = [v for row in reader.table("D_800E5DF4") for v in row]
    flag = families["yoshi_flag"]["clips"][0]
    for i in range(4):
        add(
            "yoshi_valley",
            "yoshi_flag",
            models["models"][flag["model_ids"][0]]["name"],
            flags[i * 4 : i * 4 + 3],
            rotation=[0, flags[i * 4 + 3] & 65535, 0],
            solid=False,
            subtype=1,
            animation_frames=flag["frame_count"],
            runtime_kind="sign",
            source="update_objects.c:6740; D800E5DF4; fixed yaw, skeletal frames animate",
        )
    seagull = families["seagull"]["clips"][0]
    for i in range(10):
        center = reader.position("seagull0" if i < 5 else "seagull1")
        add(
            "koopa_troopa_beach",
            "seagull",
            models["models"][seagull["model_ids"][0]]["name"],
            center,
            solid=False,
            subtype=0 if i < 5 else 1,
            animation_frames=seagull["frame_count"],
            path_symbol=["D_800E6034", "D_800E60F0", "D_800E61B4", "D_800E6280"][i % 4],
            source_random_offset_ranges=[[-100, 99], [0, 19], [-100, 99]],
            requires_authoritative_seed=True,
            source="update_objects.c:6527–6589",
        )
    for name in ["D_800E6034", "D_800E60F0", "D_800E61B4", "D_800E6280"]:
        paths[name] = spline(name)
    ferry = path("dks_jungle_parkway", "d_course_dks_jungle_parkway_ferry_path")
    points = [[x, -40, z] for x, z in reader.run("2d", ferry)]
    paths["ferry_runtime"] = dict(
        points_source=points,
        kind="source_generate_2d_path",
        source_controls=ferry,
        source_function_sha256=LAYOUT["motion_functions"]["generate2d"],
        source="vehicle_utils.inc.c:22–33 generate_ferry_path; height−40; last control excluded as native",
    )
    add(
        "dks_jungle_parkway",
        "ferry",
        "ferry_hull",
        points[0],
        path_symbol="ferry_runtime",
        node=0,
        speed_source=1.6666666,
        collision_half_extent_source=[100, 30, 30],
        source_native_collision="is_collide_with_vehicle length200,width60 and ydiff<60; not symmetricboxexact",
        source="vehicle_utils.inc.c:469–635",
        activation="source disabled for3+players; sharedallmodes is explicit adaptation",
    )
    sample_counts = {}
    for name, p in paths.items():
        if "controls" not in p:
            continue
        rows = reader.run(
            "spline",
            [r["offset_source"] + [r["velocity"]] for r in p["controls"]],
            p["control_count"],
        )
        p.update(
            kind="source_periodic_spline_samples",
            samples_source=[r[:3] for r in rows],
            derivatives_source=[r[3:] for r in rows],
            sample_ticks=1,
            requires_original_spline_evaluation=False,
            source_function_sha256=LAYOUT["motion_functions"]["spline"],
        )
        p["runtime_contract"] = (
            "Path.points = transformed origin+samples_source; Path.left = source derivative with axis mapping and scale ONLY, no origin/atlas translation; right duplicates left to satisfy paired lane schema. One sample per 30Hz source-object update; runtime uses left as a direction vector."
        )
        sample_counts[name] = len(rows)
    p = paths["rainbow_primary"]
    width = LAYOUT["rainbow_separation"]
    rows = reader.run("boundary", [row + [0] for row in p["points_source"]], width)
    p.update(
        left_source=[r[:3] for r in rows],
        right_source=[r[3:] for r in rows],
        source_maximum_separation=width,
        source_boundary_sha256=LAYOUT["motion_functions"]["boundary"],
    )
    return dict(
        format="mk64-source-mesh-definitions-draft",
        version=1,
        coordinate_space="original MK64 XYZ/source units; composer applies X,-Z,Y and selected course center/atlas/scale exactlyonce",
        model_indices="local188-model asset; remap using composerbase146",
        definitions=defs,
        paths=paths,
        assemblies=models["assemblies"],
        asset_sha256=models["sha256"],
        spline_sample_counts=sample_counts,
        notes=[
            "Penguin scales are per-instance, models already divideby8 for bake precision. Chomp.03,seagull.2,flag.027,fish2 source scales are already in model header; instance scale1.",
            "Spline samples are evaluated by original C functions, one per object update, with original integer duration accumulator and exact periodic wrap. left/right spline guidepoints are explicitly defined by each path runtime_contract.",
            "Source visibility/playercount gates and deterministic multiplayer PRNG are runtime adaptation choices.",
            "Moving Luigi balloon attached item may be omitted if not wired; do not claim it merely from visible balloon.",
        ],
    )


def _effects(reader):
    splines = []
    for name in ("D_800E5988", "D_800E5A44", "D_800E5B08", "D_800E5BD4", "D_800E5C90"):
        p = reader.spline(name)
        rows = p["controls"][: p["control_count"]]
        splines.append(
            dict(
                symbol=name,
                source_control_count=p["control_count"],
                points_mk_source_local=[r["offset_source"] for r in rows],
                durations=[r["velocity"] for r in rows],
                loop=True,
            )
        )
    return {
        "format": "rr64-source-effect-motion",
        "version": 1,
        "coordinate_space": "Boo points are LOCAL source MK(x,y,z); convert(x,-z,y)*catalogue_scale, NO atlas translation.",
        "splines": splines,
        "boo_groups": [
            {
                "subtype": 0,
                "start_path_range": [201, 210],
                "end_path_ranges": [[181, 190], [281, 290]],
            },
            {
                "subtype": 1,
                "start_path_range": [511, 520],
                "end_path_ranges": [[491, 500], [621, 630]],
            },
        ],
        "fish": {
            "source_origin": reader.position("fish"),
            "trigger_path_range": [160, 171],
            "yaw": 22528,
            "speed": 25,
            "vertical_speed": 18,
            "gravity": 0.7,
            "visible_movement_ticks": 71,
        },
        "bat": [
            {
                "subtype": 1,
                "origin_base_mk": reader.position("bat1origin"),
                "origin_random_max_exclusive": [30, 25, 30],
                "origin_random_sign": [-1, 1, 1],
                "target_base_mk": reader.position("bat1target"),
                "target_random_max_exclusive": [0, 0, 150],
                "target_random_sign": [0, 0, -1],
                "exit_x": reader.position("bat1exit")[0],
                "initial_pitch": 56320,
                "initial_pitch_target": 2048,
                "pitch_target_ticks": 31,
                "activate_distance": 1150,
            },
            {
                "subtype": 2,
                "origin_base_mk": reader.position("bat2origin"),
                "origin_random_max_exclusive": [30, 25, 30],
                "origin_random_sign": [-1, 1, 1],
                "target_base_mk": reader.position("bat2target"),
                "target_random_max_exclusive": [0, 0, 200],
                "target_random_sign": [0, 0, 1],
                "exit_x": reader.position("bat2exit")[0],
                "initial_pitch": 0,
                "initial_pitch_target": 0,
                "activate_distance": 700,
            },
        ],
        "source_sha256": "ROM tables: " + SOURCE_REFERENCE_COMMIT,
    }


def _rigid(reader):
    spawns = {}
    for key in ("gThomwpSpawns50CC", "gThwompSpawns100CCExtra", "gThomwpSpawns150CC"):
        spawns[key] = []
        for x, z, subtype, phase in reader.table(key):
            p = [x, 0, z]
            spawns[key].append(
                dict(
                    source_position=p,
                    position=reader.world("bowsers_castle", p),
                    subtype=subtype,
                    phase=phase,
                )
            )
    rocks = []
    for x, y, z, index in reader.table("d_course_choco_mountain_falling_rock_spawns"):
        if x == -32768:
            continue
        p = [x, y + 10, z]
        rocks.append(
            dict(
                id=index,
                model=6,
                source_position=p,
                position=reader.world("choco_mountain", p),
                respawn_updates=[60, 120, 180][index],
                source_radius=10,
            )
        )
    paths = {}
    for slug, name in [
        ("kalimari_desert", "d_course_kalimari_desert_train_path"),
        ("toads_turnpike", "d_course_toads_turnpike_track_path"),
    ]:
        rows = reader.path(slug, name)
        paths[slug] = dict(
            source_symbol=name,
            source_control_points=rows,
            world_control_points=[reader.world(slug, r[:3]) for r in rows],
            warning="Source controls, not the runtime resampled path. Preserve the original path preprocessing and follow algorithm.",
        )
    return {
        "format": "rr64-hazard-source-placements",
        "version": 1,
        "coordinate_space": "positions already RR rider-world/atlas; source_position remains unscaled MK64",
        "thwomp_spawn_tables": spawns,
        "falling_rocks": rocks,
        "paths": paths,
        "traffic": {
            "family_count": 7,
            "source_init_function": "initialize_toads_turnpike_vehicle",
            "families": [
                {"kind": "box_truck", "models": [7, 8, 9], "path_offset": 0},
                {"kind": "school_bus", "models": [10], "path_offset": 75},
                {"kind": "tanker_truck", "models": [11], "path_offset": 50},
                {"kind": "car", "models": [12], "path_offset": 25},
            ],
            "spawn_path_index": "((i * runtimePathCount) / 7 + familyOffset) % runtimePathCount",
            "lane_type": "random_int(3) in races; i%3 in time trials",
            "initial_lane_factor": "(laneType-1)*0.6",
            "speed_A_source": "CCindex*90/216+4.583333333333333",
            "speed_B_source": "CCindex*90/216+2.9166666666666665",
            "speed_selection": "A iff (CCindex>0 or timeTrial) and laneType==2, otherwise B",
            "source_class_indices": {"50cc": 0, "100cc": 1, "150cc": 2, "extra": 3},
        },
        "train": {
            "count": 2,
            "speed_source_per_update": 5,
            "source_init": "init_vehicles_trains",
            "initial_path_index": "(i * runtimePathCount / 2 + 160) % runtimePathCount; passengers advance4 each, then tender+3, locomotive+4",
            "models": {"engine": 13, "tender": 14, "passenger": 15},
            "single_player_passengers": 5,
            "source_multiplayer": "2P versus uses tender+passenger[4];3P/4P source uses locomotive only",
        },
        "source_commit": SOURCE_REFERENCE_COMMIT,
        "private_assets_not_distributable": True,
    }
