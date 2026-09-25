"""Finite ramp momentum and wood rolling audio derived from source contacts."""

import collections, math
from . import COURSE_SCALE, SOURCE_REFERENCE_COMMIT
from .actor_rigid import source_floor


def boosts(source, surfaces):
    slug = source["course"]
    if slug not in ("dks_jungle_parkway", "royal_raceway", "koopa_troopa_beach"):
        return None
    scale = COURSE_SCALE
    groups = collections.defaultdict(list)
    for t in surfaces["triangles"]:
        surface = str(t["source"]["surface"])
        if surface not in (
            {"RAMP"}
            if slug == "koopa_troopa_beach"
            else {"BOOST_RAMP_WOOD", "254", "BOOST_RAMP_ASPHALT"}
        ):
            continue
        v = t["vertices"]
        a, b, c = v
        den = (b[0] - a[0]) * (c[1] - a[1]) - (b[1] - a[1]) * (c[0] - a[0])
        if abs(den) < 1e-6:
            continue
        gx = ((b[2] - a[2]) * (c[1] - a[1]) - (c[2] - a[2]) * (b[1] - a[1])) / den
        gy = ((b[0] - a[0]) * (c[2] - a[2]) - (c[0] - a[0]) * (b[2] - a[2])) / den
        if math.hypot(gx, gy) < 0.02:
            continue
        groups[t["source"]["display_list"]].append((t, gx, gy))
    pads = []
    for name, tri in sorted(groups.items()):
        # Highest actual facet supplies takeoff slope. Native contact handles all
        # preceding ramp pieces; vertical side faces never become trigger surfaces.
        top = max(tri, key=lambda t: max(v[2] for v in t[0]["vertices"]))
        slope = math.hypot(top[1], top[2])
        direction = [top[1] / slope, top[2] / slope, 0]
        vertices = list({tuple(v) for t, _, _ in tri for v in t["vertices"]})
        high = max(v[2] for v in vertices)
        edge = [v for v in vertices if abs(v[2] - high) < 0.001]
        assert len(edge) >= 2
        lip = [sum(v[k] for v in edge) / len(edge) for k in range(3)]
        length = max(sum((lip[k] - v[k]) * direction[k] for k in range(2)) for v in vertices)
        pad = {
            "id": len(pads),
            "triangles": [t["vertices"] for t, _, _ in tri],
            "direction": direction,
            "lip": lip,
            "slope": slope,
            "length": length,
            "minimum_speed": 35.0,
            "source_display_list": name,
            "source_surface": top[0]["source"]["surface"],
            "native_gravity": 20,
            "derivation": "owner-requested Koopa ramp minimum speed; original uphill geometry",
        }
        if slug != "koopa_troopa_beach":
            landing_index = 250 if slug == "dks_jungle_parkway" else 670
            # Derive exact coordinate transform from one matching source/final vertex.
            first = top[0]
            si = first["source"]["source_vertices"][0]
            old = source["vertices"][si]["position"]
            new = first["vertices"][0]
            translation = [
                new[0] - old[0] * scale,
                new[1] + old[2] * scale,
                new[2] - old[1] * scale,
            ]
            lp = source["path"][landing_index]["position"][:]
            # Published source collision audit established these authored landing floors.
            lp[1] = source_floor(source, lp[0], lp[2])[0]
            landing = [
                lp[0] * scale + translation[0],
                -lp[2] * scale + translation[1],
                lp[1] * scale + translation[2],
            ]
            distance = math.hypot(landing[0] - lip[0], landing[1] - lip[1])
            if slug == "royal_raceway" and top[0]["source"]["section"] == 20:
                pad["minimum_speed"] = 50.0
                pad["derivation"] = (
                    "donor preliminary asphalt boost before final ramp; native slope, no relocation"
                )
            else:
                speed = math.sqrt(20 * distance**2 / (2 * (slope * distance + lip[2] - landing[2])))
                pad["minimum_speed"] = speed * 1.04
                pad.update(
                    landing=landing,
                    landing_source_path_index=landing_index,
                    range=distance,
                    flight_derivation="native g=20 ballistic range with source lip/landing and 4 percent clearance",
                )
            pad["derivation"] = "source-authored wood/asphalt boost translated to native momentum"
        pads.append(pad)
    assert len(pads) == (
        {"dks_jungle_parkway": 1, "royal_raceway": 2, "koopa_troopa_beach": 8}[slug]
    )
    output = {
        "format": "rr64-course-boosts",
        "version": 1,
        "course": slug,
        "source_reference_commit": SOURCE_REFERENCE_COMMIT,
        "pads": pads,
    }
    return output


def wood_surface_ids(source, surfaces):
    allowed = {"BOOST_RAMP_WOOD", "ROPE_BRIDGE", "WOOD_BRIDGE"}
    if source["course"] == "banshee_boardwalk":
        allowed.add("BRIDGE")
    return sorted(
        t["id"]
        for t in surfaces["triangles"]
        if str(t.get("source", {}).get("surface", "")) in allowed
    )
