"""ROM-derived animated sprite bank and original spawn tables."""

from pathlib import Path
import json, struct
from .common import digest as sha, require
from .assets import Assets
from . import SOURCE_REFERENCE_COMMIT
from .neon_timing import sequences


class Extractor:
    def __init__(self, donor):
        self.donor = donor
        self.rom = donor.rom
        self.images = Assets(donor)
        self.layout = json.loads(Path(__file__).with_name("sprite_layout.json").read_text())
        self.actor_layout = json.loads(Path(__file__).with_name("actor_layout.json").read_text())
        self.models = []
        self.families = []
        self.common_geometry = {}

    def decoded(self, offset):
        return self.donor.mio0(offset)

    def asset(self, slug, name, palette_override=None):
        w, h, pixels = self.images.texture(slug, name, palette_override)
        return {
            "name": name,
            "width": w,
            "height": h,
            "pixels": pixels,
            "provenance": {"course": slug, "asset": name, "rgba16_sha256": sha(pixels)},
        }

    def common_vertices(self, offset, count):
        block = self.decoded(0x132B50)
        self.common_geometry[hex(0x0D000000 + offset)] = {
            "vertex_count": count,
            "sha256": sha(block[offset : offset + count * 16]),
        }
        require(
            block[0x6940:0x6948] == bytes.fromhex("b100040200000604"), "Common quad triangle order"
        )
        return [
            list(v) for v in struct.iter_unpack(">hhhHhh4B", block[offset : offset + count * 16])
        ]

    def named_vertices(self, relative, name):
        if relative.startswith("courses/"):
            slug = relative.split("/")[1]
            info = self.actor_layout["courses"][slug]["symbols"][name]
            raw = self.donor.read(slug, info["address"], info["size"])
        else:
            info = self.actor_layout["symbols"][name]
            off = info["rom_offset"]
            raw = self.rom[off : off + info["size"]]
        require(info["type"] == "Vtx", "Expected named actor vertices")
        return [list(v) for v in struct.iter_unpack(">hhhHhh4B", raw)]

    def source_table(self, name):
        info = self.layout["symbols"].get(name)
        if info is None:
            info = self.layout["course_symbols"][name]
            raw = self.donor.read(info["course"], info["address"], info["size"])
        else:
            raw = self.rom[info["rom_offset"] : info["rom_offset"] + info["size"]]
        code = "f" if info["type"] == "f32" else "h"
        return [list(v) for v in struct.iter_unpack(">" + str(info["stride"]) + code, raw)]

    def neon_positions(self):
        rows = []
        for row in self.layout["neon_positions"]:
            values = []
            for info in row:
                if info["type"] == "f64":
                    value = struct.unpack_from(">d", self.rom, info["rom_offset"])[0]
                elif info["type"] == "mips-f32":
                    hi = struct.unpack_from(">I", self.rom, info["high_instruction"])[0]
                    lo = struct.unpack_from(">I", self.rom, info["low_instruction"])[0]
                    require(
                        hi >> 26 == 15 and lo >> 26 == 13, "Position immediate instruction encoding"
                    )
                    value = struct.unpack(
                        ">f", struct.pack(">I", ((hi & 65535) << 16) | (lo & 65535))
                    )[0]
                else:
                    ins = struct.unpack_from(">I", self.rom, info["instruction"])[0]
                    require(ins >> 26 == 9, "Position immediate instruction encoding")
                    value = struct.unpack(">h", struct.pack(">H", ins & 65535))[0]
                values.append(value)
            rows.append(values)
        return rows + self.source_table("D_800E6734")

    @staticmethod
    def transform(vertices, roll, half_uv):
        result = []
        for raw in vertices:
            vertex = raw.copy()
            if roll:
                vertex[0], vertex[1] = -vertex[0], -vertex[1]
            if half_uv:
                require(vertex[4] % 2 == 0 and vertex[5] % 2 == 0, "Half-UV must remain exact")
                vertex[4] //= 2
                vertex[5] //= 2
            result.append(vertex)
        return result

    @staticmethod
    def image_band(texture, start, height, width=None):
        width = width or texture["width"]
        raw = texture["pixels"]
        rows = []
        for row in range(start, start + height):
            y = min(row, texture["height"] - 1)
            line = raw[y * texture["width"] * 2 : (y + 1) * texture["width"] * 2]
            line += line[-2:] * (width - texture["width"])
            rows.append(line)
        return {"width": width, "height": height, "pixels": b"".join(rows), "row_start": start}

    def add_family(
        self,
        name,
        slug,
        frames,
        vertices,
        scale,
        radius,
        *,
        roll=True,
        half_uv=True,
        mirrored=False,
        translucent=False,
        combine=0,
        interval=1,
        billboard="yaw",
        refs=(),
        semantics="animation",
        variants=None,
        texture_flags=None,
    ):
        first = 16 + len(self.models)
        flags = 8 | (4 if translucent else 2) | (32 if mirrored else 64) | 256 | combine
        if texture_flags is not None:
            flags = (flags & ~(32 | 64 | 128 | 256)) | texture_flags
        for frame_index, texture in enumerate(frames):
            raw_vertices = vertices if variants is None else variants[frame_index]
            batches = []
            if texture["width"] == 64 and texture["height"] == 64:
                require(len(raw_vertices) == 8, "64x64 source overlap requires eight vertices")
                chunks = [
                    (raw_vertices[:4], self.image_band(texture, 0, 32)),
                    (raw_vertices[4:8], self.image_band(texture, 31, 32)),
                ]
            elif texture["width"] == 48 and texture["height"] == 40:
                # The source 48x40 Boo quad spans 39 texels; cut at exact row31.
                v = raw_vertices
                require(len(v) == 4 and v[0][1] == -19 and v[2][1] == 20, "Boo source quad")
                middle = [v[3].copy(), v[2].copy()]
                for m in middle:
                    m[1] = 12
                    m[5] = 1984
                lower = [v[0], v[1], middle[1], middle[0]]
                upper = [middle[0].copy(), middle[1].copy(), v[2].copy(), v[3].copy()]
                for m in upper:
                    m[5] -= 1984
                chunks = [
                    (lower, self.image_band(texture, 0, 32, 64)),
                    (upper, self.image_band(texture, 31, 16, 64)),
                ]
            else:
                require(
                    len(raw_vertices) == 4 and texture["width"] * texture["height"] <= 2048,
                    "Source quad size",
                )
                chunks = [(raw_vertices, self.image_band(texture, 0, texture["height"]))]
            for quad, image in chunks:
                cooked = self.transform(quad, roll, half_uv)
                # Common0D006940 uses 0,2,1 and0,3,2; actor-specific course lists use opposite winding.
                order = (0, 2, 1, 0, 3, 2) if half_uv else (0, 1, 2, 0, 2, 3)
                batches.append(
                    {"flags": flags, "vertices": [cooked[i] for i in order], "image": image}
                )
            verts = [v for batch in batches for v in batch["vertices"]]
            self.models.append(
                {
                    "id": 16 + len(self.models),
                    "name": f"{name}_{frame_index}",
                    "source_scale": scale,
                    "batches": batches,
                    "minimum": [min(v[i] for v in verts) for i in range(3)],
                    "maximum": [max(v[i] for v in verts) for i in range(3)],
                    "raw_vertices": raw_vertices,
                    "texture": texture["provenance"],
                }
            )
        family = {
            "name": name,
            "course": slug,
            "models": list(range(first, 16 + len(self.models))),
            "source_scale": scale,
            "collision_radius_source": radius,
            "collision_radius_units": "Original world-space object.boundingBoxSize; do not multiply by source_scale",
            "billboard": billboard,
            "source_fixed_roll": 0x8000 if roll else 0,
            "fixed_roll_baked": roll,
            "source_texture_scale_baked": 0.5 if half_uv else 1.0,
            "frame_ticks": interval,
            "frame_semantics": semantics,
            "source_refs": list(refs),
        }
        self.families.append(family)
        return family

    def build(self, mole_only=False):
        a = self.asset
        v = self.common_vertices
        add = self.add_family
        add(
            "mole",
            "moo_moo_farm",
            [a("moo_moo_farm", f"gTextureMole{i}") for i in range(1, 8)],
            v(0x62B0, 4),
            0.15,
            6,
            mirrored=True,
            interval=2,
            refs=[
                "src/update_objects.c:func_80081848/func_80081AFC",
                "src/render_objects.c:func_80054D00",
            ],
        )
        add(
            "mole_dirt",
            "moo_moo_farm",
            [a("moo_moo_farm", "gTextureMooMooFarmDirt")],
            v(0x5770, 4),
            0.15,
            0,
            roll=False,
            mirrored=True,
            texture_flags=32 | 64 | 128 | 256,
            refs=["src/update_objects.c:func_8008153C", "src/render_objects.c:func_80054F04"],
        )
        if mole_only:
            return
        add(
            "crab",
            "koopa_troopa_beach",
            [a("koopa_troopa_beach", f"gTextureCrab{i}") for i in range(1, 8)],
            v(0x60B0, 8),
            0.15,
            1,
            interval=2,
            refs=[
                "src/update_objects.c:func_80082870/func_80082B34",
                "src/render_objects.c:render_crabs",
            ],
        )
        hedgehog = a("yoshi_valley", "gTextureYoshiValleyHedgehog")
        add(
            "hedgehog",
            "yoshi_valley",
            [hedgehog] * 2,
            v(0x60B0, 8),
            0.2,
            2,
            interval=5,
            variants=[v(0x60B0, 8), v(0x6130, 8)],
            refs=["src/update_objects.c:func_800833D0", "src/render_objects.c:func_800555BC"],
        )
        add(
            "snowman_head",
            "frappe_snowland",
            [a("frappe_snowland", "gTextureSnowmanHead")],
            v(0x61B0, 8),
            0.1,
            0,
            refs=["src/render_objects.c:render_object_snowmans"],
        )
        add(
            "snowman_body",
            "frappe_snowland",
            [a("frappe_snowland", "gTextureSnowmanBody")],
            v(0x60B0, 8),
            0.1,
            2,
            refs=["src/update_objects.c:func_80083B0C"],
        )
        add(
            "snowman_snow",
            "frappe_snowland",
            [a("frappe_snowland", "gTextureSnow")],
            v(0x5AE0, 4),
            0.1,
            0,
            roll=False,
            refs=[
                "src/update_objects.c:func_80083538/func_8008379C",
                "src/render_objects.c:render_object_snowmans_list_2",
            ],
        )
        # Named source Vtx symbol is stable in this pinned revision.
        plants = self.named_vertices(
            "courses/mario_raceway/course_data.c", "d_course_mario_raceway_piranha_plant_model"
        )
        for slug, palette in [
            ("mario_raceway", "gTLUTMarioRacewayPiranhaPlant"),
            ("royal_raceway", "gTLUTRoyalRacewayPiranhaPlant"),
        ]:
            plant_vertices = self.named_vertices(
                f"courses/{slug}/course_data.c", f"d_course_{slug}_piranha_plant_model"
            )
            require(plant_vertices == plants, "Plant course geometry no longer identical")
            add(
                "piranha_" + slug,
                slug,
                [
                    a("mario_raceway", f"gTexturePiranhaPlant{i}", (slug, palette))
                    for i in range(1, 10)
                ],
                plants,
                1.0,
                5,
                roll=False,
                half_uv=False,
                mirrored=True,
                combine=1024,
                interval=6,
                refs=[
                    "src/actors/piranha_plant/update.inc.c",
                    "src/actors/piranha_plant/render.inc.c",
                ],
            )
        kiwano = self.named_vertices(
            "courses/dks_jungle_parkway/course_data.c", "d_course_dks_jungle_parkway_kiwano_model"
        )
        add(
            "kiwano",
            "dks_jungle_parkway",
            [
                a("dks_jungle_parkway", f"gTextureDksJungleParkwayKiwanoFruit{i}")
                for i in range(1, 4)
            ],
            kiwano,
            1.0,
            2,
            roll=False,
            half_uv=False,
            combine=1024,
            interval=8,
            refs=["src/actors/kiwano_fruit/update.inc.c", "src/actors/kiwano_fruit/render.inc.c"],
        )
        add(
            "bat",
            "banshee_boardwalk",
            [a("banshee_boardwalk", f"gTextureBat{i}") for i in range(1, 5)],
            v(0x62B0, 4),
            0.1,
            3,
            mirrored=True,
            refs=["src/update_objects.c:func_8007DAF8", "src/render_objects.c:func_800524B4"],
        )
        for name, symbol in [("boo", "D_800E4470"), ("boo_mirrored", "D_800E44B0")]:
            add(
                name,
                "banshee_boardwalk",
                [a("banshee_boardwalk", f"gTextureBoo{i:02}") for i in range(1, 30)],
                self.named_vertices("src/update_objects.c", symbol),
                0.15,
                0,
                translucent=True,
                combine=4096,
                semantics="Source view-angle selector, not a uniform loop; frames0..18 plus mirror flag, frame28 special state.",
                refs=[
                    "src/update_objects.c:func_8007C360/func_8007C4A4",
                    "src/render_objects.c:func_800523B8",
                ],
            )
        positions = self.neon_positions()
        names = [
            "Mushroom",
            "Mario",
            "Boo",
            "Peach",
            "Luigi",
            "DonkeyKong",
            "Yoshi",
            "Bowser",
            "Wario",
            "Toad",
        ]
        for i, name in enumerate(names):
            frames = (
                [
                    a(
                        "rainbow_road",
                        "gTextureRainbowRoadNeon" + name,
                        ("rainbow_road", "gTLUTRainbowRoadNeon" + name + str(frame)),
                    )
                    for frame in range(1, 6)
                ]
                if i < 3
                else [a("rainbow_road", "gTextureRainbowRoadNeon" + name)]
            )
            family = add(
                "neon_" + name.lower(),
                "rainbow_road",
                frames,
                v(0x60B0, 8),
                8,
                0,
                refs=[
                    "src/update_objects.c:update_object_neon/update_neon_texture",
                    "src/render_objects.c:render_object_neon",
                ],
                semantics="Changing source TLUT with fixed source CI8 image; exact state-machine timeline included.",
            )
            family["source_position"] = positions[i]
        # Source smoke is I8, used with object-specific primitive/environment
        # colors. Preserve exact intensity as IA16; caller must supply the tint.
        smoke = []
        block = self.decoded(0x132B50)
        for i, offset in enumerate((0x2BC58, 0x2C058, 0x2C458, 0x2C858)):
            intensity = block[offset : offset + 1024]
            require(len(intensity) == 1024, "Smoke bounds")
            smoke.append(
                {
                    "name": f"common_texture_particle_smoke_{i + 1}",
                    "width": 32,
                    "height": 32,
                    "pixels": bytes(channel for value in intensity for channel in (value, value)),
                    "provenance": {
                        "common_segment": "0D",
                        "rom_offset": "0x132B50",
                        "block_offset": hex(offset),
                        "source_format": "i8",
                        "source_sha256": sha(intensity),
                        "conversion": "I8 replicated exactly to IA16; dynamic source tint remains runtime-owned",
                    },
                }
            )
        flame = add(
            "bowser_flame_smoke",
            "bowsers_castle",
            smoke,
            v(0x5AE0, 4),
            1,
            0,
            translucent=True,
            combine=16 | 32768,
            interval=1,
            refs=[
                "src/render_objects.c:render_object_bowser_flame/render_object_bowser_flame_particle/func_8005477C"
            ],
            semantics="Source permanent torch uses environment-to-primitive color, global smoke frame D_80165598; per-particle RGB/alpha from runtime.",
        )
        flame["requires_dynamic_tint"] = True
        puff_raw = block[0x29458:0x29858]
        require(len(puff_raw) == 1024, "Train/ferry smoke bounds")
        puff = {
            "name": "D_0D029458",
            "width": 32,
            "height": 32,
            "pixels": bytes(channel for value in puff_raw for channel in (value, value)),
            "provenance": {
                "common_segment": "0D",
                "rom_offset": "0x132B50",
                "block_offset": "0x29458",
                "source_format": "i8",
                "source_sha256": sha(puff_raw),
                "conversion": "Exact I8 to IA16; original primitive/environment tint belongs to runtime",
            },
        }
        add(
            "train_ferry_smoke",
            "shared",
            [puff],
            v(0x5AE0, 4),
            0.5,
            0,
            translucent=True,
            combine=16 | 32768,
            refs=[
                "src/render_objects.c:render_object_train_smoke_particles/render_object_paddle_boat_smoke_particles"
            ],
            semantics="Original single smoke image explicitly loaded by renderer, independent of object.activeTexture.",
        )["requires_dynamic_tint"] = True

    def placements(self):
        out = {}
        for symbol in (
            "gMoleSpawns",
            "gHedgehogSpawns",
            "gHedgehogPatrolPoints",
            "gSnowmanSpawns",
            "gCrabSpawns",
        ):
            rows = self.source_table(symbol)
            entry = {
                "source_rows": (
                    [r[:3] for r in rows]
                    if symbol in ("gHedgehogSpawns", "gSnowmanSpawns")
                    else rows
                ),
                "source_file": "src/data/some_data.c",
                "coordinate_space": "Unscaled original MK64 coordinates; no atlas translation",
            }
            if symbol in ("gHedgehogSpawns", "gSnowmanSpawns"):
                entry["sections"] = [r[3] & 65535 for r in rows]
            out[symbol] = entry
        out["gMoleSpawns"]["groups"] = [
            {"first": 0, "count": 8},
            {"first": 8, "count": 11},
            {"first": 19, "count": 12},
        ]
        out["gMoleSpawns"]["origin_offset_source"] = [0, -9, 0]
        out["gCrabSpawns"]["row_fields"] = ["start_x", "patrol_x", "start_z", "patrol_z"]
        out["gCrabSpawns"][
            "height_rule"
        ] = "Original ground query +2.5 source units; no authored Y coordinate in this table"
        out["gHedgehogSpawns"]["origin_offset_source"] = [0, 6, 0]
        out["gSnowmanSpawns"]["body_origin_offset_source"] = [0, 3, 0]
        out["gSnowmanSpawns"]["head_origin_offset_source"] = [0, 8, 0]
        expected = {
            "gMoleSpawns": 31,
            "gHedgehogSpawns": 15,
            "gHedgehogPatrolPoints": 15,
            "gSnowmanSpawns": 19,
            "gCrabSpawns": 10,
        }
        require(
            all(len(out[k]["source_rows"]) == count for k, count in expected.items()),
            "Source spawn counts changed",
        )
        for slug in ("mario_raceway", "royal_raceway"):
            symbol = (
                f"d_course_{slug}_piranha_plant_spawns"
                if slug == "mario_raceway"
                else "d_course_royal_raceway_piranha_plant_spawn"
            )
            out[symbol] = {
                "source_rows": [r for r in self.source_table(symbol) if r[0] != -32768],
                "source_file": f"courses/{slug}/course_data.c",
                "coordinate_space": "Unscaled MK64",
            }
        return out

    def finish(self, old):
        require(
            old[:8] == b"MKHZ0001" and struct.unpack_from(">I", old, 8)[0] == 16,
            "Original hazard model identity",
        )
        binary = bytearray(old[:8] + struct.pack(">I", 16 + len(self.models)) + old[12:])
        records = []
        for model in self.models:
            data = bytearray(
                struct.pack(
                    ">IfI6f",
                    model["id"],
                    model["source_scale"],
                    len(model["batches"]),
                    *model["minimum"],
                    *model["maximum"],
                )
            )
            batches = []
            for batch in model["batches"]:
                image = batch["image"]
                data += struct.pack(
                    ">4I",
                    batch["flags"],
                    image["width"],
                    image["height"],
                    len(batch["vertices"]) // 3,
                )
                data += b"".join(struct.pack(">hhhHhh4B", *v) for v in batch["vertices"])
                data += image["pixels"]
                batches.append(
                    {
                        "flags": batch["flags"],
                        "width": image["width"],
                        "height": image["height"],
                        "source_row_start": image["row_start"],
                        "pixels_sha256": sha(image["pixels"]),
                    }
                )
            binary += struct.pack(">I", len(data)) + data
            records.append(
                {k: value for k, value in model.items() if k != "batches"} | {"batches": batches}
            )
        require(binary[12 : len(old)] == old[12:], "Original model records changed")
        if any(f["name"] == "neon_mushroom" for f in self.families):
            timing = sequences()
            for family in self.families:
                if family["name"] in ("neon_mushroom", "neon_mario", "neon_boo"):
                    family.update(timing[family["name"][5:]])
        metadata = {
            "format": "rr64-source-actor-sprites",
            "version": 1,
            "source_commit": SOURCE_REFERENCE_COMMIT,
            "source_rom_sha256": sha(self.rom),
            "private_assets_not_distributable": True,
            "model_file": "models.bin",
            "model_count": 16 + len(self.models),
            "model_sha256": sha(binary),
            "preserved_model_ids": [0, 15],
            "source_units": "MK64; caller applies catalogue source_to_world_scale once",
            "families": self.families,
            "models": records,
            "placements": self.placements(),
            "common_geometry": self.common_geometry,
            "input_hashes": {"source_rom": sha(self.rom)},
            "limitations": [
                "Source state machines and view-dependent frame selection require runtime integration.",
                "This extraction alone is not visual or collision acceptance.",
            ],
        }
        return bytes(binary), metadata


def build(donor, base_bank):
    extractor = Extractor(donor)
    extractor.build()
    return extractor.finish(base_bank)
