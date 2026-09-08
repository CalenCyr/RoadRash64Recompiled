# Weapon caller coverage candidate

The first scale candidate FAILED the user's visual test. This candidate also
scopes the rider weapon branch in func_80010120 at 0x8001039C, with cleanup at
0x800103A4. That shared call site also draws other graph types; node type2 and
exact rider+0x5BC graph ownership are required before the correction is enabled.
Existing scope in func_80011CC0 remains. No unrelated generic graph caller is
admitted. Both paths use func_8000F9E8; no new generic matrix scaling is added.

Opt-in RR64_WEAPON_DIAGNOSTICS counters report calls, accepted scopes, source
hook visits, successful corrections, alternate calls, and source conversions via
the periodic logger. This resolves the first test's lack of evidence that the
correction actually ran. Counter updates are disabled outside diagnostic runs.

Original hook generation, native build, and both caller scope/scale/context tests
pass with diagnostics enabled and disabled. Visual success is still unverified.
If artifact persists, use actual coverage before proposing another scale change.
Rolling terrain remains reverted. No launch or publication.
