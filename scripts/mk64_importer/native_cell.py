"""Experimental native Road Rash 64 terrain-cell encoder (no bundled assets).

Sources: active generated 146C8/147D8/14A10 ground query and 7D1A0/7D4C4
terrain display-list builder, plus rr64_world_terrain_assets.cpp. This emits
plain big-endian 3F/3D/3C records, not display lists or an alternate renderer.

Vertex X/Y are absolute terrain coordinates (4 * native rider position).
Vertex Z is the RAW signed-16 terrain height; native rendering/contact uses
Z * 0.5. Thus a desired ground-query height H must be supplied as Z=2*H.
Caller clips triangles to each 1000-unit cell before encoding. Geometry on a
cell edge must appear in both adjacent cells if their interiors use it.
UVs are signed native N64 s10.5 texture coordinates; colors are RGBA bytes.
Surface IDs are native table indices 0..14; names are deliberately not guessed.
Surface=None omits contact (e.g. visual walls, NOT a solid wall collider).

validate_cell(..., roundtrip=True) checks encoded records and byte-roundtrips
vertex, triangle and collision-list fields in memory. It does not export ROM data.
"""

from __future__ import annotations

import math
import struct
from collections import defaultdict
from dataclasses import dataclass
from typing import Iterable

WIDTH = 70
CELL_SIZE = 1000
HALF_WORLD = WIDTH * CELL_SIZE // 2
TEXTURES = 1280


@dataclass(frozen=True)
class Vertex:
    x: int
    y: int
    z: int
    s: int = 0
    t: int = 0
    rgba: tuple[int, int, int, int] = (255, 255, 255, 255)


@dataclass(frozen=True)
class Triangle:
    vertices: tuple[Vertex, Vertex, Vertex]
    texture: int = 0xFFFF
    surface: int | None = 5
    render: bool = True
    translucent: bool = False


def _require(ok: bool, message: str) -> None:
    if not ok:
        raise ValueError(message)


def _pad(data: bytearray, alignment: int) -> None:
    data.extend(bytes((-len(data)) % alignment))


def cell_origin(cell_index: int) -> tuple[int, int]:
    _require(0 <= cell_index < WIDTH * WIDTH, "cell index outside fixed native grid")
    x, y = divmod(cell_index, WIDTH)
    return (x * CELL_SIZE - HALF_WORLD + 500, y * CELL_SIZE - HALF_WORLD + 500)


def world_cell(x: float, y: float) -> int:
    _require(math.isfinite(x) and math.isfinite(y), "nonfinite terrain coordinate")
    _require(
        -HALF_WORLD <= x < HALF_WORLD and -HALF_WORLD <= y < HALF_WORLD,
        "coordinate outside fixed native grid",
    )
    return int((x + HALF_WORLD) / CELL_SIZE) * WIDTH + int((y + HALF_WORLD) / CELL_SIZE)


def morton_key(x: int, y: int) -> int:
    _require(0 <= x < 8 and 0 <= y < 8, "collision subcell outside 8x8 grid")
    return sum(((x >> b) & 1) << (2 * b) | ((y >> b) & 1) << (2 * b + 1) for b in range(3))


def pack_triangle(indices: tuple[int, int, int]) -> int:
    _require(len(indices) == 3 and all(0 <= i < 32 for i in indices), "invalid packet index")
    return (indices[0] << 10) | (indices[1] << 5) | indices[2]


def unpack_triangle(word: int) -> tuple[int, int, int]:
    return ((word >> 10) & 31, (word >> 5) & 31, word & 31)


def pack_vertex(vertex: Vertex, origin: tuple[int, int]) -> bytes:
    values = (vertex.x - origin[0], vertex.y - origin[1], vertex.z, vertex.s, vertex.t)
    _require(
        all(isinstance(v, int) and -32768 <= v <= 32767 for v in values),
        "vertex/UV must fit signed native 16-bit fields",
    )
    _require(
        len(vertex.rgba) == 4 and all(isinstance(c, int) and 0 <= c <= 255 for c in vertex.rgba),
        "invalid RGBA byte",
    )
    return struct.pack(">hhhHhh4B", *values[:3], 0, *values[3:], *vertex.rgba)


def _bucket_coverage(triangle: Triangle, origin: tuple[int, int]) -> list[int]:
    # Conservative XY bbox duplication; native point-in-triangle does the exact
    # test. Include both sides of an exact 125-unit boundary. Never omit a
    # triangle just because its centroid belongs to a different subcell.
    limits = []
    for axis in ("x", "y"):
        lower = origin[0 if axis == "x" else 1] - 500
        values = [getattr(v, axis) - lower for v in triangle.vertices]
        lo = max(0, min(7, math.floor((min(values) - 1e-7) / 125)))
        hi = max(0, min(7, math.floor(max(values) / 125)))
        limits.append(range(lo, hi + 1))
    return [morton_key(x, y) for x in limits[0] for y in limits[1]]


