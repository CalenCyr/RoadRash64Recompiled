# Koopa Beach and Luigi tunnel terrain ownership

RC1 owner testing exposed overlapping Koopa beach/cliff terrain, its sealed
shortcut, and a floating Luigi brick tunnel. The tested executable and locally
imported pack matched RC1 exactly; import completed successfully. The run had
normal release diagnostics disabled, so this evidence does not locate every
reported collision or flicker frame.

Both source courses use camera-dependent near/far meshes. Importing their union
can retain a distant simplified shell inside the detailed terrain. Some are
curved or contain tunnel openings, so exact coplanar subtraction does not remove
the entire alternative. The distant Koopa cliffs cover shortcut openings; the
straightened Luigi tunnel crosses its detailed curved shell.

## Implementation

`scripts/race_pack_koopa_beach.py` selects authenticated detailed terrain using
the adjacent metadata recipe. `scripts/race_pack_luigi_raceway.py` verifies the
tunnel's material, topology, endpoint correspondence and camera ownership.
All geometry is decoded from the supplied ROM. Recipes contain identities and
validation hashes, not replacement mesh or texture data.

The shared `mk64_importer/terrain.py` calls these corrections after its existing
exact camera cleanup and before final encoding/contact promotion. Retain the
original physical mesh, native first-hit ordering and finite contact associated
with the corrected visible terrain. Never reuse exploration contacts promoted
from a discarded distant shell. Do not fix these defects with depth bias,
invisible passage walls or an infinite floor.

Luigi's ten coarse faces are replaced by the existing sixty-face detailed
tunnel. Its original twenty-four physical wall triangles remain. Floor probes
and production wall/surface sweeps cover the old phantom shell and retained
detailed contact. Koopa includes both shortcut openings and surrounding
authenticated terrain alternatives. The final native pack is checked separately
from the intermediate source mesh.

## Pack lifecycle and scope

Converter revision `1.0.1-c39` regenerates the full pack. It retains the earlier
Koopa shortcut and Luigi tunnel repairs. RC4 additionally clips distant beach
fragments using 118 authenticated neighbouring-terrain relationships, whose
shared physical edges connect them to the primary near terrain. Cross-material
beach/shore/cliff joins are covered, while uncovered fragments are retained.

The original Koopa inland cliff has an open top: a closed 72-edge rim, separate
from the 49-edge sea boundary. RC4 derives a finite visible sand surface from
that inland rim for off-road exploration. Its concave outline and existing
heights are preserved. Triangulation and subdivision avoid steep slivers; the
maximum added slope is approximately 35 degrees. The 290 resulting triangles
use ROM-derived beach material, texture density and colour. The public recipe
contains identifiers, validation hashes and construction rules, not donor mesh
coordinates or texture data. The outer sea boundary remains open and the
shortcut remains below the new top with its original entrance, exit and walls.
The original rim edges stay unsplit to avoid quantization gaps against the
existing cliff; subdivision applies only inside the new surface.

New contact is promoted from the final encoded visible faces, including the
inland top. Original physical triangle contents and first-hit order remain
unchanged. Cell culling bounds retain both the original terrain and the added
surface. This is a finite extension of the imported terrain, not a world floor
or an invisible barrier; falling beyond the actual sea edge is still possible.

The RC4 full-pack comparison checks the other fifteen courses, texture/audio/
actor assets and all native route binaries against RC2/RC3. Original gameplay
physics and stock Road Rash courses are outside this repair. Existing authored
steep cliff sides, objects and stacked tunnel geometry remain separate surfaces.

The pack is latched when Play starts. Reimport through Mods in a fresh process
before Play; existing installed files do not update merely because the game
executable changed. Catalogue digest admission requires matching corrected packs
online without changing network protocol56 or the catalogue schema.

Offline data/contact checks do not establish that every surface is correct in
live play. RC4 still requires owner testing of the reported beach overlap,
off-road top and shortcut. Detailed private evidence is preserved under the
project's `analysis/course-followup-20260924-c37` and
`analysis/highlight-audio-20260925-c39` folders and is not included in downloads.
