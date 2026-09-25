"""Extract the original sixteen rigid hazard models directly from ROM."""

import struct
from .actor_base import ModelSource
from . import source_walk as c


def build(donor):
    sources = {}
    models = []

    def model(slug, name, roots, scale=1, face=None, offsets=None):
        source = sources.setdefault(slug, ModelSource(donor, slug))
        groups = source.export(roots, face, offsets)
        verts = [v for g in groups for v in g["vertices"]]
        low = [min(v[i] for v in verts) for i in range(3)]
        high = [max(v[i] for v in verts) for i in range(3)]
        return dict(
            id=len(models),
            name=name,
            source_course=slug,
            source_roots=roots,
            source_scale=scale,
            groups=groups,
            minimum=low,
            maximum=high,
        )

    for face in range(6):
        models.append(
            model(
                "bowsers_castle",
                f"thwomp_face_{face}",
                ["d_course_bowsers_castle_dl_thwomp"],
                face=face,
            )
        )
    models.append(
        model("choco_mountain", "falling_rock", ["d_course_choco_mountain_dl_falling_rock"])
    )
    for i, root in enumerate(["23858", "238A0", "238E8"]):
        models.append(
            model(
                "toads_turnpike",
                f"box_truck_{i}",
                [f"d_course_toads_turnpike_dl_{root}", "toads_turnpike_dl_0"],
            )
        )
    for name, root, scale in [
        ("school_bus", "toads_turnpike_dl_3", 1),
        ("tanker_truck", "toads_turnpike_dl_6", 1),
        ("car", "toads_turnpike_dl_9", 0.1),
    ]:
        models.append(model("toads_turnpike", name, [root], scale))
    for name, roots, wheels in [
        (
            "train_engine",
            ["1C0F0", "1B978"],
            [(32, 6, "22DB8"), (16, 6, "22DB8"), (-12, 12, "22D70"), (-34, 12, "22D70")],
        ),
        ("train_tender", ["1F228"], [(8, 6, "22DB8"), (-8, 6, "22DB8")]),
        (
            "train_passenger",
            ["20A20", "20A08"],
            [(28, 6, "22DB8"), (12, 6, "22DB8"), (-8, 6, "22DB8"), (-24, 6, "22DB8")],
        ),
    ]:
        full = [f"d_course_kalimari_desert_dl_{x}" for x in roots] + [
            "d_course_kalimari_desert_dl_22D28"
        ]
        offsets = [None] * len(full)
        for z, y, root in wheels:
            for x in [17, -17]:
                full += [f"d_course_kalimari_desert_dl_{root}"]
                offsets.append([x, y, z])
        models.append(model("kalimari_desert", name, full, offsets=offsets))

    binary = bytearray(b"MKHZ0001" + struct.pack(">I", len(models)))
    report = []
    for m in models:
        rec = bytearray(
            struct.pack(
                ">IfI6f", m["id"], m["source_scale"], len(m["groups"]), *m["minimum"], *m["maximum"]
            )
        )
        groups = []
        for g in m["groups"]:
            tex = g["texture"]
            w, h = (tex["width"], tex["height"]) if tex else (0, 0)
            pixels = tex["raw"] if tex else b""
            vtx = b"".join(struct.pack(">hhhHhh4B", *v) for v in g["vertices"])
            c.require(w * h <= 2048 and len(g["vertices"]) % 3 == 0, "Material bounds")
            rec += struct.pack(">IIII", g["flags"], w, h, len(g["vertices"]) // 3) + vtx + pixels
            groups.append(
                {
                    "flags": g["flags"],
                    "triangles": len(g["vertices"]) // 3,
                    "texture": tex["source_asset"] if tex else None,
                    "width": w,
                    "height": h,
                    "source_lists": g["source_lists"],
                    "source_lighting_baked": g["source_lighting"],
                }
            )
        binary += struct.pack(">I", len(rec)) + rec
        scale = 0.05 * m["source_scale"]
        low = m["minimum"]
        high = m["maximum"]
        center = [(low[i] + high[i]) * 0.5 * scale for i in range(3)]
        extent = [(high[i] - low[i]) * 0.5 * scale for i in range(3)]
        report.append(
            {k: v for k, v in m.items() if k != "groups"}
            | {
                "groups": groups,
                "triangles": sum(g["triangles"] for g in groups),
                "collision_model_bounds": {
                    "offset": [center[0], -center[2], center[1]],
                    "half_extent": [extent[0], extent[2], extent[1]],
                    "space": "RR rider-local before source yaw; render origin same as source actor",
                },
                "limitations": (
                    ["Source dynamic lighting replaced by source-light color bake."]
                    if any(g["source_lighting_baked"] for g in groups)
                    else []
                ),
            }
        )
    return bytes(binary), report
