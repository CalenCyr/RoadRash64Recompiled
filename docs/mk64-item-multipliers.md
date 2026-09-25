# MK64 question-box attack multipliers

RC6 adds the existing Road Rash ×2 and ×4 attack power-ups to the imported
course item roulette. Each available weapon and each multiplier has one entry
in the random pool. Full weapon inventory leaves the two multipliers available.
The independent item RNG does not change native AI or traffic randomness.

The host (or offline game) chooses the reward once on box contact and applies
it once after the 48-tick roulette. Native `func_80037960` receives pickup type
1 or 2, preserving the equipped weapon and inventory. It sets the original
25-second timer and replaces any previous multiplier, including ×4 to ×2.
Expiry and combat calculations continue through the original game code.
Crashing during the roulette retains the collected reward; retiring or
replacing the recipient cancels it. Guests and prediction never grant rewards.

Reward IDs 15/16 are separate from native weapon indices. The roulette keeps
a safe weapon ID for native inventory reads, then substitutes only the three
audited weapon-icon sprite arguments. Multiplier icons are native sprites
191/192. Their original 64×64 artwork is fitted to the native 24×24 weapon
footprint by reading atlas dimensions and adjusting only the newly allocated
sprite record; failed allocations cannot resize a previous player's icon.
The selected reward blinks briefly; the original persistent effect
indicator continues independently. Three/four-player layouts use the existing
bounded per-viewport item indication. Stock-course HUDs remain unchanged.

The existing complete host tick already carries the multiplier state and timer
in rider dynamics. The item packet layout remains unchanged, but protocol 57
is required because older validators reject the new reward IDs and cannot
assemble those ticks. All peers must use a compatible build. No map conversion
is needed; RC4/RC5 packs and converter `1.0.1-c39` remain current.

Verification uses the actual native grant/timer code, the production item
collector, bounded HUD fixtures, and in-process packet assembly/correction.
These checks do not establish visual, audible or Internet-play acceptance.
Private evidence is retained under `analysis/item-multipliers-20260925-c41`;
no fixture data, ROM, generated maps or private logs belong in a public ZIP.
