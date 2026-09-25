# Portable terrain conversion

`scripts/mk64_importer/terrain.py` converts a neutral course extracted from the
user's verified Mario Kart 64 ROM. It has no dependency on the private analysis
folders or their asset payloads. Its JSON recipes contain source identities and
conversion choices, not vertices, texture pixels, or display-list commands.

`build_course(source, roots, display_lists, texture_dir, registry, placement)`
returns geometry, native cells, wall metadata, surface metadata and an audit.
The pack assembler owns ROM validation, stable texture registration, routes,
actors, sound, final texture-state fixes and race-specific ramp/boost metadata.
`encode_source` is also available for the historical native route-probe stages;
it does not implicitly filter or correct its supplied source.

The preparation order is intentional:

1. Apply the common baseline: Mario's detailed camera alternatives, exact
   coplanar recovery and the donor's gameplay vertex initialization.
2. Reproduce accepted course-specific source corrections. Moo's projection
   starts from the original camera union; Frappe has two successive near-ground
   passes. DK, Royal and Yoshi use their separately authenticated recipes.
3. Build authored contact metadata using original collision indices, classify
   finite panels, and apply the donor collision filter. Lower only the proven
   Rainbow, Banshee and Yoshi rails, including Banshee's attached caps.
4. Encode final geometry and preserve the native first-hit contact order.
   Wario's remaining rounding seam is derived from converted triangle IDs;
   its retained detailed surface must cover the removed sub-unit wedge.
5. Add contact only for classified final visible triangles. Water, lava,
   decorative artwork and genuine empty gaps remain excluded. No invisible
   corridor, boundary wall or world-sized floor is introduced.

Frappe's earlier source-contact pass remains before the common encoded-contact
pass because four native bucket references depend on that ordering. Yoshi's
rail correction preserves every original nonvertical contact in order while
replacing the authenticated vertical panels.

The C36 comparison regenerates all 471 native cells exactly and checks all 16
courses' authored wall/surface records and finite rail records against the
preserved candidate. Generated exploration positions match; their fresh IDs and
provenance are deterministic and need not duplicate historical development
metadata. This comparison establishes conversion parity, not GPU or online
play acceptance. The separate ROM-extraction and complete-pack checks must also
pass before distributing the importer.
