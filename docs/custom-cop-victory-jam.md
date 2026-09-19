# Custom Cop victory and spoke jam — September 19

User report: split-screen Custom Cop did not end after all riders were busted;
cop wheel jam was unavailable. Scope includes every mode offering Custom Cop,
local and online. No game launch until the next explicit ready.

The initial opponent count subtracted human cops only, but the victory scan
skipped every category-7 actor, including native AI police. Count non-cop actors
at role initialization instead. The required human cop rule and host-only outcome
ownership remain unchanged. Existing yellow COPS WIN! and three-second result
delay are retained. This fixes a proven roster mismatch; the user's specific live
roster was not captured, so live acceptance remains pending.

The cop input adapter consumed C-Down from native C-Down+C-Right spoke jam as a
weapon trick. Preserve the full chord for cops and racers; only standalone
C-Down maps to the custom trick. The dedicated shortcut previously ran only for
profile zero. It now tracks edges and selected fists/weapon separately for four
local slots. Native jam timing, proximity, weapon eligibility and hit rules remain.
Online consumes the same guest input adapter and host outcome implementation;
clients retain replicated victory presentation rather than deciding race results.

Production build passed. RR64LocalRaceOptionsSmoke passed with added native-memory
cases for an AI cop alongside the final busted racer, three-second victory delay,
and preserved spoke-jam held/pressed bits without triggering trick. HUD widget
checks: 205 passed. No local race or two-machine online verification performed.

Prepared build identity is recorded in
analysis/release-1.2-online/sky-review-20260919/pending-custom-cop-victory-jam/build.json.
Preserved prior HUD/pickup executable. No renderer or HUD changes in this fix.

## Optional AI police

Custom Cop now exposes AI Cops: Off/On in the original race-options list. The
row is hidden/skipped when Custom Cop is off. Bit 10 stores the choice; settings
version 4 preserves versions 1–3 with that bit initially clear. Online host setup
already transmits the packed choices; validation accepts the additional bit.
Protocol 36 rejects older peers rather than letting them use different AI rules.

With Off, native 516B8's unreserved police-category count is transferred to a
regular racer category at 51954, after human reservations and before AI profile
selection. This preserves total slots and selects ordinary bike/rider/physics
profiles instead of hiding cop meshes or changing live actors. On permits the
original allocation; it does not increase the actor cap or force extra cops when
the stock allocation has none. Player cop selection is unchanged.

Native-memory tests cover Off conversion, On preservation, setup bit validation
and transfer, plus the earlier victory/jam checks. Production build and 205 HUD
checks pass. Generated hook position inspected. Live menu/local/online acceptance
still pending; no game launched. The prepared executable and hash are recorded in
pending-custom-cop-ai-option/build.json in the same private test folder.
