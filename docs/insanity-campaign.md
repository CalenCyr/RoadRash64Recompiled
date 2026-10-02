# Insanity chapter for Big Game

## Authorized behavior

On September 27 the owner explicitly requested a Big Game bonus chapter using
the existing eight Insanity tracks. The latest clarification preserves the
original ending after Level 5: these are optional bonus races to keep playing
afterward, with no replacement or second mandatory ending. This new extension
supersedes the guard-only proposal and the brief proposal to move the ending.
Implemented for 1.4.2. A September 29 player report exposed a save-cache
regression missed by the original offline tests; see the correction below.
Full live bonus-campaign completion and corrected save/load acceptance remain
pending.

The transition must retain ordinary Levels 1–5 and allow completed Level 5 saves
to continue. The Insanity bike purchase/Join flow must keep valid bike ownership
and money. The additional chapter needs real track selection, qualification,
save/load and repeatable completion; incrementing the old index alone is unsafe.
No new course or audio assets are distributed: all native tracks and resources
come from the user's Road Rash 64 ROM.

## Why the original transition fails

The reported 1.4.1 sequence is: finish Level 5, purchase either $60,000 Insanity
bike with $60,545, then choose Join. Original shop code can increment campaign
index four to five. Original campaign menu code indexes the five-entry track
pointer table with that value, obtains the adjacent integer six, then tries to
dereference it as a guest pointer. The recomp's completed-save return makes the
shop accessible after the original terminal point. The user's exception log has
not been supplied, so this is a traced matching failure path, not a stack match.

Original results code explicitly sends level index four to the ending. The
ending unlocks Insanity as race-selection tier six, without advancing the Big
Game profile to another campaign chapter. Native Thrash consumes a separate
Insanity route table. These distinct indices must not be treated interchangeably.

## External cross-check

- [Antseezee's track guide](https://gamefaqs.gamespot.com/n64/198491-road-rash-64/faqs/46566)
  lists eight Insanity courses. Its later FAQ admits uncertainty about unlocking
  the mode, so its suggested prerequisites are not treated as authoritative.
- [Drew Wilson's firsthand review](https://www.freezenet.ca/review-road-rash-64-n64/)
  describes Insanity as a Thrash unlock after completing the campaign as a Thrasher.
- [The speedrunning discussion](https://www.speedrun.com/rr64/forums/p9zj3)
  distinguishes Big Game and Insanity tracks and their shortcut handling.

The implementation must preserve those bonus tracks and earned unlocks while
adding their new Big Game chapter. Online anecdotes alone do not define the
save layout or justify bypassing native pointer and progression checks.

## Implemented campaign mapping

The original Level 5 results branch, ending, rewards and credits are unchanged.
Its existing completed-save return still highlights Save Game. A completed
profile can buy either native $60,000 Insanity model (25 or 26) and choose Join.
That path retains the purchase and remaining money, starts a fresh bonus
qualification word, changes the active chapter index from four to five, and
requests the original campaign-menu transition. Earlier chapters keep native
promotion. Joining while already in the bonus cannot advance beyond it.

The menu labels this chapter **Insanity Bonus**. Its eight course choices use
the actual ROM Insanity route table at `800A739C` and tier six. Individual
campaign table reads borrow the final native chapter's eight descriptors and
track count. After the original descriptor copy, the helper supplies the
Insanity route, tier, race type and rider pool. Native previews receive that
same route. The original ROM tables remain unchanged.

The native Insanity rider pool lacks a family requested by some Level 5
population templates. The existing bounded donor fallback also runs during
offline bonus campaign construction, after native human reservation. It moves
only unsupported requests to available families and preserves the racer count;
ordinary local multiplayer and online ownership conditions are unchanged.

Race payouts, combat-bonus multipliers, arrest/repair charges and population
settings retain Level 5's behavior. The current and next bike-shop views use
the native Insanity list. The bonus results screen uses the ordinary return
path regardless of how many bonus courses are qualified, so there is no second
ending or further promotion. All eight courses remain available for replay.
Passwords still describe the original campaign and are bounded to its final
entry; Pak saves carry the new bonus progress.

Bonus qualification storage does not reuse profile `+54`: that location is
part of the original gang-reputation array. The original Pak file already has
256 bytes, while its profile payload occupies 248. A version-bound integrity
tag and all eight native four-bit qualification counters fit in the remaining
eight bytes. The serialized base profile retains chapter four and a valid
native checksum, allowing an older build to load the original completed
campaign safely. This candidate restores chapter five and the bonus counters
only for an accepted, matching extension. Original qualifications, inventory,
name and statistics stay in their existing fields.
Saving that profile again in an older build clears the unrecognized bonus
extension; the original completed campaign remains usable.

Profile `+20` is the original gang category assigned by Join, rather than the
difficulty setting. Native ending rewards continue to use that category.

## September 29 save-cache and label correction

The 1.4.2 Pak writer stores the complete 256-byte file, including the bonus
extension. However, the extension cache was cleared at entry to native scan
`800207DC`. That routine returns immediately when the native profile cache is
valid. Repeated menu scans and selected load `80020ECC` therefore erased the
bonus identity without rereading its extension. The native base profile then
loaded as Level 5, retaining the purchased bike and remaining money.

Resetting extension metadata now occurs at `80020858`, after the native
cache/error early returns, only when a real scan begins. The native new-profile
initializer also leaves the Pak cache valid; its hook now resets only live
bonus results, preserving the matching saved-slot extensions. Genuine rescans
still invalidate removed/replaced records, and extensions remain bound to the
exact native profile and their integrity tag.

All three save-slot label formatters (load, save and overwrite) display Level 6
for a validated bonus slot. Only the formatting argument changes. Shared slot
metadata remains at the compatible base chapter because other consumers use
it for native race-tier unlocking; changing that shared value would grant an
unrelated tier.

The previous save fixture replaced the entire native scan with a stub that
always reread files, hiding its cache-hit path. The expanded fixture executes
the complete native scan and error/invalidation routines against a simulated
Pak device. It covers first scan, repeated menu scans, selected load, New Game
followed by cached load, six independent slots, ordinary saves, corrupt tails,
and failed I/O. The separate label fixture executes the original metadata and
all three formatter/unlock instruction sequences, checking that native unlocks
and profile bytes remain unchanged.

Keep affected saves and backups. An untouched 1.4.2 bonus save retains its
extension and should load correctly with this fix. If the player subsequently
overwrote the save after it fell back to Level 5, the ordinary save path cleared
that extension; its bonus race counters cannot be reconstructed from that file
alone. Do not recommend deleting an older save, automatically promote every
completed Level 5 profile, or promise free rejoining through the purchase UI.
The reporter's actual save file has not been inspected.

## Verification boundary

Use the actual native purchase, Join, menu initialization, route consumers,
qualification and saved-profile routines in offline regressions. Preserve old
saves, earlier successful test artifacts and unrelated mode behavior. Record
the final source/build identities and limitations alongside the new rival-engine
candidate. A fresh **ready** is required for a game test.
