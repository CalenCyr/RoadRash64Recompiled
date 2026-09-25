"""Bounded neutral material/triangle interpreter for ROM course decoding."""

import json, re
from .source_rom import require


def number(value):
    require(
        re.fullmatch(r"-?(?:0[xX][0-9a-fA-F]+|[0-9]+)", str(value)) is not None,
        "Expected integer operand",
    )
    return int(value, 16 if "x" in str(value).lower() else 10)


def canonical(value):
    return json.dumps(value, sort_keys=True, separators=(",", ":"))


def orientation_key(indices):
    # Cyclic rotation preserves winding; reversed winding is deliberately distinct.
    a, b, c = indices
    return min((a, b, c), (b, c, a), (c, a, b))


class Walker:
    def __init__(self, arrays, vertices, textures):
        self.arrays, self.vertices, self.textures = arrays, vertices, textures
        self.visited = set()
        self.command_count = 0

    @staticmethod
    def state(edge=False, cull=True, shade=False):
        return {
            "cache": [None] * 32,
            "texture": None,
            "texture_scale": ["0xFFFF", "0xFFFF", "0", "G_TX_RENDERTILE", "G_ON"],
            "texture_enabled": True,
            "tiles": {},
            "tile_sizes": {},
            "load_block": None,
            "texture_lut": None,
            "lights": None,
            "combine": [
                "G_CC_SHADE" if shade else "G_CC_MODULATEIDECALA" if edge else "G_CC_MODULATEIA"
            ]
            * 2,
            "render": (
                ["G_RM_AA_ZB_TEX_EDGE", "G_RM_AA_ZB_TEX_EDGE2"]
                if edge
                else ["G_RM_AA_ZB_OPA_SURF", "G_RM_AA_ZB_OPA_SURF2"]
            ),
            "geometry_modes": {"G_ZBUFFER", "G_SHADE", "G_SHADING_SMOOTH"}
            | ({"G_CULL_BACK"} if cull else set()),
        }

    def material(self, state):
        textured = state["texture_enabled"] and state["combine"] != ["G_CC_SHADE"] * 2
        tile = state["tiles"].get("G_TX_RENDERTILE")
        tile_size = state["tile_sizes"].get("G_TX_RENDERTILE")
        texture = None
        view = None
        if textured:
            require(tile and tile_size, "Textured primitive lacks render tile")
            # Wrap/mirror period follows masks, independently of scrolling
            # lower tile origins. These course render tiles all use masks.
            require(
                tile[10] != "G_TX_NOMASK" and tile[7] != "G_TX_NOMASK",
                "Unmasked render tile unsupported",
            )
            width = 1 << number(tile[10])
            height = 1 << number(tile[7])
            begin = number(tile[3])
            end = begin + ((height - 1) * number(tile[2]) * 8 + width * 2 + 7) // 8
            matches = [
                (start, v)
                for start, v in state.get("loaded_textures", {}).items()
                if start <= begin and v[1] >= end
            ]
            require(len(matches) == 1, "Unresolved render TMEM range")
            start, loaded = matches[0]
            texture = loaded[0]
            require(texture in self.textures, f"Unresolved active texture {texture}")
            source = self.textures[texture]
            if (
                begin != loaded[2]
                or width != source["width"]
                or height != source["height"]
                or number(tile[2]) * 8 != width * 2
            ):
                view = {
                    "load_start": loaded[2],
                    "load_dxt": loaded[3],
                    "render_start": begin,
                    "render_line": number(tile[2]),
                    "width": width,
                    "height": height,
                }
        return {
            "texture": texture,
            "texture_enabled": textured,
            "texture_scale": state["texture_scale"],
            "tile": tile,
            "texture_view": view,
            "tile_size": tile_size,
            "load_block": state["load_block"],
            "texture_lut": state["texture_lut"],
            "combine": state["combine"],
            "render": state["render"],
            "geometry_modes": sorted(state["geometry_modes"]),
            # NumLights cannot affect these primitives while lighting is off.
            "lights": state["lights"] if "G_LIGHTING" in state["geometry_modes"] else None,
            "extra_state": dict(state.get("extra_state", {})),
        }

    def walk(self, name, state, emit, stack=()):
        require(
            name in self.arrays and name not in stack and len(stack) < 64,
            f"Missing/recursive display list {name}",
        )
        self.visited.add(name)
        for command, args in self.arrays[name]:
            self.command_count += 1
            require(self.command_count < 2_000_000, "Display-list work limit exceeded")
            if command == "gsSPEndDisplayList":
                return
            if command == "gsSPDisplayList":
                require(len(args) == 1, "Bad call")
                self.walk(args[0], state, emit, stack + (name,))
            elif command == "gsSPVertex":
                address, count, first = map(number, args)
                require(
                    address >> 24 == 4 and (address & 0xFFFFFF) % 16 == 0,
                    "Runtime terrain vertex must resolve to expanded segment 04",
                )
                start = (address & 0xFFFFFF) // 16
                require(
                    1 <= count <= 32
                    and 0 <= first <= 32 - count
                    and 0 <= start <= len(self.vertices) - count,
                    "Vertex load out of bounds",
                )
                state["cache"][first : first + count] = range(start, start + count)
            elif command in ("gsSP1Triangle", "gsSP2Triangles"):
                values = list(map(number, args))
                require(len(values) == (4 if command == "gsSP1Triangle" else 8), "Bad triangle")
                for i in range(0, len(values), 4):
                    a, b, c, flag = values[i : i + 4]
                    require(
                        flag == 0 and all(0 <= x < 32 for x in (a, b, c)), "Bad triangle indices"
                    )
                    ids = [state["cache"][x] for x in (a, b, c)]
                    require(None not in ids, "Triangle references unloaded vertex")
                    emit(ids, state, name)
            elif command == "gsDPSetTextureImage":
                require(len(args) == 4, "Bad texture image")
                state["texture"] = args[3]
            elif command == "gsDPSetTile":
                require(len(args) == 12, "Bad tile")
                state["tiles"][args[4]] = args
            elif command == "gsDPSetTileSize":
                require(len(args) == 5, "Bad tile size")
                state["tile_sizes"][args[0]] = args
            elif command == "gsSPTexture":
                require(len(args) == 5 and args[4] in ("G_ON", "G_OFF"), "Bad texture switch")
                state["texture_scale"] = args
                state["texture_enabled"] = args[4] == "G_ON"
            elif command in ("gsSPSetGeometryMode", "gsSPClearGeometryMode"):
                modes = {s.strip() for s in args[0].split("|")}
                if command == "gsSPSetGeometryMode":
                    state["geometry_modes"].update(modes)
                else:
                    state["geometry_modes"].difference_update(modes)
            elif command == "gsDPSetCombineMode":
                require(len(args) == 2, "Bad combine mode")
                state["combine"] = args
            elif command == "gsDPSetRenderMode":
                require(len(args) == 2, "Bad render mode")
                state["render"] = args
            elif command == "gsDPLoadBlock":
                require(len(args) == 5 and args[0] in state["tiles"], "Invalid load tile")
                loadtile = state["tiles"][args[0]]
                require(
                    loadtile[1] == "G_IM_SIZ_16b" and number(args[1]) == number(args[2]) == 0,
                    "Unsupported texture load format/offset",
                )
                begin = number(loadtile[3])
                end = begin + (number(args[3]) + 1) * 2 // 8
                require(0 <= begin < end <= 512, "Texture load exceeds TMEM")
                loaded = state.setdefault("loaded_textures", {})
                for old in list(loaded):
                    if old < end and loaded[old][1] > begin:
                        del loaded[old]
                loaded[begin] = (state["texture"], end, begin, number(args[4]))
                state["load_block"] = args
            elif command == "gsSPNumLights":
                state["lights"] = args
            elif command == "gsDPSetTextureLUT":
                require(len(args) == 1, "Bad LUT state")
                state["texture_lut"] = args[0]
            elif command in (
                "gsDPSetCycleType",
                "gsDPSetFogColor",
                "gsSPFogFactor",
                "gsDPSetTextureFilter",
                "gsDPSetTexturePersp",
                "gsDPSetAlphaCompare",
                "gsSPLight",
                "gsSPSetLights1",
            ):
                state.setdefault("extra_state", {})[command] = args
            elif command in ("gsDPTileSync", "gsDPLoadSync", "gsDPPipeSync", "gsDPNoOp"):
                require(not args, "Unexpected sync args")
            else:
                raise ValueError(f"Unsupported reached command {command} in {name}")
        raise ValueError(f"No executed end in {name}")
