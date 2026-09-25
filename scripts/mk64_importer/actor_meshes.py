"""ROM-only posed actor meshes and complete skeletal animation clips."""

from pathlib import Path
import json, math, re, struct
from .actor_base import ModelSource as Base
from . import source_walk as c
from .common import digest, f32
from . import SOURCE_REFERENCE_COMMIT

DONOR = None
SINE = []
COSINE = []


def signed(n):
    return n - 65536 if n >= 32768 else n


def ident():
    return [[float(x == y) for y in range(4)] for x in range(4)]


def matrix(pos, ang):
    sx, sy, sz = [SINE[(a & 65535) >> 4] for a in ang]
    cx, cy, cz = [COSINE[(a & 65535) >> 4] for a in ang]
    # Source animation.c:36 mtxf_translate_rotate2, row-vector convention.
    mul = lambda a, b: f32(a * b)
    m = [
        [mul(cy, cz), mul(cy, sz), -sy, 0.0],
        [
            f32(mul(mul(sx, sy), cz) - mul(cx, sz)),
            f32(mul(mul(sx, sy), sz) + mul(cx, cz)),
            mul(sx, cy),
            0.0,
        ],
        [
            f32(mul(mul(cx, sy), cz) + mul(sx, sz)),
            f32(mul(mul(cx, sy), sz) - mul(sx, cz)),
            mul(cx, cy),
            0.0,
        ],
        [float(pos[0]), float(pos[1]), float(pos[2]), 1.0],
    ]
    # The original renderer submits a 16.16 matrix at each limb.
    return [[math.trunc(x * 65536) / 65536 for x in row] for row in m]


def multiply(a, b):
    return [[sum(a[i][k] * b[k][j] for k in range(4)) for j in range(4)] for i in range(4)]


def transform(v, m, w=1):
    return [sum(v[k] * m[k][j] for k in range(3)) + w * m[3][j] for j in range(3)]


