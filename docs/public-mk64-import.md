# Clean-install MK64 pack import

Introduced in Candidate36, the native import action remains on the ordinary **MK64 Race Pack** entry
in **Settings → Mods**. A missing pack displays **Import MK64 ROM**; an installed
pack displays **Reimport MK64 ROM**. The normal enable/disable toggle remains.
The player selects the supported Road Rash 64 ROM in the launcher first, then
selects their Mario Kart 64 USA ROM using the import button's file picker.

The bundled converter runs in the background with progress and cancellation.
It includes its Python runtime and native conversion helpers, so players do not
install development tools. It accepts supported z64/v64/n64 byte orders and
checks the whole normalized ROM before reading its course data. The generated
mod is cached locally; there is no repeated ROM selection on ordinary launches.

1.4.3 bundles converter `1.0.1-c40` for both platforms. Existing c40 packs
remain compatible; reimport older packs with the bundled converter.

Initial Candidate36 verification (historical): native UI integration and Windows ON/OFF builds
pass. A full ROM-only conversion passed 2,043 comparisons and the native loader.
The frozen Windows converter then ran through the actual native import worker
from an unrelated Unicode directory with only its bundled runtime available.
It installed all 16 courses; all 862 files, including the catalogue, matched the
source conversion exactly. This took about 350 seconds on the development PC.
Cancellation, malformed output, process cleanup and previous-pack preservation
have separate offline checks. No visual import test has been launched. Linux's
process supervisor is checked, but full Linux conversion parity is not yet
verified. Do not infer visual or online acceptance from these offline checks.

For a public asset-free build, the player must choose their own supported Mario
Kart64 ROM in a one-time import flow. Validate its revision and byte order before
conversion. Generate the current corrected course meshes, contacts, routes,
previews, animated scenery, item data and optional audio locally, then validate
the resulting catalogue/assets and install the MK64 Race Pack Mods entry.
Keep the player's Road Rash64 ROM requirement separate.

The shipped converter must reproduce all reviewed corrections and gameplay
adaptations from donor input plus asset-free code/metadata. Requiring loose
private analysis folders or distributing pre-extracted replacement terrain,
art or music would not satisfy this clean-install workflow. Treat successful
reproduction from a clean workspace as a release gate, including platform
dependencies, cancellation, invalid ROM, failed/partial import, cache/version
updates and preservation of an existing valid installation.

The worker validates the complete generated catalogue, file membership, sizes
and hashes before installation. It stages beside the destination, retains the
previous pack as a recovery copy, and rolls back if the final rename fails.
Failed or cancelled conversion leaves the active pack intact. This is not a
claim of atomic recovery from power loss during the directory rename sequence.
Game initialization, active online sessions and pack changes cannot overlap
installation. Completing a successful import enables the normal Mods entry.

Each online peer supplies and converts their own ROM. The existing handshake
compares the installed/enabled pack identity and must reject a missing, disabled
or different pack with a clear message before racing. Do not transfer donor
assets from host to guests. Online play remains experimental and requires
matching game builds and generated pack identities.

Build tooling: `scripts/build_mk64_importer.py` freezes the converter and copies
the separately compiled contact/motion helpers. Only converter code, numeric
address/layout rules and runtime dependencies belong in a public package.
Generated tracks, previews, actor data and audio stay outside the shipped ZIP.
