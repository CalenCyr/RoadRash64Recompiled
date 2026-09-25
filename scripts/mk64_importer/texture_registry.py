"""Stable native texture identities without distributing texture data."""

import json
import struct
from pathlib import Path
from .common import require, digest
from .materials import material_variant


def create():
    """Reserve established IDs; texture bytes must still come from this ROM."""
    rows = json.loads(Path(__file__).with_name("texture_layout.json").read_text())["records"]
    return {
        r["sha256"]: {
            **r,
            "blob": None,
            "final_features": r["features"],
            "features": r["features"] & 255,
        }
        for r in rows
    }


def discover(source, directory, records):
    for material in source["materials"]:
        material_variant(source, material, directory, records)


def finalize(records):
    """Authenticate regenerated texels before applying the final depth flags."""
    expected = json.loads(Path(__file__).with_name("texture_layout.json").read_text())["records"]
    require(len(records) == len(expected), "Unexpected course texture variant")
    result = []
    for row in sorted(records.values(), key=lambda r: r["index"]):
        require(
            row["blob"] is not None,
            "Course texture variant was not regenerated: " + str(row["index"]),
        )
        require(digest(row["blob"]) == row["sha256"], "Regenerated texture identity mismatch")
        blob = bytearray(row["blob"])
        struct.pack_into(">I", blob, 0x38, 0x4D4B0000 | row["final_features"])
        result.append(
            (
                {k: row[k] for k in ("index", "format")} | {"features": row["final_features"]},
                bytes(blob),
            )
        )
    return result