class Source(Base):
    def __init__(self, slug):
        super().__init__(DONOR, slug)
        self.current_light = None
        self.current_ambient = None
        self.used_lights = set()
        self.pose = ident()
        self.unit = 1

    def export_parts(self, parts, lighting=False, cull=True, shade=False):
        source = self

        class Arrays:
            def __contains__(self, key):
                return key.lstrip("&") in source.raw

            def __getitem__(self, key):
                for op, original in source.commands(key.lstrip("&")):
                    args = list(original)
                    if op == "gsDPLoadTextureBlock":
                        (
                            texture,
                            fmt,
                            size,
                            width,
                            height,
                            pal,
                            cms,
                            cmt,
                            masks,
                            maskt,
                            shifts,
                            shiftt,
                        ) = args
                        w, h = c.number(width), c.number(height)
                        c.require(size == "G_IM_SIZ_16b", "Block macro size")
                        line = (w * 2 + 7) // 8
                        dxt = (2048 + line - 1) // line
                        yield "gsDPSetTextureImage", [fmt, size, "1", texture]
                        yield "gsDPSetTile", [
                            fmt,
                            size,
                            "0",
                            "0",
                            "G_TX_LOADTILE",
                            pal,
                            cmt,
                            maskt,
                            shiftt,
                            cms,
                            masks,
                            shifts,
                        ]
                        yield "gsDPLoadBlock", ["G_TX_LOADTILE", "0", "0", str(w * h - 1), str(dxt)]
                        yield "gsDPSetTile", [
                            fmt,
                            size,
                            str(line),
                            "0",
                            "G_TX_RENDERTILE",
                            pal,
                            cmt,
                            maskt,
                            shiftt,
                            cms,
                            masks,
                            shifts,
                        ]
                        yield "gsDPSetTileSize", [
                            "G_TX_RENDERTILE",
                            "0",
                            "0",
                            str((w - 1) * 4),
                            str((h - 1) * 4),
                        ]
                        continue
                    if op == "gsSPDisplayList":
                        args[0] = args[0].lstrip("&")
                    if op == "gsSPVertex":
                        m = re.fullmatch(r"&?(\w+)(?:\[(\d+)\])?", args[0])
                        c.require(m and m[1] in source.names, "Unknown vertex " + args[0])
                        start, count = source.names[m[1]]
                        index = int(m[2] or 0)
                        n = c.number(args[1])
                        c.require(index + n <= count, "Vertex range")
                        args[0] = hex(0x04000000 + (start + index) * 16)
                    if op == "gsSPLight":
                        m = re.fullmatch(r"&?(\w+)\.([la])", args[0])
                        c.require(m and m[1] in source.lights, "Unknown source light " + str(args))
                        light = source.lights[m[1]]
                        source.used_lights.add(m[1])
                        if m[2] == "l":
                            source.current_light = light[3:]
                        else:
                            source.current_ambient = light[:3]
                    if op == "gsSPSetLights1":
                        c.require(args[0] in source.lights, "Unknown source lights " + args[0])
                        light = source.lights[args[0]]
                        source.current_ambient = light[:3]
                        source.current_light = light[3:]
                        source.used_lights.add(args[0])
                    yield op, args

        walker = c.Walker(Arrays(), self.vertices, self.textures)
        state = c.Walker.state(cull=cull, shade=shade)
        if lighting:
            state["geometry_modes"].add("G_LIGHTING")
        self.current_light = [255, 255, 255, 0, 0, 120]
        self.current_ambient = [115, 115, 115]
        self.used_lights = set()
        groups = []

        def emit(ids, s, dl):
            material = walker.material(s)
            combine = material["combine"][0]
            c.require(
                material["combine"][0] == material["combine"][1], "Different two-cycle combines"
            )
            c.require(
                combine
                in (
                    "G_CC_SHADE",
                    "G_CC_MODULATEIA",
                    "G_CC_MODULATEI",
                    "G_CC_MODULATERGB",
                    "G_CC_MODULATEIDECALA",
                    "G_CC_BLENDRGBA",
                    "G_CC_DECALRGB",
                ),
                "Unsupported actor combiner " + combine,
            )
            tex = self.textures.get(material["texture"]) if material["texture_enabled"] else None
            c.require(material["texture_view"] is None, "Actor TMEM subview unsupported " + dl)
            flags = int("G_CULL_BACK" in s["geometry_modes"])
            c.require("G_CULL_FRONT" not in s["geometry_modes"], "Unexpected front culling")
            if any("TEX_EDGE" in x for x in material["render"]):
                flags |= 2
            if any("XLU" in x or "CLD" in x for x in material["render"]):
                flags |= 4
            if any("XLU_DECAL" in x for x in material["render"]):
                flags |= 8192
            if tex:
                flags |= 8
                tile = material["tile"]
                if tex["format"] == "ia16":
                    flags |= 16
                if "MIRROR" in tile[9] and "NOMIRROR" not in tile[9]:
                    flags |= 32
                if "CLAMP" in tile[9]:
                    flags |= 64
                if "MIRROR" in tile[6] and "NOMIRROR" not in tile[6]:
                    flags |= 128
                if "CLAMP" in tile[6]:
                    flags |= 256
                if combine in ("G_CC_MODULATEI", "G_CC_MODULATERGB", "G_CC_DECALRGB"):
                    flags |= 512
                if combine == "G_CC_MODULATEIDECALA":
                    flags |= 1024
                if combine == "G_CC_BLENDRGBA":
                    flags |= 2048
            key = (flags, material["texture"], combine)
            if not groups or groups[-1]["key"] != key:
                groups.append(
                    dict(
                        key=key,
                        flags=flags,
                        texture=tex,
                        vertices=[],
                        source_lists=[],
                        source_lighting=False,
                    )
                )
            g = groups[-1]
            if dl not in g["source_lists"]:
                g["source_lists"].append(dl)
            for idx in ids:
                v = list(self.vertices[idx])
                p = transform(v[:3], self.pose)
                v[:3] = [round(n * self.unit) for n in p]
                c.require(all(-32768 <= n <= 32767 for n in v[:3]), "Baked position exceeds S16")
                if tex:
                    scales = [c.number(x) for x in material["texture_scale"][:2]]
                    # Existing adapter emits FFFF full scale. Preserve source
                    # half scales in coordinates; FFFF remains identity.
                    for axis in range(2):
                        v[4 + axis] = round(v[4 + axis] * scales[axis] / 65535)
                if "G_LIGHTING" in s["geometry_modes"]:
                    normal = transform(
                        [
                            signed(n) if n > 255 else n - 256 if n >= 128 else n
                            for n in self.vertices[idx][6:9]
                        ],
                        self.pose,
                        0,
                    )
                    l = self.current_light
                    a = self.current_ambient
                    dot = max(0, sum(normal[k] * l[k + 3] for k in range(3)) / (127 * 127))
                    v[6:9] = [round(min(255, max(0, a[k] + l[k] * dot))) for k in range(3)]
                    # Opaque source geometry can have arbitrary normal-alpha0;
                    # preserve it: the source opaque render mode ignores alpha.
                    g["source_lighting"] = True
                if "G_TEXTURE_GEN" in s["geometry_modes"]:
                    c.require(
                        tex and "G_TEXTURE_GEN_LINEAR" not in s["geometry_modes"],
                        "Unsupported texgen variant",
                    )
                    normal = transform(
                        [n - 256 if n >= 128 else n for n in self.vertices[idx][6:9]], self.pose, 0
                    )
                    length = math.sqrt(sum(n * n for n in normal))
                    c.require(length > 0, "Zero texgen normal")
                    scale = [c.number(x) for x in material["texture_scale"][:2]]
                    v[4:6] = [
                        round((normal[axis] / length + 1) * 16384 * scale[axis] / 65536)
                        for axis in range(2)
                    ]
                    g["texture_gen_baked"] = True
                if combine == "G_CC_DECALRGB":
                    v[6:9] = [255] * 3  # Exact RGB equivalent with MODULATEI.
                g["vertices"].append(v)

        for root, pose, light in parts:
            self.pose = pose
            if light is not None:
                self.current_ambient = light[:3]
                self.current_light = light[3:]
            walker.walk(root.lstrip("&"), state, emit)
        return groups


