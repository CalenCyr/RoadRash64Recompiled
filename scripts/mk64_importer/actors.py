"""Compose ROM-derived actors without reading development packs or source trees.

Model identities, source paths, attachments and deterministic spawn ordering are
shared by every importer. Only generated inputs are accepted; this module never
contains an extracted mesh, texture, animation stream or spawn table.
"""

import copy
import hashlib
import math
import struct
from .common import require
from .actor_rigid import build as build_rigid


def digest(data):
    return hashlib.sha256(data).hexdigest()


class Composer:
    def __init__(
        self,
        courses,
        geometries,
        sources,
        items,
        sprites,
        meshes,
        sprite_bank,
        mesh_bank,
        rigid_models,
        placement,
    ):
        self.catalogue = {"courses": courses}
        self.sprites, self.meshes = sprites, meshes
        self.sprite_bank, self.mesh_bank = sprite_bank, mesh_bank
        self.placement = placement
        self.items = copy.deepcopy(items)
        self.families = {f["name"]: f for f in sprites["families"]}
        self.transforms = {slug: geo["transform"] for slug, geo in geometries.items()}
        self.sources = sources
        self.courses = build_rigid(sources, rigid_models, placement)
        self.adaptations = {slug: [] for slug in self.courses}
        self.anchors, self.ground_evidence, self.item_changes = [], [], {}
        for course in courses:
            slug = course["id"]
            require(self.transforms[slug]["scale"] == 0.234375, "Unexpected source units")
        self.mesh_base = sprites["model_count"]
        self.flame_base = self.mesh_base + len(meshes["models"])
        self.model_count = self.flame_base + 4
        require(self.model_count <= 2048, "Too many actor models")

    def source_course(self, slug):
        return self.sources[slug]

    def path(self, slug, points, *, local=False, durations=None, source_name=""):
        paths = self.courses[slug]["paths"]
        convert = self.local if local else self.world
        result = {
            "points": [convert(slug, p) for p in points],
            "source_name": source_name,
            "source_coordinate_space": "local MK64 offsets" if local else "absolute MK64 world",
        }
        require(5 <= len(points) <= 4096, "Source path count")
        if durations is not None:
            require(
                len(durations) == len(points) and all(1 <= v <= 10000 for v in durations),
                "Source spline durations",
            )
            result["durations"] = durations
        paths.append(result)
        return len(paths) - 1

    def local(self, slug, xyz):
        scale = self.transforms[slug]["scale"]
        x, y, z = xyz
        return [x * scale, -z * scale, y * scale]

    def world(self, slug, xyz):
        t = self.transforms[slug]
        x, y, z = xyz
        cx, cz = t["source_center_xz"]
        dx, dy, dz = t["atlas_translation_world"]
        result = [(x - cx) * t["scale"] + dx, -(z - cz) * t["scale"] + dy, y * t["scale"] + dz]
        require(all(math.isfinite(v) for v in result), "Invalid actor position")
        return result

    def source_floor(self, slug, x, z, ceiling=20):
        data = self.source_course(slug)
        hits = []
        for index, triangle in enumerate(data["collision_triangles"]):
            a, b, c = (data["vertices"][i]["position"] for i in triangle["vertices"])
            determinant = (b[2] - c[2]) * (a[0] - c[0]) + (c[0] - b[0]) * (a[2] - c[2])
            if abs(determinant) < 1e-9:
                continue
            u = ((b[2] - c[2]) * (x - c[0]) + (c[0] - b[0]) * (z - c[2])) / determinant
            v = ((c[2] - a[2]) * (x - c[0]) + (a[0] - c[0]) * (z - c[2])) / determinant
            if min(u, v, 1 - u - v) < -1e-9:
                continue
            y = u * a[1] + v * b[1] + (1 - u - v) * c[1]
            if y <= ceiling:
                hits.append((y, index))
        require(hits, f"No authored ground under {slug} ({x},{z})")
        y, index = max(hits)
        self.ground_evidence.append(
            {
                "course": slug,
                "source_xz": [x, z],
                "source_height": y,
                "source_triangle": index,
                "ceiling": ceiling,
            }
        )
        return y

    def definition(self, slug, kind, family_name, xyz, **options):
        family = self.families[family_name]
        scale = self.transforms[slug]["scale"]
        radius = family["collision_radius_source"] * scale
        solid = options.pop("solid", radius > 0)
        position = self.world(slug, xyz)
        entries = self.courses[slug]["definitions"]
        d = {
            "id": len(entries),
            "kind": kind,
            "model": family["models"][0],
            "subtype": 0,
            "phase": 0,
            "path": 0,
            "node": 0,
            "position": position,
            "target": position.copy(),
            "offset": [0, 0, 0],
            "half_extent": [radius if solid else 0.001] * 3,
            "scale": 1,
            "speed": 0,
            "minimum_height": -100,
            "lane": 0,
            "animation_frames": len(family["models"]),
            "frame_ticks": family["frame_ticks"],
            "parent": 0xFFFFFFFF,
            "billboard": family["billboard"],
            "solid": solid,
            "collision_radius": radius if solid else 0,
            "source_actor": {
                "family": family_name,
                "position_mk64": list(xyz),
                "source_scale_baked_in_model": family["source_scale"],
                "source_refs": family["source_refs"],
            },
        }
        d.update(options)
        entries.append(d)
        self.anchors.append(
            {
                "course": slug,
                "id": d["id"],
                "kind": kind,
                "source_position": list(xyz),
                "world_position": position,
            }
        )
        return d

    def base_families(self):
        p = self.sprites["placements"]
        slug = "moo_moo_farm"
        for index, xyz in enumerate(p["gMoleSpawns"]["source_rows"]):
            position = [xyz[0], xyz[1] - 9, xyz[2]]
            group = 1 if index < 8 else 2 if index < 19 else 3
            mole = self.definition(slug, "mole", "mole", position, subtype=group)
            self.definition(
                slug, "smoke", "mole_dirt", position, subtype=3, parent=mole["id"], solid=False
            )
        self.adaptations[slug].append(
            "One cosmetic dirt companion per hole instead of original8single-player/4multiplayer fragments; all31holes and original shared mole motion retained."
        )
        slug = "koopa_troopa_beach"
        for x, tx, z, tz in p["gCrabSpawns"]["source_rows"]:
            y = self.source_floor(slug, x, z) + 2.5
            self.definition(slug, "crab", "crab", [x, y, z], target=self.world(slug, [tx, y, tz]))
        slug = "yoshi_valley"
        for i, (xyz, target) in enumerate(
            zip(p["gHedgehogSpawns"]["source_rows"], p["gHedgehogPatrolPoints"]["source_rows"])
        ):
            position = [xyz[0], xyz[1] + 6, xyz[2]]
            self.definition(
                slug,
                "hedgehog",
                "hedgehog",
                position,
                target=self.world(slug, [target[0], xyz[1] + 6, target[2]]),
                phase=i % 6,
                speed=(0.5 + 0.1 * (i % 6)) * self.transforms[slug]["scale"],
            )
        slug = "frappe_snowland"
        for xyz in p["gSnowmanSpawns"]["source_rows"]:
            body_position = [xyz[0], xyz[1] + 3, xyz[2]]
            body = self.definition(slug, "snowman", "snowman_body", body_position, subtype=0)
            self.definition(
                slug,
                "snowman",
                "snowman_head",
                [xyz[0], xyz[1] + 8, xyz[2]],
                subtype=1,
                parent=body["id"],
                solid=False,
            )
            self.definition(
                slug,
                "smoke",
                "snowman_snow",
                body_position,
                subtype=4,
                parent=body["id"],
                solid=False,
            )
        self.adaptations[slug].append(
            "One cosmetic snow fragment per snowman body;19originalbody/head pairs and recovery behavior retained."
        )
        for slug, symbol in [
            ("mario_raceway", "d_course_mario_raceway_piranha_plant_spawns"),
            ("royal_raceway", "d_course_royal_raceway_piranha_plant_spawn"),
        ]:
            for x, y, z, flags in p[symbol]["source_rows"]:
                self.definition(slug, "plant", "piranha_" + slug, [x, y, z])["source_actor"][
                    "flags"
                ] = flags
        for family in self.families.values():
            if not family["name"].startswith("neon_"):
                continue
            d = self.definition(
                "rainbow_road", "neon", family["name"], family["source_position"], solid=False
            )
            if "frame_sequence" in family:
                initial = family["initial_frame_sequence"]
                d["frame_sequence"] = initial + family["frame_sequence"]
                d["visible_sequence"] = (
                    family["initial_visible_sequence"] + family["visible_sequence"]
                )
                d["animation_loop_start"] = len(initial)
                require(
                    len(d["frame_sequence"]) == len(d["visible_sequence"]) <= 4096,
                    "Neon schedule bound",
                )

    def effects(self):
        metadata = self.placement["effects"]
        slug = "banshee_boardwalk"
        primary = [p["position"] for p in self.source_course(slug)["path"]]
        paths = [
            self.path(
                slug,
                spline["points_mk_source_local"],
                local=True,
                durations=spline["durations"],
                source_name=spline["symbol"],
            )
            for spline in metadata["splines"]
        ]
        require(len(paths) == 5, "Boo needs five source spline families")
        for group, start, finish in ((0, 201, 281), (1, 511, 621)):
            for phase in range(5):
                self.definition(
                    slug,
                    "boo",
                    "boo",
                    primary[start],
                    subtype=group,
                    phase=phase,
                    target=self.world(slug, primary[finish]),
                    path=paths[phase],
                    animation_frames=58,
                    solid=False,
                )
        for group, slots in zip(metadata["bat"], (12, 8)):
            for phase in range(slots):
                self.definition(
                    slug,
                    "bat",
                    "bat",
                    group["origin_base_mk"],
                    subtype=group["subtype"],
                    phase=phase,
                    target=self.world(slug, group["target_base_mk"]),
                )
        self.adaptations[slug].append(
            "Bats use12/8authoritative slots instead of source40/30; original random spawn envelopes and flight behavior retained. Boo groups retain five source splines each."
        )
        # The original Kiwano has one actor per human, triggered by source grass.
        # Reserve the complete online actor-slot range, inactive for absent slots.
        slug = "dks_jungle_parkway"
        primary = [p["position"] for p in self.source_course(slug)["path"]]
        path_id = self.path(slug, primary, source_name="original DK primary race path")
        for slot in range(14):
            self.definition(slug, "kiwano", "kiwano", primary[0], subtype=slot, path=path_id)
        # DK's eight permanent torches are separate from Bowser's five emitters.
        for xyz in self.placement["torches"]:
            self.definition(
                "dks_jungle_parkway",
                "flame",
                "bowser_flame_smoke",
                xyz,
                subtype=0,
                scale=0.8,
                solid=False,
            )
        values = [v for xyz in self.placement["small_fire"] for v in xyz]
        require(len(values) == 12, "Four source small fire emitters")
        emitters = [(self.placement["large_fire"], 4, 20, 0x2100, 1)]
        emitters += [
            (values[i : i + 3], 5, 10, 0 if i // 3 % 2 == 0 else 0x8000, 0.5)
            for i in range(0, len(values), 3)
        ]
        for position, subtype, count, yaw, scale in emitters:
            for phase in range(count):
                d = self.definition(
                    "bowsers_castle",
                    "flame",
                    "bowser_flame_smoke",
                    position,
                    subtype=subtype,
                    phase=phase,
                    model=self.flame_base,
                    scale=scale,
                    animation_frames=4,
                    frame_ticks=1,
                    rotation=[0, yaw, 0],
                    solid=False,
                )
                d["source_actor"]["source_refs"] = [
                    "src/code_8006E9C0.c:COURSE_BOWSER_CASTLE",
                    "src/update_objects.c:func_8007614C/func_8007661C",
                    "src/render_objects.c:render_object_bowser_flame_particle",
                ]
                d["source_actor"]["material"] = "primitive RGB with intensity alpha, flag16384"

    def mesh_definition(self, slug, kind, model, xyz, *, radius=0, solid=False, **options):
        entries = self.courses[slug]["definitions"]
        position = self.world(slug, xyz)
        r = radius * self.transforms[slug]["scale"]
        d = {
            "id": len(entries),
            "kind": kind,
            "model": self.mesh_base + model,
            "subtype": 0,
            "phase": 0,
            "path": 0,
            "node": 0,
            "position": position,
            "target": position.copy(),
            "offset": [0, 0, 0],
            "half_extent": [r if solid and r else 0.001] * 3,
            "scale": 1,
            "speed": 0,
            "minimum_height": -100,
            "lane": 0,
            "animation_frames": 1,
            "frame_ticks": 1,
            "parent": 0xFFFFFFFF,
            "billboard": "none",
            "solid": solid,
            "collision_radius": r if solid else 0,
        }
        d.update(options)
        entries.append(d)
        self.anchors.append(
            {
                "course": slug,
                "id": d["id"],
                "kind": kind,
                "source_position": list(xyz),
                "world_position": position,
            }
        )
        return d

    def meshes_and_attachments(self):
        metadata = self.placement["mesh"]
        require(metadata["asset_sha256"] == self.meshes["sha256"], "Mesh placement identity")
        paths = {}
        assemblies = {a["name"]: a for a in metadata["assemblies"]}
        for source in metadata["definitions"]:
            slug = source["course"]
            kind = source.get("runtime_kind", source["kind"])
            xyz = source["position_source"].copy()
            options = {
                key: source[key]
                for key in (
                    "scale",
                    "subtype",
                    "phase",
                    "rotation",
                    "node",
                    "frame_ticks",
                    "animation_frames",
                )
                if key in source
            }
            options["speed"] = source.get("speed_source", 0) * self.transforms[slug]["scale"]
            options["source_actor"] = copy.deepcopy(source)
            if "target_source" in source:
                options["target"] = self.world(slug, source["target_source"])
            if "clip_start" in source:
                base = source["model"]
                options["clips"] = [
                    {"start": start - base, "count": count}
                    for start, count in zip(source["clip_start"], source["clip_count"])
                ]
                options["animation_frames"] = max(c["start"] + c["count"] for c in options["clips"])
            if "collision_half_extent_source" in source:
                x, y, z = source["collision_half_extent_source"]
                scale = self.transforms[slug]["scale"]
                options["half_extent"] = [x * scale, z * scale, y * scale]
            if "source_random_offset_ranges" in source:
                # An immutable per-instance seed keeps generated assets equal on
                # every peer. This replaces only the source initialization RNG.
                seed = hashlib.sha256(
                    (slug + ":" + str(len(self.courses[slug]["definitions"]))).encode()
                ).digest()
                jitter = [
                    low + int.from_bytes(seed[4 * a : 4 * a + 4], "big") % (high - low + 1)
                    for a, (low, high) in enumerate(source["source_random_offset_ranges"])
                ]
                xyz = [xyz[a] + jitter[a] for a in range(3)]
                options["source_actor"]["selected_rng_offset"] = jitter
            if "path_symbol" in source:
                symbol = source["path_symbol"]
                p = metadata["paths"][symbol]
                key = (slug, symbol)
                if p.get("kind") == "source_periodic_spline_samples":
                    require(
                        not p.get("requires_original_spline_evaluation"), "Unresolved source spline"
                    )
                    points = [[xyz[a] + q[a] for a in range(3)] for q in p["samples_source"]]
                    path_id = self.path(slug, points, source_name=symbol)
                    # Small derivatives must not be added to/subtracted from
                    # large atlas coordinates, which loses heading precision.
                    self.courses[slug]["paths"][path_id]["left"] = [
                        self.local(slug, q) for q in p["derivatives_source"]
                    ]
                    self.courses[slug]["paths"][path_id]["right"] = copy.deepcopy(
                        self.courses[slug]["paths"][path_id]["left"]
                    )
                    self.courses[slug]["paths"][path_id][
                        "source_heading_contract"
                    ] = "local scaled direction vectors, no atlas translation"
                    self.courses[slug]["paths"][path_id]["source_sample_ticks"] = p["sample_ticks"]
                    options["path"] = path_id
                else:
                    require("points_source" in p, "Unevaluated mesh motion path " + symbol)
                    if key not in paths:
                        path_id = self.path(slug, p["points_source"], source_name=symbol)
                        if "left_source" in p:
                            self.courses[slug]["paths"][path_id]["left"] = [
                                self.world(slug, q) for q in p["left_source"]
                            ]
                            self.courses[slug]["paths"][path_id]["right"] = [
                                self.world(slug, q) for q in p["right_source"]
                            ]
                        paths[key] = path_id
                    options["path"] = paths[key]
            if kind == "fish":
                primary = self.source_course(slug)["path"]
                options["target"] = self.world(slug, primary[165]["position"])
            d = self.mesh_definition(
                slug,
                kind,
                source["model"],
                xyz,
                radius=source.get("collision_radius_source", 0),
                solid=source["solid"],
                **options,
            )
            if kind == "balloon":
                course = next(c for c in self.catalogue["courses"] if c["id"] == slug)
                items = self.items[slug]
                scale = self.transforms[slug]["scale"]
                box = {
                    "id": len(items["boxes"]),
                    "position": self.world(slug, [xyz[0], xyz[1] + 290, xyz[2]]),
                    "radius": 5.5 * scale,
                    "kind": 5,
                    "parent_hazard": d["id"],
                    "parent_offset": [0, 0, -10 * scale],
                    "source_symbol": "Luigi source balloon attached item",
                    "source_parent_position": xyz,
                    "source_parent_offset": [0, -10, 0],
                }
                items["boxes"].append(box)
                require(len(items["boxes"]) <= 64, "Attached item capacity")
                self.item_changes[slug] = items
            if kind == "ferry":
                self.wheels(slug, d, assemblies["ferry"])
                for side in (-30, 30):
                    for phase in range(4):
                        self.definition(
                            slug,
                            "smoke",
                            "train_ferry_smoke",
                            xyz,
                            subtype=1,
                            parent=d["id"],
                            phase=phase,
                            solid=False,
                            target=self.local(slug, [side, 180, 45]),
                        )
                self.adaptations[slug].append(
                    "One original source ferry is shared across all player counts; no mirrored or extra ferry invented."
                )
        # Replace baked-wheel train visuals with their exact wheel-free body
        # records, then add the authored independent wheel transforms.
        slug = "kalimari_desert"
        trains = [d for d in self.courses[slug]["definitions"] if d["kind"] == "train"]
        for d in trains:
            name = (
                "train_engine"
                if d["subtype"] == 6
                else "train_tender" if d["subtype"] == 5 else "train_passenger"
            )
            a = assemblies[name]
            d["collision_model"] = d["model"]
            d["model"] = self.mesh_base + a["body_model"]
            d["source_actor"] = {"assembly": name, "source": a["source"]}
            self.wheels(slug, d, a)
            if name == "train_engine":
                for phase in range(3):
                    # Definitions require world origins; pose positioning uses
                    # the source-local parent exhaust target from the first tick.
                    t = self.transforms[slug]
                    p = d["position"]
                    cx, cz = t["source_center_xz"]
                    dx, dy, dz = t["atlas_translation_world"]
                    s = t["scale"]
                    xyz = [(p[0] - dx) / s + cx, (p[2] - dz) / s, -(p[1] - dy) / s + cz]
                    self.definition(
                        slug,
                        "smoke",
                        "train_ferry_smoke",
                        xyz,
                        subtype=0,
                        parent=d["id"],
                        phase=phase,
                        solid=False,
                        target=self.local(slug, [0, 65, 25]),
                    )
        self.adaptations[slug].append(
            "All14sourcecarriages and104animatedwheelmeshes retained;3smokepuffs perengine instead of full original particlepool to keep128sharedposes including4crossings."
        )

    def wheels(self, slug, parent, assembly):
        scale = self.transforms[slug]["scale"]
        for child in assembly["wheel_children"]:
            d = {
                "id": len(self.courses[slug]["definitions"]),
                "kind": "wheel",
                "model": self.mesh_base + child["model"],
                "subtype": (
                    1
                    if assembly["name"] == "ferry"
                    else 2 if assembly["name"] == "train_tender" else 0
                ),
                "phase": 0,
                "path": 0,
                "node": 0,
                "position": parent["position"].copy(),
                "offset": [0, 0, 0],
                "half_extent": [0.001] * 3,
                "scale": parent["scale"],
                "speed": 0,
                "minimum_height": -100,
                "lane": 0,
                "parent": parent["id"],
                "target": self.local(slug, child["offset_source"]),
                "solid": False,
                "collision_radius": 0,
                "billboard": "none",
                "animation_frames": 1,
                "frame_ticks": 1,
                "rotation": [round(child["phase_degrees"] * 65536 / 360) % 65536, 0, 0],
                "source_actor": {
                    "assembly": assembly["name"],
                    "offset_source": child["offset_source"],
                    "source": assembly["source"],
                },
            }
            self.courses[slug]["definitions"].append(d)

    def merge_models(self):
        first = self.sprite_bank
        second = self.mesh_bank
        require(digest(first) == self.sprites["model_sha256"], "Sprite asset changed")
        require(digest(second) == self.meshes["sha256"], "Mesh asset changed")
        require(first[:8] == second[:8] == b"MKHZ0001", "Model format mismatch")
        require(struct.unpack_from(">I", first, 8)[0] == self.mesh_base, "Sprite model count")
        require(
            struct.unpack_from(">I", second, 8)[0] == len(self.meshes["models"]), "Mesh model count"
        )
        result = bytearray(first[:8] + struct.pack(">I", self.model_count) + first[12:])
        offset = 12
        for i in range(len(self.meshes["models"])):
            size = struct.unpack_from(">I", second, offset)[0]
            require(offset + 4 + size <= len(second), "Truncated mesh record")
            record = bytearray(second[offset + 4 : offset + 4 + size])
            require(struct.unpack_from(">I", record)[0] == i, "Mesh model order")
            struct.pack_into(">I", record, 0, self.mesh_base + i)
            result += struct.pack(">I", size) + record
            offset += 4 + size
        require(offset == len(second), "Mesh trailing bytes")
        require(result[12 : len(first)] == first[12:], "Sprite records changed during merge")
        # The same original intensity images serve two different source draw
        # functions. Append material variants after all existing IDs so sprite
        # and articulated mesh identities remain stable.
        offset = 12
        for index in range(self.mesh_base):
            size = struct.unpack_from(">I", first, offset)[0]
            record = bytearray(first[offset + 4 : offset + 4 + size])
            if 141 <= index <= 144:
                struct.pack_into(">I", record, 0, self.flame_base + index - 141)
                batches = struct.unpack_from(">I", record, 8)[0]
                cursor = 36
                for _ in range(batches):
                    flags, width, height, triangles = struct.unpack_from(">4I", record, cursor)
                    require(flags & 32768, "Expected source torch intensity material")
                    struct.pack_into(">I", record, cursor, (flags & ~32768) | 16384)
                    cursor += 16 + triangles * 3 * 16 + width * height * 2
                require(cursor == len(record), "Flame material variant record extent")
                result += struct.pack(">I", len(record)) + record
            offset += 4 + size
        return result

    def validate(self):
        for slug, data in self.courses.items():
            definitions = data["definitions"]
            require(len(definitions) <= 128, "Too many poses " + slug)
            for index, d in enumerate(definitions):
                require(d["id"] == index, "Actor identity ordering")
                require(
                    d["model"] < self.model_count
                    and d.get("animation_frames", 1) <= self.model_count - d["model"],
                    "Model range",
                )
                require(
                    d.get("parent", 0xFFFFFFFF) == 0xFFFFFFFF or d["parent"] < index,
                    "Actor parent ordering",
                )
                require(
                    all(
                        math.isfinite(x)
                        for key in ("position", "offset", "half_extent")
                        for x in d[key]
                    ),
                    "Nonfinite actor",
                )
                require(all(0 < x <= 100 for x in d["half_extent"]), "Collider extents")
                require(
                    all(0 <= f < d.get("animation_frames", 1) for f in d.get("frame_sequence", [])),
                    "Frame range",
                )
                require(
                    0 <= d.get("animation_loop_start", 0) < len(d.get("frame_sequence", [0])),
                    "Animation loop range",
                )


def compose(
    courses,
    geometries,
    sources,
    items,
    sprite_manifest,
    mesh_manifest,
    sprite_bank,
    mesh_bank,
    rigid_models,
    placement,
):
    """Return the combined model bank, per-course hazards and item definitions."""
    composer = Composer(
        courses,
        geometries,
        sources,
        items,
        sprite_manifest,
        mesh_manifest,
        sprite_bank,
        mesh_bank,
        rigid_models,
        placement,
    )
    composer.base_families()
    composer.effects()
    composer.meshes_and_attachments()
    composer.validate()
    for slug, data in composer.courses.items():
        data["adaptations"] += composer.adaptations[slug]
        data["actor_asset_source_commit"] = sprite_manifest["source_commit"]
    return bytes(composer.merge_models()), composer.courses, composer.items
