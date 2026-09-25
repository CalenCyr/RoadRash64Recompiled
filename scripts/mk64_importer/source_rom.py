"""Bounded readers for a user's supported Mario Kart 64 cartridge image.

Only layout/address recipes ship with the importer. Every geometry, texture,
animation and audio byte must be read from the locally supplied ROM.
"""

from __future__ import annotations
import hashlib
import struct
from pathlib import Path

US_SHA1 = "579c48e211ae952530ffc8738709f078d5dd215e"
COURSES = [
    (0, "mario_raceway", "Mario Raceway"),
    (1, "choco_mountain", "Choco Mountain"),
    (2, "bowsers_castle", "Bowser's Castle"),
    (3, "banshee_boardwalk", "Banshee Boardwalk"),
    (4, "yoshi_valley", "Yoshi Valley"),
    (5, "frappe_snowland", "Frappe Snowland"),
    (6, "koopa_troopa_beach", "Koopa Troopa Beach"),
    (7, "royal_raceway", "Royal Raceway"),
    (8, "luigi_raceway", "Luigi Raceway"),
    (9, "moo_moo_farm", "Moo Moo Farm"),
    (10, "toads_turnpike", "Toad's Turnpike"),
    (11, "kalimari_desert", "Kalimari Desert"),
    (12, "sherbet_land", "Sherbet Land"),
    (13, "rainbow_road", "Rainbow Road"),
    (14, "wario_stadium", "Wario Stadium"),
    (18, "dks_jungle_parkway", "DK's Jungle Parkway"),
]


def require(ok, message):
    if not ok:
        raise ValueError(message)


def normalize_rom(raw):
    require(len(raw) == 0xC00000, "Supported Mario Kart 64 USA ROM must be 12 MiB")
    if raw[:4] == bytes.fromhex("80371240"):
        data = bytes(raw)
    elif raw[:4] == bytes.fromhex("37804012"):
        out = bytearray(raw)
        out[0::2], out[1::2] = raw[1::2], raw[0::2]
        data = bytes(out)
    elif raw[:4] == bytes.fromhex("40123780"):
        out = bytearray(raw)
        for i in range(4):
            out[i::4] = raw[3 - i :: 4]
        data = bytes(out)
    else:
        raise ValueError("Unrecognized Nintendo64 ROM byte order")
    require(
        hashlib.sha1(data).hexdigest() == US_SHA1,
        "This importer requires the supported Mario Kart 64 USA ROM",
    )
    return data


def decode_mio0(data, offset=0, expected_size=None):
    require(
        0 <= offset <= len(data) - 16 and data[offset : offset + 4] == b"MIO0", "Missing MIO0 block"
    )
    size, compressed, raw = struct.unpack_from(">III", data, offset + 4)
    require(
        0 < size <= 0x1000000 and (expected_size is None or size == expected_size),
        "Invalid MIO0 output size",
    )
    require(16 <= compressed <= raw and offset + raw < len(data), "Invalid MIO0 streams")
    cp = offset + compressed
    rp = offset + raw
    bits = 0
    out = bytearray()
    while len(out) < size:
        control = offset + 16 + bits // 8
        require(control < offset + compressed, "MIO0 control stream overflow")
        if data[control] & (0x80 >> (bits % 8)):
            require(rp < len(data), "MIO0 literal overflow")
            out.append(data[rp])
            rp += 1
        else:
            require(cp + 2 <= offset + raw, "MIO0 backreference overflow")
            value = int.from_bytes(data[cp : cp + 2], "big")
            cp += 2
            length = (value >> 12) + 3
            distance = (value & 4095) + 1
            require(
                distance <= len(out) and len(out) + length <= size, "Invalid MIO0 backreference"
            )
            for _ in range(length):
                out.append(out[-distance])
        bits += 1
    return bytes(out), rp - offset


class Donor:
    """A verified ROM with lazy decompression; segment bytes never come from recipes."""

    def __init__(self, rom, recipes=None):
        self.rom = normalize_rom(Path(rom).read_bytes() if isinstance(rom, (str, Path)) else rom)
        self.recipes = recipes or {}
        self._segments = {}
        self._mio = {}

    def mio0(self, offset, size=None):
        if offset not in self._mio:
            self._mio[offset] = decode_mio0(self.rom, offset)[0]
        out = self._mio[offset]
        require(size is None or len(out) == size, "MIO0 recipe length mismatch")
        return out

    def course_table(self, course):
        index = next((i for i, s, _ in COURSES if s == course), course)
        require(isinstance(index, int) and 0 <= index < 20, "Invalid course identifier")
        return struct.unpack_from(">12I", self.rom, 0x122390 + index * 0x30)

    def segment(self, course, segment):
        key = (course, segment)
        if key in self._segments:
            return self._segments[key]
        table = self.course_table(course)
        if segment == 6:
            result = self.mio0(table[0])
        elif segment == 9:
            result = self.rom[table[4] : table[5]]
        elif segment == 15:
            result = self.rom[table[2] : table[3]]
        elif segment == 4:
            packed = self.mio0(table[2] + (table[6] & 0xFFFFFF))
            count = table[7]
            require(len(packed) >= count * 14, "Truncated course vertex stream")
            out = bytearray()
            for i in range(count):
                x, y, z, s, t, r, g, b, unused = struct.unpack_from(">5h4B", packed, i * 14)
                out += struct.pack(
                    ">3hH2h4B", x, y, z, (r & 3) | ((g & 3) << 2), s, t, r & 252, g & 252, b, 255
                )
            result = bytes(out)
        elif segment == 7:
            from .source_displaylists import unpack

            result = unpack(self.segment(course, 15), table[8] & 0xFFFFFF, table[9])
        else:
            raise ValueError(f"Unsupported donor segment {segment}")
        self._segments[key] = result
        return result

    def read(self, course, address, size):
        segment = address >> 24
        data = self.segment(course, segment)
        offset = address & 0xFFFFFF
        require(size >= 0 and offset + size <= len(data), "Donor segmented read out of range")
        return data[offset : offset + size]

    def binary(self, symbol, course=None, size=None):
        info = self.recipes.get("symbols", {}).get(symbol)
        if info is None and course is not None:
            info = self.recipes.get("courses", {}).get(course, {}).get("symbols", {}).get(symbol)
        require(info is not None, "Unknown donor symbol " + symbol)
        length = info.get("size", size)
        require(length is not None, "Symbol read requires a size")
        if size is not None:
            require(size <= length, "Symbol read exceeds recipe")
            length = size
        if "rom_offset" in info:
            source = self.mio0(info["mio0"]) if "mio0" in info else self.rom
            offset = info["rom_offset"]
            require(0 <= offset <= len(source) - length, "Donor symbol out of bounds")
            return source[offset : offset + length]
        return self.read(course or info.get("course"), info["address"], length)
