"""Read picture, sky and item assets from verified ROM offsets.

The adjacent JSON contains addresses, dimensions and type names only. Pixel and
vertex bytes are always taken from the supplied image, never bundled here.
"""

from __future__ import annotations

import json
import math
from pathlib import Path
import struct

from .common import require
from .route_assembly import LAYOUT as ATLAS


def layout():
    return json.loads(Path(__file__).with_name("asset_layout.json").read_text(encoding="utf-8"))


def item_position(course, geometry, point):
    """Retain the accepted atlas rescaling arithmetic for persistent boxes."""
    cx, cz = geometry["transform"]["source_center_xz"]
    x, y, z = point
    first = ATLAS["stages"][0]
    dx, dy, dz = first["placements"][course]
    scale = first["scale"]
    result = [(x - cx) * scale + dx / 4, -(z - cz) * scale + dy / 4, y * scale + dz / 8]
    for before, after in zip(ATLAS["stages"], ATLAS["stages"][1:]):
        ratio = after["scale"] / before["scale"]
        old = before["placements"][course]
        new = after["placements"][course]
        result = [(result[i] - old[i] / 4) * ratio + new[i] / 4 for i in range(2)] + [
            result[2] * ratio
        ]
    return result


class Assets:
    def __init__(self, donor):
        self.donor = donor
        self.layout = layout()

    def texture(self, course, symbol, palette_override=None):
        info = self.layout["assets"][course][symbol]
        block = self.donor.mio0(info["rom_offset"])
        width, height = info["width"], info["height"]
        fmt = info["type"]
        require(fmt in ("rgba16", "ia16", "ci8"), "Unsupported source image type: " + fmt)
        start = info.get("block_offset", 0)
        size = width * height * (1 if fmt == "ci8" else 2)
        pixels = block[start : start + size]
        require(len(pixels) == size, "Source texture bounds")
        if fmt == "ci8":
            pal_course, pal_symbol = palette_override or (course, info["tlut"])
            palette = self.texture(pal_course, pal_symbol)[2]
            require(len(palette) == 512, "Invalid CI8 palette")
            pixels = b"".join(palette[i * 2 : i * 2 + 2] for i in pixels)
        return width, height, pixels

    def preview(self, course):
        name = "gTextureCoursePreview" + "".join(p.capitalize() for p in course.split("_"))
        # These identifier spellings follow the source extraction table.
        info = self.layout["previews"][name]
        result = self.donor.mio0(info["rom_offset"])
        require(len(result) == info["width"] * info["height"] * 2, "Course preview size")
        # Preserve original pixels. The picture-only runtime material pass
        # makes the preview opaque; changing its asset alpha is unnecessary.
        return bytes(result)

    def sky(self, course, geometry, source=None):
        colors = []
        for info in self.layout["courses"][course]["sky_colors"]:
            values = [
                v & 255 for v in struct.unpack_from(">6H", self.donor.rom, info["rom_offset"])
            ]
            colors.extend((values[:3], values[3:]))
        points = [
            p
            for cell in geometry["cells"]
            for triangle in cell["triangles"]
            for p in triangle["vertices"]
        ]
        low = [min(p[k] / (8 if k == 2 else 4) for p in points) for k in range(3)]
        high = [max(p[k] / (8 if k == 2 else 4) for p in points) for k in range(3)]
        if course == "rainbow_road" and source is not None:
            # Rail shortening is a gameplay adaptation. Keep the established
            # enclosing sky volume based on the original authored rail height.
            # The bound is recomputed from the ROM, not a saved world position.
            height = (
                max(v["position"][1] for v in source["vertices"]) * geometry["transform"]["scale"]
            )
            high[2] = round(height * 8) / 8
        center = [(a + b) * 0.5 for a, b in zip(low, high)]
        half_diagonal = math.dist(low, high) * 0.5
        result = dict(
            version=1,
            stars=False,
            colors=colors,
            textures=[],
            sprites=[],
            center=center,
            radius=max(2048.0, 4 * half_diagonal + 512.0),
        )
        families = {
            "mario_raceway": ("gKalimariDesertClouds", 5),
            "luigi_raceway": ("gLuigiRacewayClouds", 2),
            "moo_moo_farm": ("gYoshiValleyMooMooFarmClouds", 0),
            "yoshi_valley": ("gYoshiValleyMooMooFarmClouds", 0),
            "koopa_troopa_beach": ("gKoopaTroopaBeachClouds", 3),
            "royal_raceway": ("gRoyalRacewayClouds", 4),
            "sherbet_land": ("gSherbetLandClouds", 1),
            "kalimari_desert": ("gKalimariDesertClouds", 5),
            "rainbow_road": ("gToadsTurnpikeRainbowRoadStars", None),
            "toads_turnpike": ("gToadsTurnpikeRainbowRoadStars", None),
            "wario_stadium": ("gWarioStadiumStars", None),
        }
        if course not in families:
            return result
        name, exhaust = families[course]

        def rows(symbol):
            recipe = self.layout["symbols"][symbol]
            start = recipe["rom_offset"]
            values = list(struct.iter_unpack(">4H", self.donor.rom[start : start + recipe["size"]]))
            return values[
                : next((i for i, value in enumerate(values) if value[0] == 65535), len(values))
            ]

        values = rows(name)
        result["stars"] = exhaust is None
        if exhaust is None:
            common = self.donor.mio0(0x132B50)
            textures = [(16, 16, common[0x293D8:0x29458])]
        else:
            pixels = self.donor.mio0(self.layout["sky_clouds"][exhaust])
            require(len(pixels) in (3072, 4096), "Cloud texture block size")
            textures = [(64, 32, pixels[n : n + 1024]) for n in range(0, len(pixels), 1024)]
        result["textures"] = [
            dict(width=w, height=h, intensity4=pixels.hex()) for w, h, pixels in textures
        ]
        yaw = rows("gLuigiRacewayClouds") if course == "mario_raceway" else values
        require(len(yaw) == len(values), "Cloud position table size")
        for i, (_, elevation, size, texture) in enumerate(values):
            require(texture < len(textures), "Cloud texture index")
            result["sprites"].append(
                dict(
                    yaw=yaw[i][0],
                    elevation=elevation - 65536 if elevation >= 32768 else elevation,
                    size=size / 100,
                    texture=texture,
                )
            )
        return result

    def item_model(self):
        common = self.donor.mio0(0x132B50)
        require(len(common) == 0x2D158, "Common asset segment size")
        return b"MKIBOX01" + common[0x1CE8:0x1EE8] + common[0x1EE8:0x2EE8]

    def items(self, course, geometry):
        symbols = self.layout["courses"][course]["symbols"]
        symbol = next(name for name in symbols if name.endswith("_item_box_spawns"))
        info = symbols[symbol]
        data = self.donor.read(course, info["address"], info["size"])
        rows = list(struct.iter_unpack(">4h", data))
        require(rows[-1] == (-32768, 0, 0, 0), "Missing item box sentinel")
        transform = geometry["transform"]
        scale = transform["scale"]
        cx, cz = transform["source_center_xz"]
        dx, dy, dz = transform["atlas_translation_world"]

        def point(x, y, z):
            return item_position(course, geometry, [x, y, z])

        boxes = [
            dict(
                id=i,
                position=point(x, y + 8.66, z),
                radius=5.5 * scale,
                kind=2,
                source_position=[x, y, z],
                source_param=param,
                source_symbol=symbol,
                source_index=i,
            )
            for i, (x, y, z, param) in enumerate(rows[:-1])
        ]
        return dict(
            format="rr64-course-items",
            version=1,
            course=course,
            source_to_world_scale=scale,
            boxes=boxes,
            coordinate_space="RR64 rider world; atlas translation already applied",
        )

    @staticmethod
    def special_koopa_item(items, geometry, source_position):
        """The elevated box is initialized in code, outside the spawn table."""
        transform = geometry["transform"]
        scale = transform["scale"]
        cx, cz = transform["source_center_xz"]
        dx, dy, dz = transform["atlas_translation_world"]
        x, y, z = source_position
        items["boxes"].append(
            dict(
                id=len(items["boxes"]),
                position=item_position("koopa_troopa_beach", geometry, source_position),
                radius=5.5 * scale,
                kind=5,
                source_position=source_position,
                source_index=0,
                source_symbol="init_actor_hot_air_balloon_item_box",
                special="static elevated actor",
            )
        )
