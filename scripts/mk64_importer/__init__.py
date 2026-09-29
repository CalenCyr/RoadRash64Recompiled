"""Asset-free, offline MK64 race-pack conversion from user-provided ROMs.

The package contains conversion algorithms and address/selection recipes only.
All game vertices, images, models, animation samples and music are decoded from
the supplied ROM into an explicit staging directory. It never installs a pack.
"""

CONVERTER_VERSION = "1.0.1-c40"
SOURCE_REFERENCE_COMMIT = "58cfcb022e10f83bc3b889d7e97508cae6837098"
COURSE_SCALE = 0.234375
