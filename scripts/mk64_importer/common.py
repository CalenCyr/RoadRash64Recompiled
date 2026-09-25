"""Small shared helpers; no global filesystem state or installation writes."""

from __future__ import annotations

import hashlib
import json
import os
from pathlib import Path
import struct

MK64_SHA256 = "d6b8538dd63f0132ecb2856e7d32816ed3c30e3e479aecd23cf83fb6ba17a5da"
RR64_SHA256 = "74e49e863484b5d17dcbe3891b99f2cafe3cf7511aa1dc5427022f699301db73"
COURSES = (
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
)


def require(condition, message):
    if not condition:
        raise ValueError(message)


def digest(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def orientation_key(values):
    values = tuple(values)
    return min(values[i:] + values[:i] for i in range(len(values)))


def f32(value):
    return struct.unpack(">f", struct.pack(">f", value))[0]


def normalize_rom(data: bytes, expected: str, name: str) -> bytes:
    """Accept the usual z64/v64/n64 byte orders, then verify the whole image."""
    if data[:4] == b"\x37\x80\x40\x12":
        require(len(data) % 2 == 0, "Truncated " + name + " ROM")
        swapped = bytearray(len(data))
        swapped[0::2], swapped[1::2] = data[1::2], data[0::2]
        data = bytes(swapped)
    elif data[:4] == b"\x40\x12\x37\x80":
        require(len(data) % 4 == 0, "Truncated " + name + " ROM")
        swapped = bytearray(len(data))
        for offset in range(4):
            swapped[offset::4] = data[3 - offset :: 4]
        data = bytes(swapped)
    require(data[:4] == b"\x80\x37\x12\x40", "Not an N64 " + name + " ROM")
    require(
        digest(data) == expected, "Unsupported " + name + " ROM; use the unmodified USA version"
    )
    return data


def read_rom(path: Path, expected: str, size: int, name: str) -> bytes:
    """Reject accidental large files before allocating or decoding their data."""
    with Path(path).open("rb") as source:
        require(os.fstat(source.fileno()).st_size == size, "Unsupported " + name + " ROM size")
        data = source.read(size + 1)
    require(len(data) == size, "The " + name + " ROM changed while being read")
    return normalize_rom(data, expected, name)


def json_bytes(value) -> bytes:
    return (
        json.dumps(value, sort_keys=True, separators=(",", ":"), allow_nan=False) + "\n"
    ).encode("utf-8")


def atomic_json(path: Path, value):
    """Readers always see one complete progress/result record."""
    path = Path(path)
    path.parent.mkdir(parents=True, exist_ok=True)
    temporary = path.with_name(path.name + ".tmp")
    temporary.write_bytes(json_bytes(value))
    os.replace(temporary, path)


class Cancelled(Exception):
    pass


class Progress:
    def __init__(self, path=None, cancel_file=None):
        self.path = Path(path) if path else None
        self.cancel_file = Path(cancel_file) if cancel_file else None

    def check(self):
        if self.cancel_file and self.cancel_file.exists():
            raise Cancelled("Import cancelled")

    def update(self, stage, message, percent):
        self.check()
        record = dict(stage=stage, message=message, percent=max(0, min(100, float(percent))))
        if self.path:
            atomic_json(self.path, record)
