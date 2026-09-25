"""Native closed-route serialization using explicit binary32 arithmetic.

These thresholds are consumed by the game's original progress functions. True
Bezier arc length would look more accurate but would disagree with that code.
No compiler, historical executable or extracted game data is needed here.
"""

from __future__ import annotations

import math
import struct

from .common import f32, require


def add(a, b):
    return f32(f32(a) + f32(b))


def sub(a, b):
    return f32(f32(a) - f32(b))


def mul(a, b):
    return f32(f32(a) * f32(b))


def distance(a, b):
    x, y = (sub(a[k], b[k]) for k in range(2))
    return f32(math.sqrt(add(mul(x, x), mul(y, y))))


def native_length(curve):
    start, control, end = curve
    m0 = [mul(add(start[k], control[k]), 0.5) for k in range(2)]
    m1 = [mul(add(end[k], control[k]), 0.5) for k in range(2)]
    return add(add(distance(start, m0), distance(m0, m1)), distance(m1, end))


def pose(curve, parameter):
    t = f32(parameter)
    u = sub(1, t)
    direction = [
        add(mul(u, sub(curve[1][k], curve[0][k])), mul(t, sub(curve[2][k], curve[1][k])))
        for k in range(2)
    ]
    require(
        add(mul(direction[0], direction[0]), mul(direction[1], direction[1])) >= f32(1e-8),
        "Zero route tangent at spawn or finish",
    )
    heading = f32(math.atan2(direction[1], direction[0]))
    if heading < 0:
        heading = add(heading, f32(2 * math.pi))
    point = [
        add(
            add(mul(mul(u, u), curve[0][k]), mul(mul(mul(2, u), t), curve[1][k])),
            mul(mul(t, t), curve[2][k]),
        )
        for k in range(2)
    ]
    return [*point, heading]


def encode(curves, start_parameter, finish_parameter, laps=3, left_width=1.5, right_width=1.5):
    """Return exact big-endian route records and native threshold metadata."""
    curves = [[[f32(v) for v in point] for point in curve] for curve in curves]
    start_parameter, finish_parameter = f32(start_parameter), f32(finish_parameter)
    require(3 <= len(curves) <= 1024, "Invalid number of route curves")
    require(0 <= start_parameter < finish_parameter < 1, "Invalid route finish parameters")
    require(laps in (1, 3, 7), "Unsupported lap count")
    require(
        all(math.isfinite(v) and 0 < v <= 38.25 for v in (left_width, right_width)),
        "Invalid route lane width",
    )
    widths = [math.floor(f32(f32(v) / f32(0.15)) + 0.5) for v in (left_width, right_width)]
    require(all(1 <= v <= 255 for v in widths), "Route lane bounds quantize out of range")
    records = []
    period = f32(0)
    for i, curve in enumerate(curves):
        require(
            len(curve) == 3
            and all(len(p) == 2 and all(math.isfinite(v) for v in p) for p in curve),
            "Invalid route curve",
        )
        require(curve[2] == curves[(i + 1) % len(curves)][0], "Route endpoints do not join exactly")
        length = native_length(curve)
        require(
            math.isfinite(length)
            and length >= f32(0.01)
            and distance(curve[0], curve[1]) >= f32(0.001)
            and distance(curve[1], curve[2]) >= f32(0.001),
            "Degenerate route curve",
        )
        period = add(period, length)
        records.extend(((1, curve[0]), (4, curve[1])))
    first = curves[0]
    delta = [sub(first[2][k], first[1][k]) for k in range(2)]
    records.extend(
        (
            (1, first[0]),
            (4, first[1]),
            (1, first[2]),
            (4, [add(first[2][k], delta[k]) for k in range(2)]),
            (3, [add(first[2][k], mul(2, delta[k])) for k in range(2)]),
        )
    )
    blob = b"".join(
        struct.pack(">BBHBBBBff", kind, 0, 0, *widths, 0, 0, *point) for kind, point in records
    )
    lap_threshold = add(period, mul(finish_parameter, native_length(first)))
    finish_threshold = add(lap_threshold, mul(laps - 1, period))
    require(math.isfinite(finish_threshold), "Route length overflow")
    metadata = dict(
        record_count=len(records),
        wrap_segment=len(curves) * 2,
        finish_segment=len(curves) * 2,
        initial_adjustment=0,
        prior_laps_required=laps - 1,
        lap_period=period,
        lap_threshold=lap_threshold,
        finish_threshold=finish_threshold,
        finish_parameter=finish_parameter,
        start=pose(first, start_parameter),
        finish=pose(first, finish_parameter),
    )
    return blob, metadata
