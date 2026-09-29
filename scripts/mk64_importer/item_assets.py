"""Extract the original item art from the supported, user-supplied MK64 ROM.

Only addresses and decoding rules are distributed. The bank contains original
RGBA5551 pixels (CI8 expanded through its original palette), vertices and the
triangle topology read from the original F3DEX display lists. Native ItemIds
index the 16 HUD icons, including the double-mushroom intermediate state.
"""

from __future__ import annotations

import hashlib
import struct

from .common import require

MAGIC = b"R64ITEM1"
TEXTURE_COUNT = 43
MESH_COUNT = 6
# The native Star/Boo ordering differs from extraction table order.
ICON_ORDER = (0, 1, 2, 9, 10, 11, 12, 7, 14, 15, 13, 8, 3, 4, 5, 6)
GREEN_FRAMES = (0x68EB50, 0x68EDA0, 0x68EFF0, 0x68F248,
                0x68F4A8, 0x68F700, 0x68F96C, 0x68FBCC)
BLUE_FRAMES = (0x68FE20, 0x69004C, 0x690284, 0x6904C4,
               0x690708, 0x690960, 0x690BBC, 0x690DF8)
# List offset, vertex offset/count, texture ID. The shell's texture is chosen
# from its eight-frame animation at runtime. Its two meshes preserve mirroring.
MESHES = ((0x5338, 0x5238, 4, 16), (0x5368, 0x5278, 4, 16),
          (0x4B48, 0x3298, 5, 40), (0x4BD8, 0x32E8, 6, 41),
          (0x2F80, 0x2F40, 4, 42), (0x3090, 0x1D68, 24, 0xFFFFFFFF))


def rgba_ci8(indices, palette):
    require(len(palette) == 512, "Item palette size")
    return b"".join(palette[index * 2:index * 2 + 2] for index in indices)


def original_triangles(common, address, vertex_offset, vertex_count):
    """Decode only the audited, bounded common-segment item display lists."""
    triangles, loaded = [], False
    stack, pc, visited = [], address, 0
    while visited < 256:
        require(0 <= pc <= len(common) - 8, "Item display-list bounds")
        first, second = struct.unpack_from(">II", common, pc)
        pc += 8
        visited += 1
        op = first >> 24
        if op == 0xB8:
            if not stack:
                require(loaded and triangles, "Empty item mesh")
                return triangles
            pc = stack.pop()
        elif op == 0x06:
            require(second >> 24 == 0x0D and len(stack) < 4, "Item list segment/depth")
            stack.append(pc)
            pc = second & 0xFFFFFF
        elif op == 0x04:
            require(not loaded and second == 0x0D000000 + vertex_offset and
                    ((first >> 10) & 63) == vertex_count, "Item vertex recipe changed")
            loaded = True
        elif op in (0xBF, 0xB1):
            require(loaded, "Item triangle before vertices")
            words = (second,) if op == 0xBF else (first, second)
            for word in words:
                raw = ((word >> 16) & 255, (word >> 8) & 255, word & 255)
                require(all(index % 2 == 0 and index // 2 < vertex_count for index in raw),
                        "Item triangle vertex bounds")
                triangles.append(tuple(index // 2 for index in raw))
        else:
            require(op in (0xB6, 0xB7, 0xB9, 0xBA, 0xBB, 0xE6, 0xE7, 0xE8,
                           0xF2, 0xF3, 0xF5, 0xFC, 0xFD), "Unexpected item list command")
    raise ValueError("Item display-list command limit")


def extract(donor):
    common = donor.mio0(0x132B50)
    require(len(common) == 0x2D158, "Original common item segment size")
    textures = []
    for source in ICON_ORDER:
        indices = common[0x1FED8 + source * 1280:0x1FED8 + (source + 1) * 1280]
        palette = common[0x1DED8 + source * 512:0x1DED8 + (source + 1) * 512]
        textures.append((40, 32, rgba_ci8(indices, palette)))
    green_palette = common[0x4E38:0x5038]
    # Native init_red_shell_texture swaps the red/green RGBA5551 fields.
    red_palette = b"".join(struct.pack(">H", ((pixel & 0xF800) >> 5) |
                                      ((pixel & 0x07C0) << 5) | (pixel & 0x003F))
                           for (pixel,) in struct.iter_unpack(">H", green_palette))
    blue_palette = common[0x5038:0x5238]
    for frames, palette in ((GREEN_FRAMES, green_palette), (GREEN_FRAMES, red_palette),
                            (BLUE_FRAMES, blue_palette)):
        for offset in frames:
            indices = donor.mio0(offset)
            require(len(indices) == 1024, "Original shell frame size")
            textures.append((32, 32, rgba_ci8(indices, palette)))
    textures.extend(((32, 32, bytes(common[0x3348:0x3B48])),
                     (64, 32, bytes(common[0x3B48:0x4B48])),
                     (32, 64, bytes(common[0x1EE8:0x2EE8]))))
    require(len(textures) == TEXTURE_COUNT, "Complete item texture set")
    bank = bytearray(MAGIC + struct.pack("<II", TEXTURE_COUNT, MESH_COUNT))
    proof = {"textures": [], "meshes": []}
    for index, (width, height, pixels) in enumerate(textures):
        require(len(pixels) == width * height * 2, "Original item image bounds")
        bank += struct.pack("<IIII", index, width, height, len(pixels)) + pixels
        proof["textures"].append({"id": index, "width": width, "height": height,
                                  "sha256": hashlib.sha256(pixels).hexdigest()})
    for index, (display_list, vertices, count, texture) in enumerate(MESHES):
        data = bytes(common[vertices:vertices + count * 16])
        require(len(data) == count * 16, "Item mesh vertex bounds")
        triangles = original_triangles(common, display_list, vertices, count)
        bank += struct.pack("<IIII", index, count, len(triangles), texture) + data
        bank += b"".join(bytes((*triangle, 0)) for triangle in triangles)
        while len(bank) % 8:
            bank.append(0)
        proof["meshes"].append({"id": index, "display_list": display_list,
                                "vertices": count, "triangles": len(triangles),
                                "sha256": hashlib.sha256(data).hexdigest()})
    require(len(bank) < 128 * 1024, "Bounded original item art bank")
    proof["bytes"] = len(bank)
    proof["sha256"] = hashlib.sha256(bank).hexdigest()
    return bytes(bank), proof
