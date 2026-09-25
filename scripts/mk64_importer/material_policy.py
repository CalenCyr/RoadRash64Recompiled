"""Keep the established water animation phase in the static course mesh.

The cartridge initializes these scrolling tile origins to zero. The accepted
course conversion sampled a later frame of the same animation. Retaining that
phase is a compatibility policy; it does not supply any texture or mesh data.
"""

import copy
from .common import require

WATER_PHASES = {
    "dks_jungle_parkway": {
        "gDKJTexture648508": 215,
        "gDKJTexture65FB18": 212,
        "gDKJTextureWaves0": 212,
    },
    "koopa_troopa_beach": {"gKTBTextureWaves1": 108, "gKTBTextureWaves2": 132},
    "royal_raceway": {"gRRWTexture648508": 235},
}


def apply(source):
    result = copy.deepcopy(source)
    remaining = dict(WATER_PHASES.get(source["course"], {}))
    for material in result["materials"]:
        symbol = material["texture"]
        if symbol in remaining:
            phase = remaining.pop(symbol)
            require(material["tile_size"][2] == "0", "Unexpected initial water texture phase")
            material["tile_size"][2] = f"0x{phase:04X}"
    require(not remaining, "Water animation material is missing")
    return result
