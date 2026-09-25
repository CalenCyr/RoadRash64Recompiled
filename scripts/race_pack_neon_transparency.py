"""Adapt only MK64 Rainbow Road's neon billboards to a transparent background.

The donor stores pure black as opaque RGBA5551 (0x0001), even though the
billboards use the texture-edge render mode. That black rectangle merges into
the donor's black backdrop, but can hide the imported road and other signs.
This explicit import adaptation clears its alpha bit. It is not a general
black color key: terrain, vehicles, other sprites, and nonblack pixels retain
their original values. No ROM data is included in this module.
"""

import struct


NEON_TEXTURES = frozenset(
    "gTextureRainbowRoadNeon" + name
    for name in (
        "Mushroom", "Mario", "Boo", "Peach", "Luigi", "DonkeyKong",
        "Yoshi", "Bowser", "Wario", "Toad",
    )
)


def transparent_neon_pixels(texture_name, pixels):
    """Return unchanged RGB with alpha cleared for the named neon's black."""
    if texture_name not in NEON_TEXTURES:
        return pixels
    if len(pixels) % 2:
        raise ValueError("Truncated RGBA16 neon texture")
    result = bytearray(pixels)
    for offset in range(0, len(result), 2):
        if result[offset] == 0 and result[offset + 1] == 1:
            result[offset + 1] = 0
    return bytes(result)


def adapt_neon_models(asset, source_textures):
    """Patch certified model IDs using their source texture names.

    The caller supplies IDs from the donor sprite extraction metadata; IDs are
    not assumed stable across imports. Only RGBA16 texture-edge batches qualify.
    Geometry, render flags, model identity, animation and all other bytes stay
    intact. Malformed records or an incomplete mapping fail before returning.
    """
    if len(asset) < 12 or asset[:8] != b"MKHZ0001":
        raise ValueError("Invalid hazard asset header")
    selected = {
        model: name for model, name in source_textures.items()
        if name in NEON_TEXTURES
    }
    result = bytearray(asset)
    offset = 12
    seen = set()
    report = []
    count = struct.unpack_from(">I", asset, 8)[0]
    try:
        for expected_id in range(count):
            record_size = struct.unpack_from(">I", asset, offset)[0]
            offset += 4
            end = offset + record_size
            if record_size < 36 or end > len(asset):
                raise ValueError("Invalid hazard model bounds")
            model_id, _, batches = struct.unpack_from(">IfI", asset, offset)
            if model_id != expected_id:
                raise ValueError("Unordered hazard model identity")
            if model_id in selected and batches != 2:
                raise ValueError("Unexpected neon strip count")
            offset += 36
            for batch in range(batches):
                flags, width, height, triangles = struct.unpack_from(">4I", asset, offset)
                offset += 16 + triangles * 48
                texture_end = offset + width * height * 2
                if texture_end > end:
                    raise ValueError("Truncated hazard batch")
                if model_id in selected:
                    # Texture-edge RGBA16, not IA16 or a translucent model.
                    if flags != 0x14A or width != 64 or height != 32:
                        raise ValueError("Unexpected neon material or strip dimensions")
                    before = asset[offset:texture_end]
                    after = transparent_neon_pixels(selected[model_id], before)
                    result[offset:texture_end] = after
                    report.append({
                        "model": model_id, "batch": batch,
                        "texture": selected[model_id], "offset": offset,
                        "bytes": len(before),
                        "changed_pixels": sum(a != b for a, b in zip(before, after)),
                    })
                    seen.add(model_id)
                offset = texture_end
            if offset != end:
                raise ValueError("Hazard model length mismatch")
    except struct.error as error:
        raise ValueError("Truncated hazard asset") from error
    if offset != len(asset) or seen != selected.keys():
        raise ValueError("Trailing bytes or missing neon model")
    return bytes(result), report
