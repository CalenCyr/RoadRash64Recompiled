"""ROM-backed original actor display-list interpretation.

Asset and symbol recipes only describe where to read. No donor C checkout or
serialized vertex/texture arrays are used by this module.
"""

from __future__ import annotations
import json, re, struct
from pathlib import Path
from . import source_walk as c
from .common import require
from .assets import Assets
from .source_displaylists import DisplayLists
from .extract_courses import LAYOUT

literal = c.number


class CommonSegments:
    def __init__(self, donor):
        self.inner = donor

    def segment(self, course, number):
        if number == 14:
            return self.inner.rom
        return self.inner.mio0(0x132B50) if number == 13 else self.inner.segment(course, number)

    def read(self, course, address, size):
        block = self.segment(course, address >> 24)
        start = address & 0xFFFFFF
        require(0 <= start and start + size <= len(block), "Actor read outside source segment")
        return block[start : start + size]


class ModelSource:
    def __init__(self, donor, slug):
        self.donor = donor
        self.slug = slug
        self.reader = CommonSegments(donor)
        self.images = Assets(donor)
        recipes = json.loads(
            Path(__file__).with_name("actor_layout.json").read_text(encoding="utf-8")
        )["courses"][slug]["symbols"]
        self.recipes = recipes
        self.vertices = []
        self.names = {}
        self.vertex_ranges = []
        self.assets = self.images.layout["assets"][slug]
        self.textures = {}
        self.lights = {}
        layout = dict(LAYOUT["courses"][slug])
        layout["gfx"] = dict(layout["gfx"])
        for name, info in recipes.items():
            kind = info["type"]
            if kind == "Vtx":
                raw = self.reader.read(slug, info["address"], info["size"])
                rows = [list(v) for v in struct.iter_unpack(">hhhHhh4B", raw)]
                self.names[name] = (len(self.vertices), len(rows))
                self.vertices += rows
                self.vertex_ranges.append((info["address"], info["address"] + info["size"], name))
            elif kind == "texture":
                self.textures[name] = self.texture(info["asset"])
            elif kind == "Lights1":
                raw = self.reader.read(slug, info["address"], 24)
                # Preserve the accepted converter's literal-byte light bake.
                # Reinterpreting hex direction literals as signed here changes
                # existing Yoshi flag shading; a lighting change is separate.
                self.lights[name] = list(raw[:3]) + list(raw[8:11]) + list(raw[16:19])
            elif kind == "Gfx":
                layout["gfx"][name] = info["address"]
        self.arrays = DisplayLists(self.reader, slug, layout)
        for name, info in recipes.items():
            if info["type"] == "texture":
                self.arrays.textures[info["address"]] = name
        # Shared material-only lists are original common-segment commands.
        self.arrays.addresses.update(
            {"D_toads_turnpike_0D005398": 0x0D005398, "D_toads_turnpike_0D0053B0": 0x0D0053B0}
        )
        self.arrays.names.update({v: k for k, v in self.arrays.addresses.items()})
        self.raw = self.arrays.addresses
        self.baked = []

    def texture(self, key):
        w, h, raw = self.images.texture(self.slug, key)
        return dict(
            symbol=key,
            width=w,
            height=h,
            format="ia16" if self.assets[key]["type"] == "ia16" else "rgba16",
            raw=raw,
            source_asset=key,
        )

    def commands(self, name):
        if name == "D_toads_turnpike_0D005398":
            return [
                ("gsDPSetCombineMode", ["G_CC_MODULATEIA"] * 2),
                ("gsDPSetRenderMode", ["G_RM_AA_ZB_OPA_SURF", "G_RM_AA_ZB_OPA_SURF2"]),
                ("gsSPEndDisplayList", []),
            ]
        if name == "D_toads_turnpike_0D0053B0":
            return [
                ("gsDPSetCombineMode", ["G_CC_MODULATEIDECALA"] * 2),
                ("gsDPSetRenderMode", ["G_RM_AA_ZB_TEX_EDGE", "G_RM_AA_ZB_TEX_EDGE2"]),
                ("gsSPEndDisplayList", []),
            ]
        output = []
        for op, args in self.arrays[name]:
            args = list(args)
            if op == "gsSPVertex":
                address = int(args[0], 0)
                count = int(args[1], 0)
                match = next(
                    (
                        (a, n)
                        for a, b, n in self.vertex_ranges
                        if a <= address and address + count * 16 <= b
                    ),
                    None,
                )
                if match is None:
                    # Some original vertex loads span adjacent declared arrays.
                    # The cartridge address/count is the authoritative extent.
                    n = "rom_vertices_" + format(address, "08X") + "_" + str(count)
                    rows = [
                        list(v)
                        for v in struct.iter_unpack(
                            ">hhhHhh4B", self.reader.read(self.slug, address, count * 16)
                        )
                    ]
                    self.names[n] = (len(self.vertices), count)
                    self.vertices += rows
                    self.vertex_ranges.append((address, address + count * 16, n))
                    match = address, n
                start, n = match
                require((address - start) % 16 == 0, "Misaligned actor vertex")
                args[0] = n + "[" + str((address - start) // 16) + "]"
            elif op == "gsSPLight":
                address = int(args[0], 0)
                ambient = args[1] == "2"
                match = next(
                    (
                        n
                        for n, v in self.recipes.items()
                        if v["type"] == "Lights1"
                        and v["address"] + (0 if ambient else 8) == address
                    ),
                    None,
                )
                require(match is not None, "Unknown actor light " + hex(address))
                args[0] = match + (".a" if ambient else ".l")
            output.append((op, args))
        return output

    def export(self, roots, face=None, offsets=None):
        source = self

        class Arrays:
            def __contains__(self, key):
                return key in source.raw or key in (
                    "D_toads_turnpike_0D005398",
                    "D_toads_turnpike_0D0053B0",
                )

            def __getitem__(self, key):
                out = []
                for op, args in source.commands(key):
                    args = list(args)
                    if op == "gsSPVertex":
                        m = re.fullmatch(r"&?(\w+)(?:\[(\d+)\])?", args[0])
                        c.require(m and m[1] in source.names, "Unknown hazard vertex " + args[0])
                        start, count = source.names[m[1]]
                        index = int(m[2] or 0)
                        n = literal(args[1])
                        c.require(index + n <= count, "Named vertex bounds")
                        args[0] = hex(0x04000000 + (start + index) * 16)
                    out.append((op, args))
                return out

        walker = c.Walker(Arrays(), self.vertices, self.textures)
        state = c.Walker.state()
        state["texture_enabled"] = True
        groups = []
        if face is not None:
            name = f"thwomp_face_{face}"
            self.textures[name] = self.texture(f"gTextureThwompFace{face+1}")
            state["geometry_modes"].add("G_LIGHTING")
            state["texture"] = name
            tile = [
                "G_IM_FMT_RGBA",
                "G_IM_SIZ_16b",
                "4",
                "0",
                "G_TX_RENDERTILE",
                "0",
                "G_TX_CLAMP",
                "6",
                "0",
                "G_TX_MIRROR",
                "4",
                "0",
            ]
            state["tiles"]["G_TX_RENDERTILE"] = tile
            state["tile_sizes"]["G_TX_RENDERTILE"] = ["G_TX_RENDERTILE", "0", "0", "60", "252"]
            state["loaded_textures"] = {0: (name, 256, 0, 512)}

        def emit(ids, s, dl):
            material = walker.material(s)
            tex = self.textures.get(material["texture"]) if material["texture_enabled"] else None
            c.require(material["texture_view"] is None, "Hazard TMEM subview unsupported " + dl)
            tile = material["tile"]
            flags = 1 if "G_CULL_BACK" in s["geometry_modes"] else 0
            if any("TEX_EDGE" in x for x in material["render"]):
                flags |= 2
            if any("XLU" in x or "CLD" in x for x in material["render"]):
                flags |= 4
            if tex:
                flags |= 8
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
            c.require(
                material["combine"][0]
                in (
                    "G_CC_SHADE",
                    "G_CC_MODULATEIA",
                    "G_CC_MODULATEI",
                    "G_CC_MODULATERGB",
                    "G_CC_MODULATEIDECALA",
                ),
                "Unsupported combine " + str(material["combine"]),
            )
            if tex and material["combine"][0] in ("G_CC_MODULATEI", "G_CC_MODULATERGB"):
                flags |= 512
            if tex and material["combine"][0] == "G_CC_MODULATEIDECALA":
                flags |= 1024
            key = (flags, material["texture"])
            if not groups or groups[-1]["key"] != key:
                groups.append(
                    {
                        "key": key,
                        "flags": flags,
                        "texture": tex,
                        "vertices": [],
                        "source_lists": [],
                        "source_lighting": False,
                    }
                )
            group = groups[-1]
            if dl not in group["source_lists"]:
                group["source_lists"].append(dl)
            for index in ids:
                v = list(self.vertices[index])
                if "G_LIGHTING" in s["geometry_modes"]:
                    # Source normal bytes are not colors. Bake the original
                    # material's light into colors, leaving no native light
                    # state modified. Dynamic camera light remains a limitation.
                    group["source_lighting"] = True
                    normal = [float(n if n < 128 else n - 256) / 127 for n in v[6:9]]
                    if self.slug == "choco_mountain":
                        ambient, diffuse, direction = 113, 115, (0, -1, 0)
                    else:
                        light = self.donor.rom[0xE5238:0xE5250]  # VRAM800E4638 -> ROME5238.
                        ambient, diffuse = light[0], light[8]
                        direction = (0, 0, 1)
                    shade = round(
                        min(
                            255,
                            max(
                                0,
                                ambient
                                + diffuse * max(0, sum(a * b for a, b in zip(normal, direction))),
                            ),
                        )
                    )
                    v[6:10] = [shade, shade, shade, 255]
                group["vertices"].append(v)

        for i, root in enumerate(roots):
            if root.endswith("_dl_22D28"):
                state["geometry_modes"].discard("G_CULL_BACK")
            begin = sum(len(g["vertices"]) for g in groups)
            before = [len(g["vertices"]) for g in groups]
            walker.walk(root, state, emit)
            if offsets and offsets[i] is not None:
                # Source wheel local transforms at wheelRot0; translation only.
                offset = offsets[i]
                for gi, g in enumerate(groups):
                    for v in g["vertices"][before[gi] if gi < len(before) else 0 :]:
                        for axis in range(3):
                            v[axis] += offset[axis]
        return groups
