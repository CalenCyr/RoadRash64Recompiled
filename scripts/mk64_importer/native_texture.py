"""Encode imported RGBA16 records for the native RR64 material producer.

The native 16-bit branch preserves source texels without palette quantization.
Its tagged extension contains only the approved sampler and culling flags;
the final texture registry adds the verified depth-state flags separately.
"""

from __future__ import annotations

import struct

FEATURE_TAG = 0x4D4B0000
FEATURE_MASK = 0xFF


def material_features(material: dict, texture: dict) -> int:
    """Validated source material -> private +38 sampler/culling/scale bits."""
    if not material["texture_enabled"] or material["texture"] != texture["symbol"]:
        raise ValueError("Material does not use the expected texture")
    tile = material["tile"]
    if len(tile) != 12 or tile[:2] != [
        "G_IM_FMT_IA" if texture["format"] == "ia16" else "G_IM_FMT_RGBA",
        "G_IM_SIZ_16b",
    ]:
        raise ValueError("Unsupported source render tile")
    features = 0x40 if texture["format"] == "ia16" else 0
    for mode_index, mask_index, shift_index, dimension, shift in (
        (9, 10, 11, texture["width"], 0),
        (6, 7, 8, texture["height"], 2),
    ):
        modes = {v.strip() for v in tile[mode_index].split("|")}
        allowed = {"G_TX_NOMIRROR", "G_TX_MIRROR", "G_TX_WRAP", "G_TX_CLAMP"}
        if (
            not modes <= allowed
            or len(modes & {"G_TX_NOMIRROR", "G_TX_MIRROR"}) != 1
            or len(modes & {"G_TX_WRAP", "G_TX_CLAMP"}) != 1
            or int(tile[mask_index], 0) != dimension.bit_length() - 1
            or tile[shift_index] != "G_TX_NOLOD"
        ):
            raise ValueError("Unsupported sampler mode/mask/shift")
        features |= (int("G_TX_MIRROR" in modes) | (int("G_TX_CLAMP" in modes) << 1)) << shift
    if "G_CULL_FRONT" in material["geometry_modes"]:
        raise ValueError("Front-face culling has no prototype feature mapping")
    if "G_CULL_BACK" not in material["geometry_modes"]:
        features |= 0x10
    scale = material["texture_scale"]
    if len(scale) != 5 or scale[2:] != ["0", "G_TX_RENDERTILE", "G_ON"]:
        raise ValueError("Unsupported texture scale level/tile/state")
    values = [int(v, 0) for v in scale[:2]]
    if values == [0xFFFF, 0xFFFF]:
        features |= 0x20
    elif values != [0x8000, 0x8000]:
        raise ValueError("Unsupported unequal/nonstandard texture scale")
    return features


def encode_rgba16(pixels: bytes, width: int, height: int, *, cutout=False, features=None) -> bytes:
    if (
        type(width) is not int
        or type(height) is not int
        or width < 2
        or height < 2
        or width > 128
        or height > 128
        or width & (width - 1)
        or height & (height - 1)
        or width * height > 2048
    ):
        raise ValueError("RGBA16 texture must fit native power-of-two dimensions and 4KiB TMEM")
    if len(pixels) != width * height * 2:
        raise ValueError("RGBA16 byte count does not match dimensions")
    if features is not None and (
        type(features) is not int or features < 0 or features & ~FEATURE_MASK
    ):
        raise ValueError("Unknown private texture feature bits")
    record = bytearray(0x40)
    struct.pack_into(">III", record, 0, 0x16, 0x40 + len(pixels), 0xFFFFFFFF)
    name = b"custom-course-rgba16"
    record[0x0C : 0x0C + len(name)] = name
    # One frame, zero TLUT entries, optional native cutout material bit.
    struct.pack_into(">4H", record, 0x20, 1, 0, 0x4000 if cutout else 0, len(pixels))
    struct.pack_into(
        ">5I",
        record,
        0x2C,
        width,
        height,
        16,
        FEATURE_TAG | features if features is not None else 0,
        len(pixels),
    )
    record.extend(pixels)
    return bytes(record)