SOURCES = {}
MODELS = []
FAMILIES = []
VALIDATION = []
ASSEMBLIES = []


def source(slug):
    if slug not in SOURCES:
        SOURCES[slug] = Source(slug)
    return SOURCES[slug]


def add_model(
    slug, name, parts, scale=1, unit=1, lighting=False, cull=True, shade=False, **metadata
):
    s = source(slug)
    s.unit = unit
    groups = s.export_parts(parts, lighting, cull, shade)
    vs = [v for g in groups for v in g["vertices"]]
    c.require(vs, "Empty actor " + name)
    model = dict(
        id=len(MODELS),
        name=name,
        source_course=slug,
        source_scale=scale / unit,
        bake_units_per_source_unit=unit,
        minimum=[min(v[i] for v in vs) for i in range(3)],
        maximum=[max(v[i] for v in vs) for i in range(3)],
        groups=groups,
        source_roots=[p[0] for p in parts],
        source_lights=sorted(s.used_lights),
        **metadata,
    )
    MODELS.append(model)
    return model["id"]


def static(slug, name, roots, **kwargs):
    return add_model(slug, name, [(r, ident(), None) for r in roots], **kwargs)


def armature(slug, family, armname, animlist, scale):
    s = source(slug)

    def raw(name):
        record = s.recipes[name]
        return s.reader.read(slug, record["address"], record["size"])

    def named(address, kind):
        name = next(
            (n for n, r in s.recipes.items() if r["address"] == address and r["type"] == kind), None
        )
        c.require(name is not None, "Unknown animation pointer " + hex(address))
        return name

    instructions = raw(armname)
    offset = 0
    parsed = []
    while offset < len(instructions):
        op, size = struct.unpack_from(">II", instructions, offset)
        c.require(
            size in (2, 7) and offset + size * 4 <= len(instructions),
            "Invalid armature instruction",
        )
        if op in (1, 2, 3):
            c.require(size == 2, "Invalid simple armature instruction")
            parsed.append(({1: "DISABLE_AUTOMATIC_POP", 2: "POP_MATRIX", 3: "STOP"}[op], []))
        elif op == 0:
            c.require(size == 7, "Invalid pose instruction")
            _, pointer, x, y, z = struct.unpack_from(">IIiii", instructions, offset + 8)
            parsed.append(
                ("RENDER_MODEL_AT", [s.arrays.name(pointer), str(x), str(y), str(z)])
                if pointer
                else ("ADD_POS", list(map(str, (x, y, z))))
            )
        else:
            raise ValueError("Unknown armature operation")
        offset += size * 4
    c.require(parsed and parsed[-1][0] == "STOP", "Armature missing stop")
    clips = []
    for clip, (pointer,) in enumerate(struct.iter_unpack(">I", raw(animlist))):
        animation = named(pointer, "Animation")
        _, _, length, limbs, angle_pointer, matrix_pointer = struct.unpack(
            ">IIHHII", raw(animation)
        )
        angle_name = named(angle_pointer, "angle-array")
        matrix_name = named(matrix_pointer, "animation-vectors")
        angles = [v[0] for v in struct.iter_unpack(">h", raw(angle_name))]
        pairs = list(struct.iter_unpack(">HH", raw(matrix_name)))
        c.require(len(pairs) % 3 == 0 and length > 0, "Animation vector layout")
        specs = [pairs[i : i + 3] for i in range(0, len(pairs), 3)]
        frames = []
        signatures = []

        def vector(spec, frame):
            v = []
            for n, start in spec:
                index = start + (frame if frame < n else 0)
                c.require(0 <= index < len(angles), "Animation angle range")
                v.append(angles[index])
            return v

        for frame in range(length):
            stack = [ident()]
            hold = False
            first = True
            limb = 1
            parts = []
            maxdepth = 0
            translation = vector(specs[0], frame)
            for op, args in parsed:
                if op == "DISABLE_AUTOMATIC_POP":
                    hold = True
                elif op == "POP_MATRIX":
                    c.require(len(stack) > 1, "Armature stack underflow")
                    stack.pop()
                elif op == "STOP":
                    break
                else:
                    c.require(
                        op in ("ADD_POS", "RENDER_MODEL", "RENDER_MODEL_AT"),
                        "Unknown armature op " + op,
                    )
                    if not hold:
                        c.require(len(stack) > 1, "Automatic pop underflow")
                        stack.pop()
                    root = None if op == "ADD_POS" else args[0].lstrip("&")
                    pos = list(
                        map(
                            c.number,
                            (
                                args
                                if op == "ADD_POS"
                                else args[1:] if op == "RENDER_MODEL_AT" else ["0"] * 3
                            ),
                        )
                    )
                    if first:
                        pos = [p + t for p, t in zip(pos, translation)]
                        first = False
                    c.require(limb < len(specs), "Not enough animation limb vectors")
                    pose = multiply(matrix(pos, vector(specs[limb], frame)), stack[-1])
                    stack.append(pose)
                    maxdepth = max(maxdepth, len(stack) - 1)
                    if root:
                        parts.append((root, pose, None))
                    limb += 1
                    hold = False
            c.require(limb == len(specs) and len(stack) == 1, "Armature vector/stack not balanced")
            frames.append(
                add_model(
                    slug,
                    f"{family}_clip{clip}_frame{frame}",
                    parts,
                    scale=scale,
                    unit=8,
                    lighting=True,
                    shade=True,
                    family=family,
                    clip=clip,
                    frame=frame,
                    source_animation=animation,
                )
            )
            signatures.append(
                digest(
                    b"".join(
                        struct.pack(">hhh", *v[:3])
                        for g in MODELS[-1]["groups"]
                        for v in g["vertices"]
                    )
                )
            )
            VALIDATION.append(
                dict(
                    family=family,
                    clip=clip,
                    frame=frame,
                    limbs=limb - 1,
                    stack_end=len(stack) - 1,
                    maxdepth=maxdepth,
                    triangles=sum(len(g["vertices"]) // 3 for g in MODELS[-1]["groups"]),
                    matrices=[dict(root=p[0], matrix=p[1]) for p in parts],
                )
            )
        c.require(
            len(set(signatures)) > 1 or length == 1, "Animation unintentionally static " + family
        )
        clips.append(
            dict(
                index=clip,
                source_animation=animation,
                frame_count=length,
                model_ids=frames,
                unique_vertex_frames=len(set(signatures)),
            )
        )
    FAMILIES.append(
        dict(
            name=family,
            source_course=slug,
            source_armature=armname,
            source_animation_list=animlist,
            clips=clips,
            source_scale=scale,
            source_basis="MK64 local XYZ; runtime converts to RR X,-Z,Y",
            baked_precision_source_units=0.0625,
        )
    )


def serialize():
    data = bytearray(b"MKHZ0001" + struct.pack(">I", len(MODELS)))
    reports = []
    for m in MODELS:
        rec = bytearray(
            struct.pack(
                ">IfI6f", m["id"], m["source_scale"], len(m["groups"]), *m["minimum"], *m["maximum"]
            )
        )
        summary = []
        for g in m["groups"]:
            tex = g["texture"]
            w, h = (tex["width"], tex["height"]) if tex else (0, 0)
            pixels = tex["raw"] if tex else b""
            c.require(w * h <= 2048 and len(g["vertices"]) % 3 == 0, "Material dimensions/count")
            vtx = b"".join(struct.pack(">hhhHhh4B", *v) for v in g["vertices"])
            rec += struct.pack(">IIII", g["flags"], w, h, len(g["vertices"]) // 3) + vtx + pixels
            summary.append(
                dict(
                    flags=g["flags"],
                    triangles=len(g["vertices"]) // 3,
                    texture=tex["source_asset"] if tex else None,
                    source_lists=g["source_lists"],
                    source_lighting_baked=g["source_lighting"],
                    texture_gen_baked=g.get("texture_gen_baked", False),
                )
            )
        data += struct.pack(">I", len(rec)) + rec
        reports.append(
            {k: v for k, v in m.items() if k != "groups"}
            | dict(groups=summary, triangles=sum(g["triangles"] for g in summary))
        )
    manifest = dict(
        format="rr64-course-hazard-models",
        version=1,
        source_commit=SOURCE_REFERENCE_COMMIT,
        file="actor-models.bin",
        sha256=digest(data),
        bytes=len(data),
        models=reports,
        families=FAMILIES,
        assemblies=ASSEMBLIES,
        private_assets_not_distributable=True,
        coordinate_contract="Vertices remain MK64 local XYZ; posed armatures are stored at8x precision with model source_scale dividedby8. Multiply by source_scale and per-instance scale, then course source_to_world_scale exactlyonce.",
        light_bake={
            "fallback": "src/code_800029B0.c:66 D800DC610[1] ambient115,diffuse255,direction(0,0,120), set by render_course_actors before actorloop",
            "per_material": "Every reached gsSPLight/gsSPSetLights1 resolves actual source Lights1 RGB/direction. Fish explicit sourceROM E51C0 array, race downward direction(0,-120,0).",
            "equation": "clamp(ambient+diffuse*max(0,dot(posed_normal,source_direction)/(127*127))); reference actor orientation0",
            "texture_gen": "Reference-lookAt X/Y normal sphere-map bake, source 07C0 scale; original gold/metal reflection pixels; no live camera reflection.",
        },
        limitations=[
            "Source per-material ambient/diffuse light colors and limb normals are baked at actor orientation0; directional lighting will not react to live actor yaw/camera.",
            "Pose vertex rounding maximum.0625 sourceunit before authored objectscale; no omitted clip frames.",
            "Chomp texture generation uses a reference lookAt sphere map; live camera-dependent reflections are not ported.",
            "Reflections/shadows/distance LOD and sound are separate runtime effects; only requested primary model assets here.",
        ],
        provenance=dict(rom_sha256=digest(DONOR.rom)),
    )
    return bytes(data), manifest


def build(donor):
    global DONOR, SINE, COSINE, SOURCES, MODELS, FAMILIES, VALIDATION, ASSEMBLIES
    DONOR = donor
    SOURCES = {}
    MODELS = []
    FAMILIES = []
    VALIDATION = []
    ASSEMBLIES = []
    layout = json.loads(Path(__file__).with_name("actor_layout.json").read_text(encoding="utf-8"))[
        "symbols"
    ]
    tables = []
    for name in ("gSineTable", "gCosineTable"):
        spec = layout[name]
        tables.append(
            list(
                struct.unpack(
                    ">" + str(spec["count"]) + "f",
                    donor.rom[spec["rom_offset"] : spec["rom_offset"] + spec["size"]],
                )
            )
        )
    SINE, COSINE = tables
    if len(SINE) == 1024:
        SINE += COSINE[:3072]
    c.require(len(SINE) >= 4096 and len(COSINE) >= 4096, "Original trig table size")
    # Highest-impact rigid assets first; output can be inspected while jointed
    # frame extraction evolves without touching previous pack.
    static(
        "yoshi_valley", "yoshi_egg", ["d_course_yoshi_valley_dl_16D70"], lighting=True, shade=True
    )
    for suffix in ["both_inactive", "right_active", "left_active"]:
        static(
            "kalimari_desert",
            "crossing_" + suffix,
            ["d_course_kalimari_desert_dl_crossing_" + suffix],
            lighting=True,
            cull=False,
            shade=True,
        )
    static(
        "dks_jungle_parkway",
        "ferry_hull",
        ["d_course_dks_jungle_parkway_boat_dl", "d_course_dks_jungle_parkway_railings_dl"],
        lighting=True,
        shade=True,
    )
    static(
        "dks_jungle_parkway",
        "ferry_paddle",
        ["d_course_dks_jungle_parkway_paddle_wheel_dl"],
        lighting=True,
        cull=False,
        shade=True,
    )
    static("mario_raceway", "mario_sign", ["d_course_mario_raceway_dl_sign"])
    static("wario_stadium", "wario_sign", ["d_course_wario_stadium_dl_sign"])
    static(
        "luigi_raceway",
        "hot_air_balloon",
        ["d_course_luigi_raceway_dl_F960"],
        lighting=True,
        shade=True,
    )
    for suffix in ["22DB8", "22D70"]:
        static(
            "kalimari_desert",
            "train_wheel_" + suffix,
            ["d_course_kalimari_desert_dl_22D28", "d_course_kalimari_desert_dl_" + suffix],
            cull=False,
        )
    for name, roots, wheels, speed in [
        (
            "engine",
            ["1C0F0", "1B978"],
            [
                (32, 6, "22DB8", 0),
                (16, 6, "22DB8", 2),
                (-12, 12, "22D70", 6),
                (-34, 12, "22D70", 4),
            ],
            -9,
        ),
        ("tender", ["1F228"], [(8, 6, "22DB8", 0), (-8, 6, "22DB8", 6)], -7),
        (
            "passenger",
            ["20A20", "20A08"],
            [(28, 6, "22DB8", 0), (12, 6, "22DB8", 3), (-8, 6, "22DB8", 8), (-24, 6, "22DB8", 2)],
            -9,
        ),
    ]:
        main_id = static(
            "kalimari_desert",
            "train_" + name + "_body",
            [f"d_course_kalimari_desert_dl_{r}" for r in roots],
        )
        children = []
        for z, y, root, phase in wheels:
            wheel_id = next(m["id"] for m in MODELS if m["name"] == "train_wheel_" + root)
            for x in (17, -17):
                children.append(
                    dict(
                        model=wheel_id,
                        offset_source=[x, y, z],
                        rotation_axis="X",
                        phase_degrees=phase,
                    )
                )
        ASSEMBLIES.append(
            dict(
                name="train_" + name,
                body_model=main_id,
                wheel_children=children,
                degrees_per_source_update=speed,
                source="src/actors/train/render.inc.c + update.inc.c",
            )
        )
    ASSEMBLIES.append(
        dict(
            name="ferry",
            body_model=4,
            wheel_children=[
                dict(model=5, offset_source=[0, 16, -255], rotation_axis="X", phase_degrees=0)
            ],
            degrees_per_source_update=5,
            source="src/actors/paddle_boat/render.inc.c:48â€“55",
        )
    )
    fish = []
    for i, root in enumerate(["7B38", "7978", "78C0", "7650"]):
        light = DONOR.rom[0xE51C0 + 24 * i : 0xE51D8 + 24 * i]
        fish.append(
            (
                "d_course_banshee_boardwalk_dl_" + root,
                ident(),
                list(light[:3]) + list(light[8:11]) + [0, -120, 0],
            )
        )
    add_model("banshee_boardwalk", "race_cheep_cheep", fish, scale=2, lighting=True, shade=True)
    armature(
        "sherbet_land",
        "penguin",
        "d_course_sherbet_land_unk_data1",
        "d_course_sherbet_land_unk_data11",
        1,
    )
    armature("rainbow_road", "chain_chomp", "d_rainbow_road_unk4", "d_rainbow_road_unk3", 0.03)
    armature(
        "koopa_troopa_beach",
        "seagull",
        "d_course_koopa_troopa_beach_unk4",
        "d_course_koopa_troopa_beach_unk_data5",
        0.2,
    )
    armature(
        "yoshi_valley",
        "yoshi_flag",
        "d_course_yoshi_valley_unk5",
        "d_course_yoshi_valley_unk4",
        0.027,
    )
    return serialize()
