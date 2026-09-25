# 1.4.0 source and distribution cleanup

The accepted RC6 behavior is preserved. Final changes consolidate repeated
item-HUD atlas checks, name sprite allocation constants, clarify ownership
comments, remove routine item debug messages and their unused counter/include,
and update the release version and documentation. Existing native reward and
HUD fixtures and the release smoke suites passed after this cleanup.

The earlier RC1 source organization and RC2–RC6 corrections remain included.
Current source keeps maintained diagnostic tools and optional runtime capture;
those are supported contributor features, not unreferenced experimental code.
Normal player capture remains opt-in. Private logs, test captures, historical
candidate outputs, the unused sky assets and old launcher art are excluded
from the source/player exports. Historical workspace evidence is preserved.

The optional texture pack is included in both platform downloads, not the source
archive. All 52 existing achievement badges and their UI are unchanged at the
owner's explicit request. Existing branded launcher artwork also remains.
These retained images and translated code must not be described as containing
no game-derived material. Neither ROM, extracted gameplay/course payloads nor
locally imported MK64 files is distributed. Both games' required data are read
or generated from the user's own supported ROMs.

Pinned dependency patches and byte-locked overrides are exported with the
source. Git index/object bytes and the uploaded source archive are checked,
including Windows line-ending normalization. The converter and imported pack
format are unchanged from RC4–RC6. New license notices are shipped beside the
existing importer notices without rebuilding its tested executable.

The owner authorized publication after candidate testing. Final cleanup and
version labeling produce a new executable identity; no new game launch was
performed. Offline checks do not establish Internet smoothness, every imported
course's correctness or acceptance of a new Linux binary. Online and imported
courses remain experimental.