def encode_cell(cell_index: int, triangles: Iterable[Triangle]) -> bytes:
    triangles = list(triangles)
    try:
        return _encode_cell(cell_index, triangles)
    except ValueError as error:
        if str(error) != "collision vertex offset exceeds native u16":
            raise
    # Contact references have a 16-bit partition-relative vertex address;
    # render submesh/packet references use full pointers. Keep an invisible
    # contact pool at the front, and preserve render order independently.
    collision = [
        Triangle(t.vertices, 0xFFFF, t.surface, False) for t in triangles if t.surface is not None
    ]
    render = [
        Triangle(t.vertices, t.texture, None, True, t.translucent) for t in triangles if t.render
    ]
    return _encode_cell(cell_index, collision + render)


def _encode_cell(cell_index: int, triangles: Iterable[Triangle]) -> bytes:
    """Encode one clipped cell. Reject capacity overflow instead of truncating.

    Empty input is invalid: represent an empty map cell with a zero map ref.
    Opaque packets are grouped by texture first occurrence. Translucent packets
    preserve input order in contiguous texture runs after opaque geometry.
    """
    origin = cell_origin(cell_index)
    triangles = list(triangles)
    _require(bool(triangles), "empty cell must use an empty map reference")
    opaque: dict[int, list[Triangle]] = {}
    translucent: list[tuple[int, list[Triangle]]] = []
    for tri in triangles:
        _require(len(tri.vertices) == 3, "triangle requires three vertices")
        _require(tri.texture == 0xFFFF or 0 <= tri.texture < TEXTURES, "invalid texture index")
        _require(tri.surface is None or 0 <= tri.surface < 15, "unknown native surface ID")
        _require(
            tri.render or tri.surface is not None, "triangle is neither visible nor collidable"
        )
        for v in tri.vertices:
            pack_vertex(v, origin)
            _require(
                abs(v.x - origin[0]) <= 500 and abs(v.y - origin[1]) <= 500,
                "triangle crosses cell bounds: clip before encode_cell",
            )
        if tri.translucent and tri.render:
            if not translucent or translucent[-1][0] != tri.texture:
                translucent.append((tri.texture, []))
            translucent[-1][1].append(tri)
        else:
            opaque.setdefault(tri.texture, []).append(tri)
    materials = list(opaque.items()) + translucent

    blob = bytearray(0xF0 + len(materials) * 12)
    _pad(blob, 8)
    # (packet's partition-relative vertex offset, surface) -> packed triangles.
    groups: list[dict[tuple[int, int], list[int]]] = [defaultdict(list) for _ in range(64)]
    for submesh_id, (texture, items) in enumerate(materials):
        start = len(blob)
        sub = bytearray(0x18)
        packets = 0
        pending: list[Triangle] = []
        vertices: list[Vertex] = []

        def flush() -> None:
            nonlocal packets, pending, vertices
            if not pending:
                return
            vertex_offset = start + len(sub) + 8
            _require(
                vertex_offset <= 0xFFFF or all(t.surface is None for t in pending),
                "collision vertex offset exceeds native u16",
            )
            indices = {v: i for i, v in enumerate(vertices)}
            words = [pack_triangle(tuple(indices[v] for v in t.vertices)) for t in pending]
            packet = bytearray(8)
            for v in vertices:
                packet.extend(pack_vertex(v, origin))
            packet.extend(struct.pack(">" + "H" * len(words), *words))
            _pad(packet, 8)
            _require(len(packet) <= 0xFFFF, "packet too large")
            struct.pack_into(
                ">4H", packet, 0, len(words), len(vertices), len(packet), len(vertices) * 16
            )
            sub.extend(packet)
            for tri, word in zip(pending, words):
                if tri.surface is not None:
                    for key in _bucket_coverage(tri, origin):
                        groups[key][(vertex_offset, tri.surface)].append(word)
            packets += 1
            pending, vertices = [], []

        render_packets = 0
        for render in (True, False):
            for tri in items:
                if tri.render != render:
                    continue
                new = [v for v in dict.fromkeys(tri.vertices) if v not in vertices]
                if len(vertices) + len(new) > 32 or len(pending) == 255:
                    flush()
                    new = list(dict.fromkeys(tri.vertices))
                vertices.extend(new)
                pending.append(tri)
            flush()
            if render:
                render_packets = packets
        _require(packets <= 0xFFFF, "too many packets")
        struct.pack_into(
            ">IIIHHHHI", sub, 0, 0x3D, len(sub), submesh_id, packets, render_packets, 0, texture, 0
        )
        entry = 0xF0 + submesh_id * 12
        struct.pack_into(">III", blob, entry, start - entry, len(sub), 0)
        blob.extend(sub)
        if texture != 0xFFFF and render_packets:
            off = 0x50 + (texture // 32) * 4
            struct.pack_into(
                ">I", blob, off, struct.unpack_from(">I", blob, off)[0] | (1 << (texture % 32))
            )

    collision_offset = len(blob)
    collision = bytearray(0x10 + 64 * 2)
    for key, lists in enumerate(groups):
        _require(len(collision) <= 0xFFFF, "collision bucket offset exceeds native u16")
        _require(len(lists) <= 255, "collision list count exceeds native u8")
        struct.pack_into(">H", collision, 0x10 + key * 2, len(collision))
        collision.extend(struct.pack(">HH", key, len(lists)))
        for (offset, surface), words in lists.items():
            _require(len(words) <= 255, "collision triangle count exceeds native u8")
            collision.extend(struct.pack(">HBB", offset, surface, len(words)))
            collision.extend(struct.pack(">" + "H" * len(words), *words))
            _pad(collision, 4)
    struct.pack_into(">IIIHH", collision, 0, 0x3C, len(collision), 0, 0, 64)
    blob.extend(collision)
    _pad(blob, 8)
    _require(len(blob) <= 0xFFFF * 8, "partition exceeds native stream size field")
    struct.pack_into(
        ">IIIHHI", blob, 0, 0x3F, len(blob), cell_index, len(materials), 0, collision_offset
    )
    struct.pack_into(">2f", blob, 0x1C, *origin)
    struct.pack_into(">4f", blob, 0x24, 0, 0, 0, 1)
    xs = [v.x - origin[0] for tri in triangles for v in tri.vertices]
    ys = [v.y - origin[1] for tri in triangles for v in tri.vertices]
    # Native 19E84 expects maxX/minY/minX/maxY, not ordinary min/max order.
    struct.pack_into(">4f", blob, 0x40, max(xs), min(ys), min(xs), max(ys))
    validate_cell(bytes(blob))
    return bytes(blob)


def validate_cell(blob: bytes, *, roundtrip: bool = False) -> dict[str, int]:
    """Validate native references and optionally repack semantic stock fields.

    The roundtrip compares vertex, triangle, and collision-list payload bytes;
    unknown header words and padding are not claimed as understood.
    """

    def u16(p: int) -> int:
        _require(0 <= p <= len(blob) - 2, "short cell halfword")
        return struct.unpack_from(">H", blob, p)[0]

    def u32(p: int) -> int:
        _require(0 <= p <= len(blob) - 4, "short cell word")
        return struct.unpack_from(">I", blob, p)[0]

    _require(u32(0) == 0x3F and u32(4) == len(blob), "invalid cell header")
    packet_vertices: dict[int, int] = {}
    counts = dict(
        submeshes=u16(12),
        packets=0,
        vertices=0,
        render_triangles=0,
        collision_groups=0,
        collision_lists=0,
        collision_triangles=0,
    )
    for s in range(u16(12)):
        ref = 0xF0 + s * 12
        start, size = ref + u32(ref), u32(ref + 4)
        end = start + size
        _require(
            end <= len(blob) and u32(start) == 0x3D and u32(start + 4) == size,
            "invalid submesh ref",
        )
        _require(u16(start + 14) <= u16(start + 12), "render packet count exceeds all packets")
        p = start + 0x18
        for packet in range(u16(start + 12)):
            nt, nv, size, vb = struct.unpack_from(">4H", blob, p)
            _require(
                nv <= 32 and vb >= nv * 16 and size >= 8 + vb + nt * 2 and p + size <= end,
                "packet outside submesh",
            )
            packet_vertices[p + 8] = nv
            for v in range(nv):
                at = p + 8 + v * 16
                if roundtrip:
                    _require(
                        struct.pack(">hhhHhh4B", *struct.unpack_from(">hhhHhh4B", blob, at))
                        == blob[at : at + 16],
                        "vertex roundtrip",
                    )
            for t in range(nt):
                word = u16(p + 8 + vb + t * 2)
                _require(
                    all(i < nv for i in unpack_triangle(word)),
                    "render triangle index outside packet",
                )
                if roundtrip:
                    _require(
                        pack_triangle(unpack_triangle(word)) == word,
                        "triangle has undocumented flag bits",
                    )
            counts["packets"] += 1
            counts["vertices"] += nv
            if packet < u16(start + 14):
                counts["render_triangles"] += nt
            p += size
    c = u32(16)
    _require(u32(c) == 0x3C and c + u32(c + 4) <= len(blob), "invalid collision header")
    collision_end = c + u32(c + 4)
    previous = -1
    for g in range(u16(c + 14)):
        p = c + u16(c + 16 + g * 2)
        key, lists = u16(p), u16(p + 2)
        _require(previous < key < 64 and lists <= 255, "invalid collision key/list count")
        previous = key
        counts["collision_groups"] += 1
        p += 4
        for _ in range(lists):
            offset, surface, nt = struct.unpack_from(">HBB", blob, p)
            _require(
                offset in packet_vertices and surface < 15,
                "invalid collision vertex reference/surface",
            )
            words = [u16(p + 4 + i * 2) for i in range(nt)]
            _require(
                all(all(i < packet_vertices[offset] for i in unpack_triangle(w)) for w in words),
                "collision index outside packet",
            )
            if roundtrip:
                payload = struct.pack(">HBB", offset, surface, nt) + struct.pack(
                    ">" + "H" * nt, *[pack_triangle(unpack_triangle(w)) for w in words]
                )
                _require(payload == blob[p : p + 4 + nt * 2], "collision roundtrip")
            p += 4 + 2 * (nt + (nt & 1))
            _require(p <= collision_end, "collision list outside record")
            counts["collision_lists"] += 1
            counts["collision_triangles"] += nt
    return counts
