"""Decode cartridge display lists without shipping course command streams.

The compact stream is expanded using the game's documented opcode layout.
All command operands, vertices, images and triangle indices come from the ROM.
The small GBI table describes generic SDK render states, not course assets.
"""

from __future__ import annotations
import json, struct
from pathlib import Path
from .source_rom import require

MODES = json.loads(Path(__file__).with_name("gbi_modes.json").read_text())


def unpack(data, offset, expected):
    require(
        0 <= offset < len(data) and 0 < expected < 0x1000000, "Invalid packed display-list extent"
    )
    result = bytearray()
    p = offset

    def byte():
        nonlocal p
        require(p < len(data), "Truncated packed display list")
        value = data[p]
        p += 1
        return value

    def short():
        return byte() | (byte() << 8)

    def emit(a, b):
        require(len(result) < expected + 16, "Packed display list exceeds declared size")
        result.extend(struct.pack(">II", a, b))

    fixed = {
        0x15: (0xFC121824, 0xFF33FFFF),
        0x16: (0xFC127E24, 0xFFFFF3F9),
        0x17: (0xFCFFFFFF, 0xFFFE793C),
        0x18: (0xB900031D, 0x00552078),
        0x19: (0xB900031D, 0x00553078),
        0x26: (0xBB000001, 0xFFFFFFFF),
        0x27: (0xBB000000, 0x00010001),
        0x2A: (0xB8000000, 0),
        0x2D: (0xBE000000, 0xE),
        0x2E: (0xFC127E24, 0xFFFFF3F9),
        0x2F: (0xB900031D, 0x005049D8),
        0x53: (0xFCFFFFFF, 0xFFFCF279),
        0x54: (0xB900031D, 0x00442D58),
        0x55: (0xB900031D, 0x00404DD8),
        0x56: (0xB7000000, 0x2000),
        0x57: (0xB6000000, 0x2000),
    }

    def tri(value):
        return (
            ((value & 31) * 2 << 16) | (((value >> 5) & 31) * 2 << 8) | (((value >> 10) & 31) * 2)
        )

    while True:
        op = byte()
        if op == 255:
            break
        if op in fixed:
            emit(*fixed[op])
        elif op <= 0x14:
            emit(0xBC000002, 0x80000040)
            emit(0x03860010, 0x09000008 + op * 24)
            emit(0x03880010, 0x09000000 + op * 24)
        elif 0x1A <= op <= 0x1F or op == 0x2C:
            index = 0 if op == 0x2C else op - 0x1A
            width, height = [(32, 32), (64, 32), (32, 64)][index % 3]
            fmt = 3 if index >= 3 else 0
            s = byte()
            t = byte()
            tmem = 256 if op == 0x2C else 0
            emit(0xE8000000, 0)
            emit(
                0xF5000000 | (fmt << 21) | (2 << 19) | ((width // 4) << 9) | tmem,
                ((t & 15) << 18) | ((t >> 4) << 14) | ((s & 15) << 8) | ((s >> 4) << 4),
            )
            emit(0xF2000000, ((width - 1) * 4 << 12) | ((height - 1) * 4))
        elif 0x20 <= op <= 0x25:
            index = op - 0x20
            width, height = [(32, 32), (64, 32), (32, 64)][index % 3]
            fmt = 3 if index >= 3 else 0
            address = 0x05000000 + (byte() << 11)
            byte()
            arg = byte()
            tile = arg >> 4
            tmem = arg & 15
            emit(0xFD000000 | (fmt << 21) | (2 << 19), address)
            emit(0xE8000000, 0)
            emit(0xF5000000 | (fmt << 21) | (2 << 19) | tmem, tile << 24)
            emit(0xE6000000, 0)
            emit(
                0xF3000000,
                (tile << 24)
                | (min(width * height - 1, 2047) << 12)
                | ((2048 + width // 4 - 1) // (width // 4)),
            )
        elif op == 0x28 or 0x33 <= op <= 0x52:
            start = short()
            count = byte() & 63 if op == 0x28 else op - 0x32
            first = byte() & 63 if op == 0x28 else 0
            require(1 <= count <= 32 and first + count <= 32, "Invalid packed vertex load")
            emit(
                0x04000000 | (first * 2 << 16) | ((count << 10) + count * 16 - 1),
                0x04000000 + start * 16,
            )
        elif op == 0x29:
            emit(0xBF000000, tri(short()))
        elif op == 0x58:
            emit(0xB1000000 | tri(short()), tri(short()))
        elif op == 0x2B:
            emit(0x06000000, 0x07000000 + short() * 8)
        elif op == 0x30:
            value = byte() | (byte() << 8) | (byte() << 16)
            a = value & 31
            b = (value >> 5) & 31
            c = (value >> 10) & 31
            d = (value >> 15) & 31
            emit(0xB5000000, (d * 2 << 24) | (a * 2 << 16) | (b * 2 << 8) | (c * 2))
        else:
            raise ValueError(f"Unknown packed display-list opcode {op:02X}")
    # The native table names the offset of the final END command, not its end.
    require(len(result) in (expected, expected + 8), "Expanded display-list length mismatch")
    return bytes(result)


def tile_name(value):
    return "G_TX_RENDERTILE" if value == 0 else "G_TX_LOADTILE" if value == 7 else str(value)


def fmt_name(value):
    names = {
        0: "G_IM_FMT_RGBA",
        1: "G_IM_FMT_YUV",
        2: "G_IM_FMT_CI",
        3: "G_IM_FMT_IA",
        4: "G_IM_FMT_I",
    }
    require(value in names, "Unsupported image format")
    return names[value]


def wrap_name(value):
    return {
        0: "G_TX_NOMIRROR | G_TX_WRAP",
        1: "G_TX_MIRROR | G_TX_WRAP",
        2: "G_TX_NOMIRROR | G_TX_CLAMP",
        3: "G_TX_MIRROR | G_TX_CLAMP",
    }[value]


class DisplayLists(dict):
    """Lazy macro view of actual big-endian F3DEX commands, including fallthrough."""

    def __init__(self, donor, course, layout):
        super().__init__()
        self.donor = donor
        self.course = course
        self.layout = layout
        self.addresses = dict(layout["gfx"])
        self.names = {v: k for k, v in self.addresses.items()}
        self.textures = {t["segment_address"]: t["symbol"] for t in layout["textures"]}

    def name(self, address):
        if address not in self.names:
            require(address >> 24 in (6, 7), "Unsupported display-list segment")
            name = (
                f"d_course_{self.course}_"
                + ("packed_dl_" if address >> 24 == 7 else "dl_")
                + f"{address&0xFFFFFF:X}"
            )
            self.names[address] = name
            self.addresses[name] = address
        return self.names[address]

    def __contains__(self, name):
        return name in self.addresses

    def __getitem__(self, name):
        if not dict.__contains__(self, name):
            dict.__setitem__(self, name, self.decode(self.addresses[name]))
        return dict.__getitem__(self, name)

    def declaration(self, name):
        """Decode one named source interval for camera-variant classification.

        The execution decoder still follows native fallthrough. Camera recipes
        were established from named declarations: Yoshi's BC0 interval ends at
        CC0 despite lacking END, so that next camera is not co-occurrence proof.
        Only address boundaries are stored; commands still come from the ROM.
        """
        address = self.addresses[name]
        following = [
            v for v in self.layout["gfx"].values() if address < v and v >> 24 == address >> 24
        ]
        return self.decode(address, min(following) if following else None)

    def decode(self, address, declaration_end=None):
        data = self.donor.segment(self.course, address >> 24)
        offset = address & 0xFFFFFF
        commands = []
        require(offset % 8 == 0, "Misaligned display-list pointer")
        limit = min(len(data) - 7, offset + 0x100000)
        if declaration_end is not None:
            limit = min(limit, declaration_end & 0xFFFFFF)
        for pos in range(offset, limit, 8):
            a, b = struct.unpack_from(">II", data, pos)
            op = a >> 24
            key = f"{a:08X} {b:08X}"
            if key in MODES:
                command, args = MODES[key]
                commands.append((command, list(args)))
                continue

            def add(name, args=()):
                commands.append((name, list(map(str, args))))

            if op == 0xB8:
                add("gsSPEndDisplayList")
                return commands
            elif op == 6:
                add("gsSPDisplayList", [self.name(b)])
                if a & 0x10000:
                    add("gsSPEndDisplayList")
                    return commands
            elif op == 4:
                add("gsSPVertex", [hex(b), (a >> 10) & 63, ((a >> 16) & 255) // 2])
            elif op in (0xBF, 0xB1):

                def indices(word):
                    return [((word >> shift) & 255) // 2 for shift in (16, 8, 0)] + [0]

                add(
                    "gsSP1Triangle" if op == 0xBF else "gsSP2Triangles",
                    indices(b) if op == 0xBF else indices(a) + indices(b),
                )
            elif op == 0xB5:
                d, c, e, f = [((b >> shift) & 255) // 2 for shift in (24, 16, 8, 0)]
                add("gsSP2Triangles", [c, e, f, 0, c, f, d, 0])
            elif op in (0xB6, 0xB7):
                flags = {
                    1: "G_ZBUFFER",
                    4: "G_SHADE",
                    0x200: "G_SHADING_SMOOTH",
                    0x1000: "G_CULL_FRONT",
                    0x2000: "G_CULL_BACK",
                    0x10000: "G_FOG",
                    0x20000: "G_LIGHTING",
                    0x40000: "G_TEXTURE_GEN",
                    0x80000: "G_TEXTURE_GEN_LINEAR",
                    0x100000: "G_LOD",
                }
                require(b & ~sum(flags) == 0, "Unsupported geometry bits")
                add(
                    "gsSPSetGeometryMode" if op == 0xB7 else "gsSPClearGeometryMode",
                    [" | ".join(v for k, v in flags.items() if b & k)],
                )
            elif op == 0xBB:
                add(
                    "gsSPTexture",
                    [
                        f"0x{b>>16:04X}",
                        f"0x{b&65535:04X}",
                        (a >> 11) & 7,
                        tile_name((a >> 8) & 7),
                        "G_ON" if a & 255 else "G_OFF",
                    ],
                )
            elif op == 0xFD:
                require(b in self.textures, f"Unknown texture address {b:08X}")
                add(
                    "gsDPSetTextureImage",
                    [
                        fmt_name((a >> 21) & 7),
                        f"G_IM_SIZ_{4<<((a>>19)&3)}b",
                        (a & 4095) + 1,
                        self.textures[b],
                    ],
                )
            elif op == 0xF5:
                add(
                    "gsDPSetTile",
                    [
                        fmt_name((a >> 21) & 7),
                        f"G_IM_SIZ_{4<<((a>>19)&3)}b",
                        (a >> 9) & 511,
                        f"0x{a&511:04X}",
                        tile_name((b >> 24) & 7),
                        (b >> 20) & 15,
                        wrap_name((b >> 18) & 3),
                        (b >> 14) & 15 or "G_TX_NOMASK",
                        (b >> 10) & 15 or "G_TX_NOLOD",
                        wrap_name((b >> 8) & 3),
                        (b >> 4) & 15 or "G_TX_NOMASK",
                        b & 15 or "G_TX_NOLOD",
                    ],
                )
            elif op in (0xF2, 0xF3):
                add(
                    "gsDPSetTileSize" if op == 0xF2 else "gsDPLoadBlock",
                    [
                        tile_name((b >> 24) & 7),
                        (a >> 12) & 4095,
                        a & 4095,
                        f"0x{(b>>12)&4095:04X}" if op == 0xF2 else (b >> 12) & 4095,
                        f"0x{b&4095:04X}" if op == 0xF2 else b & 4095,
                    ],
                )
            elif op in (0xE6, 0xE7, 0xE8, 0, 0xC0):
                add(
                    {
                        0xE6: "gsDPLoadSync",
                        0xE7: "gsDPPipeSync",
                        0xE8: "gsDPTileSync",
                        0: "gsDPNoOp",
                        0xC0: "gsDPNoOp",
                    }[op]
                )
            elif op == 0xF8:
                add("gsDPSetFogColor", [f"0x{(b>>shift)&255:02X}" for shift in (24, 16, 8, 0)])
            elif op == 0xBC and a & 255 == 2:
                add("gsSPNumLights", [(b - 0x80000000) // 32 - 1])
            elif op == 0xBC and a & 255 == 8:
                add("gsSPFogFactor", struct.unpack(">hh", struct.pack(">I", b)))
            elif op == 3 and ((a >> 16) & 255) in (0x86, 0x88):
                add("gsSPLight", [hex(b), 1 if ((a >> 16) & 255) == 0x86 else 2])
            elif op == 0xBE:
                add("gsSPCullDisplayList", [a & 65535, (b & 65535) // 2])
            else:
                raise ValueError(
                    f"Unsupported F3DEX command {a:08X} {b:08X} at {address>>24:02X}{pos:06X}"
                )
        if declaration_end is not None:
            return commands
        raise ValueError(f"Unterminated display list at {address:08X}")
