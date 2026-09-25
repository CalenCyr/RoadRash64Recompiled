# Course motion converter

This build-time helper evaluates the original MK64 path and spline arithmetic
from `n64decomp/mk64` commit `58cfcb022e10f83bc3b889d7e97508cae6837098`.
It contains no cartridge controls, spawns, geometry, images, or animation data.
The importer supplies controls decoded from the user's authenticated ROM.

The protocol is whitespace-delimited on standard input. `2d N 0` accepts N
four-short path records and omits the final control, matching native vehicle
path generation. `boundary N width` accepts N path records and emits six
shorts per row (left XYZ, right XYZ). `spline controlCount rowCount` accepts
rowCount XYZ-duration records and emits offset XYZ and derivative XYZ for each
original object update. Invalid sizes/ranges fail with a nonzero exit status;
path generation and spline sampling have explicit work/output limits.

Preserve strict floating-point evaluation when building. The Windows portable
conversion was verified against all six accepted actor splines, the ferry and
train paths, and both road-boundary paths. This is equivalence to the accepted
native conversion, not a claim of last-bit N64 transcendental emulation.
